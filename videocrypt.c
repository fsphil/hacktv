/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2017 Philip Heron <phil@sanslogic.co.uk>                    */
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

/* -=== Videocrypt encoder ===-
 * 
 * This is a Videocrypt I encoder. It scrambles the image using a technique
 * called "line cut-and-rotate", and inserts the necessary data into the
 * VBI area of the image to activate the Videocrypt hardware unscrambler.
 * 
 * There are no details of the PRNG published that I could find, so the
 * cut points used here where captured by analysing how the real hardware
 * attempts to unscramble an already clear image.
 * 
 * THANKS
 * 
 * Markus Kuhn and William Andrew Steer for their detailed descriptions
 * and examples of how Videocrypt works:
 * 
 * https://www.cl.cam.ac.uk/~mgk25/tv-crypt/
 * http://www.techmind.org/vdc/
 * 
 * Ralph Metzler for the details of how the VBI data is encoded:
 * 
 * http://src.gnu-darwin.org/ports/misc/vbidecode/work/bttv/apps/vbidecode/vbidecode.cc
 * 
 * Alex L. James for providing an active Sky subscriber card, VBI samples and testing.
 *
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "video.h"

/* The cut-point sequence */
#include "videocrypt-points.h"

/* Packet header sequence */
static const uint8_t _sequence[8] = {
	0x87,0x96,0xA5,0xB4,0xC3,0xD2,0xE1,0x87,
};

/* Hamming codes */
static const uint8_t _hamming[16] = {
	0x15,0x02,0x49,0x5E,0x64,0x73,0x38,0x2F,
	0xD0,0xC7,0x8C,0x9B,0xA1,0xB6,0xFD,0xEA,
};

/* Reverse bits in an 8-bit value */
static uint8_t _reverse(uint8_t b)
{
	b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
	b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
	b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
	return(b);
}

/* Apply VBI frame interleaving */
static void _interleave(uint8_t *frame)
{
	int b, i, j;
	int offset[6] = { 0, 6, 12, 20, 26, 32 };
	uint8_t r[8];
	uint8_t m;
	
	for(b = 0; b < 6; b++)
	{
		uint8_t *s = frame + offset[b];
		
		s[0] = _reverse(s[0]);
		s[7] = _reverse(s[7]);
		
		for(i = 0, m = 0x80; i < 8; i++, m >>= 1)
		{
			r[i] = 0x00;
			for(j = 0; j < 8; j++)
			{
				r[i] |= ((m & s[j]) ? 1 : 0) << j;
			}
		}
		
		memcpy(s, r, 8);
	}
}

int vc_init(vc_t *s, vid_t *vid)
{
	double f, l;
	int x;
	
	memset(s, 0, sizeof(vc_t));
	
	s->vid      = vid;
	s->counter  = 0;
	s->command  = 0;
	s->mode     = 0x05;
	
	/* Sample rate ratio */
	f = (double) s->vid->width / VC_WIDTH;
	
	/* Videocrypt timings appear to be calculated against the centre of the hsync pulse */
	l = (double) VC_SAMPLE_RATE * s->vid->conf.hsync_width / 2;
	
	/* Quick and dirty sample rate conversion array */
	for(x = 0; x < VC_WIDTH; x++)
	{
		s->video_scale[x] = round((l + x) * f);
	}
	
	/* Allocate memory for the 1-line delay */
	s->vid->delay += 1;
	s->delay = calloc(2 * vid->width, sizeof(int16_t));
	if(!s->delay)
	{
		return(VID_OUT_OF_MEMORY);
	}
	
	return(VID_OK);
}

void vc_free(vc_t *s)
{
	if(s->delay)
	{
		free(s->delay);
	}
}

