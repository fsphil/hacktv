/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2019 Philip Heron <phil@sanslogic.co.uk>                    */
/*                                                                       */
/* This program is free software: you can redistribute it and/or modify  */
/* it under the terms of the GNU General Public License as published by  */
/* the Free Software Foundation, either version 3 of the License, or     */
/* (at your option) any later version.                                   */
/*                                                                       */
/* This program is distributed in the hope that it will be useful,       */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of        */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         */
/* GNU General Public License for more details.                          */
/*                                                                       */
/* You should have received a copy of the GNU General Public License     */
/* along with this program.  If not, see <http://www.gnu.org/licenses/>. */

/* -=== ACP / Macrovision encoder ===- */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "video.h"

int acp_init(acp_t *s, vid_t *vid)
{
	double left;
	double spacing;
	double psync_width;
	int i;
	
	memset(s, 0, sizeof(acp_t));
	
	s->vid = vid;
	
	if(s->vid->conf.lines == 625)
	{
		left = 8.88e-6;
		spacing = 5.92e-6;
		psync_width = 2.368e-6;
	}
	else
	{
		left = 8.288e-6;
		spacing = 8.288e-6;
		psync_width = 2.222e-6;
	}
	
	/* Calculate the levels */
	s->psync_level = vid->sync_level + round((vid->white_level - vid->sync_level) * 0.06);
	s->pagc_level  = vid->sync_level + round((vid->white_level - vid->sync_level) * 1.10);
	
	/* Calculate the width of each pulse */
	s->psync_width = round(vid->sample_rate * psync_width);
	s->pagc_width  = round(vid->sample_rate * 2.7e-6);
	
	/* Left position of each pulse */
	for(i = 0; i < 6; i++)
	{
		s->left[i] = round(vid->sample_rate * (left + spacing * i));
	}
	
	return(VID_OK);
}

void acp_free(acp_t *s)
{
	if(s == NULL) return;
	
	memset(s, 0, sizeof(acp_t));
}

void acp_render_line(acp_t *s)
{
	int i, x;
	
	i = 0;
	
	if(s->vid->line == 1)
	{
		/* Vary the AGC pulse level, clipped sawtooth waveform */
		i = abs(s->vid->frame * 4 % 1712 - 856) - 150;
		
		if(i < 0) i = 0;
		else if(i > 255) i = 255;
		
		i = s->vid->y_level_lookup[i << 16 | i << 8 | i];
		
		s->pagc_level = s->vid->sync_level + round((i - s->vid->sync_level) * 1.10);
	}
	
	i = 0;
	
	if(s->vid->conf.lines == 625)
	{
		/* For 625-line modes, ACP is rendered on lines 9-18 and 321-330 */
		if(s->vid->line >=   9 && s->vid->line <=  18) i = 1;
		if(s->vid->line >= 321 && s->vid->line <= 330) i = 1;
	}
	else
	{
		/* For 525-line modes, ACP is rendered on lines 12-19 and 275-282 */
		if(s->vid->line >=  12 && s->vid->line <=  19) i = 1;
		if(s->vid->line >= 275 && s->vid->line <= 282) i = 1;
	}
	
	if(i == 0) return;
	
	/* Render the P-Sync / AGC pulse pairs */
	for(i = 0; i < 6; i++)
	{
		/* Render the P-Sync pulse */
		for(x = s->left[i]; x < s->left[i] + s->psync_width; x++)
		{
			s->vid->output[x * 2] = s->psync_level;
		}
		
		/* Render the AGC pulse */
		for(; x < s->left[i] + s->psync_width + s->pagc_width; x++)
		{
			s->vid->output[x * 2] = s->pagc_level;
		}
	}
}

