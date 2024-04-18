/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2018 Philip Heron <phil@sanslogic.co.uk>                    */
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

/* -=== WSS encoder ===- */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "video.h"
#include "vbidata.h"

typedef struct {
	const char *id;
	uint8_t code;
	rational_t aspect[2];
} _wss_modes_t;

static const _wss_modes_t _wss_modes[] = {
	{ "4:3", 0x08, { { 4, 3 } } },
	{ "16:9", 0x07, { { 16, 9 } } },
	{ "14:9-letterbox", 0x01, { { 4, 3 } } },
	{ "16:9-letterbox", 0x04, { { 4, 3 } } },
	{ "auto", 0xFF, { { 4, 3 }, { 16, 9 } } },
	{ },
};

static size_t _group_bits(uint8_t *vbi, uint8_t code, size_t offset, size_t length)
{
	int i, b;
	
	while(length--)
	{
		for(i = 0; i < 6; i++, offset++)
		{
			if(i == 3) code ^= 1;
			
			b = 7 - (offset % 8);
			
			vbi[offset / 8] &= ~(1 << b);
			vbi[offset / 8] |= (code & 1) << b;
		}
		
		code >>= 1;
	}
	
	return(offset);
}

int wss_init(wss_t *s, vid_t *vid, char *mode)
{
	int level;
	size_t o;
	
	memset(s, 0, sizeof(wss_t));
	
	/* Find the mode settings */
	s->code = 0;
	for(o = 0; _wss_modes[o].id != NULL; o++)
	{
		if(strcasecmp(mode, _wss_modes[o].id) == 0)
		{
			s->code = _wss_modes[o].code;
			vid->conf.frame_aspects[0] = _wss_modes[o].aspect[0];
			vid->conf.frame_aspects[1] = _wss_modes[o].aspect[1];
			break;
		}
	}
	
	if(s->code == 0)
	{
		fprintf(stderr, "wss: Unrecognised mode '%s'.\n", mode);
		return(VID_ERROR);
	}
	
	/* Calculate the high level for the VBI data */
	level = round((vid->white_level - vid->black_level) * (5.0 / 7.0));
	
	s->vid = vid;
	s->lut = vbidata_init(
		137, s->vid->width,
		level,
		VBIDATA_FILTER_RC, (double) s->vid->width / 320, 0.7,
		s->vid->pixel_rate * 11e-6
	);
	
	if(!s->lut)
	{
		return(VID_OUT_OF_MEMORY);
	}
	
	/* Calculate the threshold pixel aspect ratio for auto mode */
	s->auto_threshold = rational_div(
		(rational_t) { 14, 9 },
		(rational_t) { s->vid->active_width, s->vid->conf.active_lines }
	);
	
	/* Prepare the VBI data. Start with the run-in and start code */
	s->vbi[0] = 0xF8; // 11111000
	s->vbi[1] = 0xE3; // 11100011
	s->vbi[2] = 0x8E; // 10001110
	s->vbi[3] = 0x38; // 00111000
	s->vbi[4] = 0xF1; // 11110001
	s->vbi[5] = 0xE0; // 11100000
	s->vbi[6] = 0xF8; // 11111___
	
	/* Group 1 (Aspect Ratio) */
	o = _group_bits(s->vbi, s->code, 29 + 24, 4);
	
	/* Group 2 (Enhanced Services) */
	o = _group_bits(s->vbi, 0x00, o, 4);
	
	/* Group 3 (Subtitles) */
	o = _group_bits(s->vbi, 0x00, o, 3);
	
	/* Group 4 (Reserved) */
	o = _group_bits(s->vbi, 0x00, o, 3);
	
	/* Calculate width of line to blank */
	s->blank_width = round(s->vid->pixel_rate * 42.5e-6);
	
	return(VID_OK);
}

void wss_free(wss_t *s)
{
	if(s == NULL) return;
	
	free(s->lut);
	
	memset(s, 0, sizeof(wss_t));
}

int wss_render(vid_t *s, void *arg, int nlines, vid_line_t **lines)
{
	wss_t *w = arg;
	vid_line_t *l = lines[0];
	int x;
	
	/* WSS is rendered on line 23 */
	if(l->line != 23)
	{
		return(1);
	}
	
	if(w->code == 0xFF)
	{
		int c;
		
		/* Auto mode selects between 4:3 and 16:9 depending
		 * on the the pixel aspect ratio of the source frame */
		
		c = rational_cmp(
			s->vframe.pixel_aspect_ratio,
			w->auto_threshold
		);
		
		_group_bits(w->vbi, c <= 0 ? 0x08 : 0x07, 29 + 24, 4);
	}
	
	/* 42.5μs of line 23 needs to be blanked otherwise the WSS bits may
	 * overlap active video */
	for(x = s->half_width; x < w->blank_width; x++)
	{
		l->output[x * 2] = s->black_level;
	}
	
	vbidata_render(w->lut, w->vbi, 0, 137, VBIDATA_MSB_FIRST, l);
	
	l->vbialloc = 1;
	
	return(1);
}