void vc_render_line(vc_t *s)
{
	int x;
	const uint8_t *bline = NULL;
	
	/* On the first line of each frame, generate the VBI data */
	if(s->vid->line == 1)
	{
		uint8_t crc;
		
		if((s->counter & 7) < 4)
		{
			/* Packet framing */
			s->vbi[0] = _sequence[s->command & 0x07];
			s->vbi[10] = s->counter & 0xFF;
		}
		else
		{
			s->vbi[0]  = (_sequence[s->command & 0x07] & 0x0F) << 4;
			s->vbi[0] |= (_sequence[s->command & 0x07] & 0xF0) >> 4;
			s->vbi[10] = s->mode;
		}
		
		for(crc = 0x00, x = 0; x < 20; x++)
		{
			if(x % 10 == 9)
			{
				s->vbi[x] = crc;
				crc = 0;
			}
			else if(x % 10 == 0)
			{
				crc += s->vbi[x];
			}
			else
			{
				/* This version only transmits null commands */
				crc += s->vbi[x] = 0x00;
			}
		}
		
		/* Hamming code the VBI data */
		for(x = 19; x >= 0; x--)
		{
			s->vbi[x * 2 + 1] = _hamming[s->vbi[x] & 0x0F];
			s->vbi[x * 2 + 0] = _hamming[s->vbi[x] >> 4];
		}
		
		/* Interleave the VBI data */
		_interleave(s->vbi);
		
		/* After 8 frames, advance to the next command */
		s->counter++;
		if((s->counter & 0x07) == 0)
		{
			/* Move to the next command */
			s->command++;
		}
	}
	
	/* Calculate VBI line, or < 0 if not */
	if(s->vid->line >= VC_VBI_FIELD_1_START && s->vid->line < VC_VBI_FIELD_1_START + VC_VBI_LINES_PER_FIELD)
	{
		/* Top VBI field */
		bline = &s->vbi[(s->vid->line - VC_VBI_FIELD_1_START) * VC_VBI_BYTES_PER_LINE];
	}
	else if(s->vid->line >= VC_VBI_FIELD_2_START && s->vid->line < VC_VBI_FIELD_2_START + VC_VBI_LINES_PER_FIELD)
	{
		/* Bottom VBI field */
		bline = &s->vbi[(s->vid->line - VC_VBI_FIELD_2_START + VC_VBI_LINES_PER_FIELD) * VC_VBI_BYTES_PER_LINE];
	}
	
	/* Render the VBI line if necessary */
	if(bline)
	{
		int b, c;
		
		x = s->video_scale[VC_VBI_LEFT];
		
		for(b = 0; b < VC_VBI_BITS_PER_LINE; b++)
		{
			c = (bline[b / 8] >> (b % 8)) & 1;
			c = c ? 0xFFFFFF : 0x000000;
			
			for(; x < s->video_scale[VC_VBI_LEFT + VC_VBI_SAMPLES_PER_BIT * (b + 1)]; x++)
			{
				s->vid->output[x * 2] = s->vid->y_level_lookup[c];
			}
		}
	}
	
	/* Scramble the line if necessary */
	bline = NULL;
	
	if(s->vid->line > VC_FIELD_1_START && s->vid->line < VC_FIELD_1_START + VC_LINES_PER_FIELD)
	{
		x = s->counter;
		bline = &_points[x * VC_LINES_PER_FRAME + s->vid->line - VC_FIELD_1_START];
	}
	else if(s->vid->line > VC_FIELD_2_START && s->vid->line < VC_FIELD_2_START + VC_LINES_PER_FIELD)
	{
		x = s->counter;
		bline = &_points[x * VC_LINES_PER_FRAME + VC_LINES_PER_FIELD + s->vid->line - VC_FIELD_2_START];
	}
	
	if(bline)
	{
		int cut;
		int lshift;
		int y;
		
		cut = 105 + (0xFF - (int) *bline) * 2;
		lshift = 710 - cut;
		
		y = s->video_scale[VC_LEFT + lshift];
		for(x = s->video_scale[VC_LEFT]; x < s->video_scale[VC_LEFT + cut]; x++, y++)
		{
			s->delay[x * 2] = s->vid->output[y * 2];
		}
		
		y = s->video_scale[VC_LEFT];
		for(; x < s->video_scale[VC_RIGHT + VC_OVERLAP]; x++, y++)
		{
			s->delay[x * 2] = s->vid->output[y * 2];
		}
	}
	
	/* Delay by 1 line. Uses the Q part for temporary storage */
	for(x = 0; x < s->vid->width * 2; x += 2)
	{
		s->delay[x + 1] = s->vid->output[x];
		s->vid->output[x] = s->delay[x];
		s->delay[x] = s->delay[x + 1];
	}
}

