/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2025 Philip Heron <phil@sanslogic.co.uk>                    */
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "video.h"
#include "vbidata.h"

int cc608_init(cc608_t *s, vid_t *vid)
{
	double offset;
	double x, w;
	double level;
	int i;
	
	memset(s, 0, sizeof(cc608_t));
	
	if(vid->conf.type == VID_RASTER_525)
	{
		s->lines[0] = 21;
		s->lines[1] = -1; // 284;
		offset = 27.382e-6;
	}
	else if(vid->conf.type == VID_RASTER_625)
	{
		s->lines[0] = 22;
		s->lines[1] = -1; //335;
		offset = 27.5e-6;
	}
	else
	{
		fprintf(stderr, "cc608: CEA/EIA-608 is not supported in this TV mode.\n");
		return(VID_ERROR);
	}
	
	/* Calculate the high level for the VBI data, 50% of the white level */
	s->lut = vbidata_init_step(
		32,
		vid->width,
		level = round((vid->white_level - vid->black_level) * 0.5),
		(double) vid->width / 32,
		vid->pixel_rate * 240e-9 * IRT1090,
		vid->pixel_rate * offset
	);
	if(!s->lut)
	{
		return(VID_OUT_OF_MEMORY);
	}
	
	/* Render the clock run-in */
	w = (double) vid->width * 7 / 32;
	x = (double) vid->pixel_rate * offset - (vid->width * 8.75 / 32);
	
	s->cri_x = x;
	s->cri_len = ceil(w);
	s->cri = malloc(sizeof(int16_t) * s->cri_len);
	if(!s->cri)
	{
		free(s->lut);
		return(VID_OUT_OF_MEMORY);
	}
	
	for(i = 0; i < s->cri_len; i++)
	{
		s->cri[i] = (0.5 - cos(((double) i - (x - s->cri_x)) * (2 * M_PI / w * 7)) * 0.5) * level;
	}
	
	s->msg = NULL;
	s->nmsg = (const uint8_t *)
		"\x14\x27" "\x14\x27" /* RU2 Roll-Up Captions-4 Rows */
		"\x14\x2D" "\x14\x2D" /* CR Carriage Return */
		"\x14\x70" "\x14\x70" /* row 15, indent 0, white (?) */
		"HELLO FROM LINE 21 & HACKTV!";
	
	return(VID_OK);
}

void cc608_free(cc608_t *s)
{
	free(s->lut);
	memset(s, 0, sizeof(cc608_t));
}

void _encode_chars(uint8_t *data, uint8_t c1, uint8_t c2)
{
	int i;
	
	fprintf(stderr, "tx: %02X %02X\n", c1, c2);
	
	c1 = (c1 & 0x7F) | 0x80;
	c2 = (c2 & 0x7F) | 0x80;
	
	for(i = 1; i < 8; i++)
	{
		c1 ^= (c1 << i) & 0x80;
		c2 ^= (c2 << i) & 0x80;
	}
	
	data[0] = (c1 << 1) | 0x01;
	data[1] = (c2 << 1) | (c1 >> 7);
	data[2] = (c2 >> 7);
}

int cc608_render(vid_t *s, void *arg, int nlines, vid_line_t **lines)
{
	cc608_t *v = arg;
	vid_line_t *l = lines[0];
	uint8_t data[3];
	int i;
	
	if(l->line != v->lines[0] &&
	   l->line != v->lines[1])
	{
		return(1);
	}
	
	if(v->msg == NULL || v->msg[0] == '\0')
	{
		v->msg = NULL;
		
		if(l->frame % 150 == 0)
		{
			v->msg = v->nmsg;
		}
	}
	
	if(v->msg == NULL || v->msg[0] == '\0')
	{
		_encode_chars(data, 0, 0);
	}
	else
	{
		_encode_chars(data, v->msg[0], v->msg[1]);
		v->msg += 2;
	}
	
	/* Render the line */
	for(i = 0; i < v->cri_len; i++)
	{
		l->output[(v->cri_x + i) * 2] += v->cri[i];
	}
	
	vbidata_render(v->lut, data, 0, 17, VBIDATA_LSB_FIRST, l);
	l->vbialloc = 1;
	
	return(1);
}

