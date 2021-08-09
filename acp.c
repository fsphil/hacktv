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
	
	if(vid->conf.lines == 625)
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
	s->psync_width = round(vid->pixel_rate * psync_width);
	s->pagc_width  = round(vid->pixel_rate * 2.7e-6);
	
	/* Left position of each pulse */
	for(i = 0; i < 6; i++)
	{
		s->left[i] = round(vid->pixel_rate * (left + spacing * i));
	}
	
	return(VID_OK);
}

void acp_free(acp_t *s)
{
	if(s == NULL) return;
	
	memset(s, 0, sizeof(acp_t));
}

int acp_render_line(vid_t *s, void *arg, int nlines, vid_line_t **lines)
{
	acp_t *a = arg;
	int i, x;
	vid_line_t *l = lines[0];
	
	i = 0;
	
	if(l->line == 1)
	{
		/* Vary the AGC pulse level, clipped sawtooth waveform */
		i = abs(l->frame * 4 % 1712 - 856) - 150;
		
		if(i < 0) i = 0;
		else if(i > 255) i = 255;
		
		i = s->yiq_level_lookup[i << 16 | i << 8 | i].y;
		
		a->pagc_level = s->sync_level + round((i - s->sync_level) * 1.10);
	}
	
	i = 0;
	
	if(s->conf.lines == 625)
	{
		/* For 625-line modes, ACP is rendered on lines 9-18 and 321-330 */
		if(l->line >=   9 && l->line <=  18) i = 1;
		if(l->line >= 321 && l->line <= 330) i = 1;
	}
	else
	{
		/* For 525-line modes, ACP is rendered on lines 12-19 and 275-282 */
		if(l->line >=  12 && l->line <=  19) i = 1;
		if(l->line >= 275 && l->line <= 282) i = 1;
	}
	
	if(i == 0 || l->vbialloc) return(1);
	
	/* Render the P-Sync / AGC pulse pairs */
	for(i = 0; i < 6; i++)
	{
		/* Render the P-Sync pulse */
		for(x = a->left[i]; x < a->left[i] + a->psync_width; x++)
		{
			l->output[x * 2] = a->psync_level;
		}
		
		/* Render the AGC pulse */
		for(; x < a->left[i] + a->psync_width + a->pagc_width; x++)
		{
			l->output[x * 2] = a->pagc_level;
		}
	}
	
	l->vbialloc = 1;
	
	return(1);
}

