/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2023 Philip Heron <phil@sanslogic.co.uk>                    */
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

static size_t _bits(uint8_t *data, size_t offset, uint64_t bits, size_t nbits)
{
	uint64_t m = 1;
	uint8_t b;
	
	for(; nbits; nbits--, offset++, bits >>= 1)
	{
		b = 1 << (offset & 7);
		if(bits & m) data[offset >> 3] |= b;
		else data[offset >> 3] &= ~b;
	}
	
	return(offset);
}

int vitc_init(vitc_t *s, vid_t *vid)
{
	int i;
	int hr;
	
	memset(s, 0, sizeof(vitc_t));
	
	s->type = vid->conf.type;
	
	if(s->type == VID_RASTER_625)
	{
		s->lines[0] = 19;
		s->lines[1] = 332;
		hr = 116;
		
		/* Fh x 116 is specified for 625-line VITC in "EBU Tech 3097
		 * EBU Time-And-Control Code For Television Tape-Recordings",
		 * but other specs use 115 for both 525 and 625-line modes.
		 * 
		 * The error margin is 115 ±2%, so 116 should be a safe value
		 * for all cases.
		*/
	}
	else if(s->type == VID_RASTER_525)
	{
		s->lines[0] = 14;
		s->lines[1] = 277;
		hr = 115;
	}
	else
	{
		fprintf(stderr, "vitc: Unsupported video mode\n");
		return(VID_ERROR);
	}
	
	if(vid->conf.frame_rate.num <= 30 &&
	   vid->conf.frame_rate.den == 1)
	{
		s->fps = vid->conf.frame_rate.num;
		s->frame_drop = 0;
	}
	else if(vid->conf.frame_rate.num == 30000 &&
	        vid->conf.frame_rate.den == 1001)
	{
		s->fps = 30;
		s->frame_drop = 1;
	}
	else
	{
		fprintf(stderr, "vitc: Unsupported frame rate %d/%d\n",
			vid->conf.frame_rate.num,
			vid->conf.frame_rate.den
		);
		
		return(VID_ERROR);
	}
	
	/* Calculate the high level for the VBI data, 78.5% of the white level */
	i = round((vid->white_level - vid->black_level) * 0.785);
	s->lut = vbidata_init_step(hr, vid->width, i, (double) vid->width / hr, vid->pixel_rate * 200e-9, 0);
	
	if(!s->lut)
	{
		return(VID_OUT_OF_MEMORY);
	}
	
	return(VID_OK);
}

void vitc_free(vitc_t *s)
{
	free(s->lut);
	memset(s, 0, sizeof(vitc_t));
}

int vitc_render(vid_t *s, void *arg, int nlines, vid_line_t **lines)
{
	vitc_t *v = arg;
	vid_line_t *l = lines[0];
	uint32_t timecode;
	uint32_t userdata;
	uint8_t data[12];
	int x, i;
	uint8_t crc;
	int fn;
	
	if(l->line != v->lines[0] && l->line != (v->lines[0] + 2) &&
	   l->line != v->lines[1] && l->line != (v->lines[1] + 2))
	{
		return(1);
	}
	
	fn = l->frame;
	
	if(v->frame_drop)
	{
		/* Frame drop, to compensate for 29.97 fps modes */
		fn += (fn / 17982) * 18;
		fn += (fn % 18000 - 2) / 1798 * 2;
	}
	
	/* Build the timecode data */
	timecode  = (fn % v->fps % 10) << 0; /* Frame number, units */
	timecode |= (fn % v->fps / 10) << 4; /* Frame number, tens */
	timecode |= (v->frame_drop ? 1 : 0) << 6; /* 1 == drop frame mode */
	timecode |= 0 << 7; /* 1 == colour framing */
	
	fn /= v->fps;
	timecode |= (fn % 10) << 8; /* Seconds, units */
	timecode |= (fn / 10 % 6) << 12; /* Seconds, tens */
	if(v->type != VID_RASTER_625)
	{
		timecode |= (l->line >= v->lines[1] ? 1 : 0) << 15; /* Field flag, 0: first/odd field, 1: second/even field */
	}
	
	fn /= 60;
	timecode |= (fn % 10) << 16; /* Minutes, units */
	timecode |= (fn / 10 % 6) << 20; /* Minutes, tens */
	
	fn /= 60;
	timecode |= (fn % 24 % 10) << 24; /* Hours, units */
	timecode |= (fn % 24 / 10) << 28; /* Hours, tens */
	if(v->type == VID_RASTER_625)
	{
		timecode |= (l->line >= v->lines[1] ? 1 : 0) << 31; /* Field flag, 0: first/odd field, 1: second/even field */
	}
	
	/* User bits, not used here */
	userdata = 0x00;
	
	/* Pack the data */
	for(x = i = 0; i < 8; i++)
	{
		x = _bits(data, x, 0x01, 2); /* Sync */
		x = _bits(data, x, timecode >> (i * 4), 4);
		x = _bits(data, x, userdata >> (i * 4), 4);
	}
	
	/* Calculate CRC */
	x = _bits(data, x, 0x01, 2); /* Sync */
	_bits(data, x, 0, 8);
	
	for(crc = i = 0; i < 11; i++)
	{
		crc ^= data[i];
	}
	
	/* Rotate the CRC and add to line */
	crc = ((crc << 6) | (crc >> 2)) & 0xFF;
	x = _bits(data, x, crc, 8);
	
	/* Render the line */
	vbidata_render(v->lut, data, 21, x, VBIDATA_LSB_FIRST, l);
	
	l->vbialloc = 1;
	
	return(1);
}

