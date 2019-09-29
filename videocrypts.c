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

/* -=== Videocrypt S encoder ===-
 * 
 * This is untested on real hardware and should be considered just a
 * simulation. The VBI data *may* be valid but the shuffling sequence
 * is definitly not. There may also be colour distortion due to hacktv
 * not operating at the specified sample rate of FPAL * 4.
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "video.h"

/* The first line of each block */
static const int _block_start[12] = {
	 28,  75, 122, 169, 216, 263,
	340, 387, 434, 481, 528, 575,
};

/* Header synchronisation sequence */
static const uint8_t _sequence[8] = {
	0x81,0x92,0xA3,0xB4,0xC5,0xD6,0xE7,0xF0,
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

/* Reverse nibbles in a byte */
static inline uint8_t _rnibble(uint8_t a)
{
	return((a >> 4) | (a << 4));
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

/* Encode VBI data */
static void _encode_vbi(uint8_t vbi[40], const uint8_t data[16], uint8_t a, uint8_t b)
{
	int x;
	
	/* Set the information (a, b) and initial check bytes for each field */
	vbi[ 9] = vbi[ 0] = a;
	vbi[19] = vbi[10] = b;
	
	/* Copy the eight security bytes for each field,
	 * while updating the check byte */
	for(x = 0; x < 8; x++)
	{
		vbi[ 9] += vbi[ 1 + x] = data[0 + x];
		vbi[19] += vbi[11 + x] = data[8 + x];
	}
	
	/* Hamming code the VBI data */
	for(x = 19; x >= 0; x--)
	{
		vbi[x * 2 + 1] = _hamming[vbi[x] & 0x0F];
		vbi[x * 2 + 0] = _hamming[vbi[x] >> 4];
	}
	
	/* Interleave the VBI data */
	_interleave(vbi);
}

static void _block_shuffle(vcs_t *s)
{
	int i, t, r;
	
	for(i = 0; i < 47; i++)
	{
		s->block[i] = i;
	}
	
	for(i = 0; i < 47; i++)
	{
		r = rand() % 47;
		
		t = s->block[i];
		s->block[i] = s->block[r];
		s->block[r] = t;
	}
}

int vcs_init(vcs_t *s, vid_t *vid, const char *mode)
{
	double f;
	int x;
	
	memset(s, 0, sizeof(vcs_t));
	
	s->vid      = vid;
	s->mode     = 0x00;
	s->counter  = 0;
	
	if(strcmp(mode, "free") == 0)
	{
		/* Nothing yet */
	}
	else
	{
		fprintf(stderr, "Unrecognised Videocrypt S mode '%s'.\n", mode);
		return(VID_ERROR);
	}
	
	/* Sample rate ratio */
	f = (double) s->vid->width / VCS_WIDTH;
	
	/* Quick and dirty sample rate conversion array */
	for(x = 0; x < VCS_WIDTH; x++)
	{
		s->video_scale[x] = round(x * f);
	}
	
	/* Allocate memory for the delay */
	s->vid->olines += VCS_DELAY_LINES;
	
	return(VID_OK);
}

void vcs_free(vcs_t *s)
{
	/* Nothing */
}

void vcs_render_line(vcs_t *s)
{
	int x, j;
	uint8_t *bline = NULL;
	int line;
	
	/* Calculate which line is about to be transmitted due to the delay */
	line = s->vid->line - VCS_DELAY_LINES;
	if(line < 1) line += s->vid->conf.lines;
	
	/* Swap the active line with the oldest line in the delay buffer,
	 * with active video offset in j if necessary. */
	j = 0;
	
	if((line >=  28 && line <= 309) ||
	   (line >= 340 && line <= 621))
	{
		int block;
		int bline;
		
		/* Calculate the line number,
		 *   0 - 281 top field,
		 * 282 - 563 bottom field
		*/
		x = line - (line < 340 ? 28 : 340 - 282);
		
		/* Calculate block number and block line */
		block = x / 47;
		bline = x % 47;
		
		if(bline == 0)
		{
			_block_shuffle(s);
		}
		
		/* Calculate target block/line */
		block = (block + 1) % 12;
		bline = s->block[bline];
		
		/* Calculate position in delay buffer */
		j = (_block_start[block] + bline) - line;
		if(j < 0) j += s->vid->conf.lines - 1;
	}
	
	vid_adj_delay(s->vid, VCS_DELAY_LINES);
	
	if(j > 0)
	{
		int16_t *dline = s->vid->oline[s->vid->odelay + j];
		for(x = s->vid->active_left * 2; x < s->vid->width * 2; x += 2)
		{
			s->vid->output[x] = dline[x];
		}
	}
	
	/* On the first line of each frame, generate the VBI data */
	if(line == 1)
	{
		uint8_t crc;
		
		if((s->counter & 1) == 0)
		{
			/* The active message is updated every 2nd frame */
			for(crc = x = 0; x < 31; x++)
			{
				crc += s->message[x] = 0x00;
			}
			
			s->message[x] = ~crc + 1;
		}
		
		if((s->counter & 1) == 0)
		{
			/* The first half of the message */
			_encode_vbi(
				s->vbi, s->message,
				_sequence[(s->counter >> 1) & 7],
				s->counter & 0xFF
			);
		}
		else
		{
			/* The second half of the message */
			_encode_vbi(
				s->vbi, s->message + 16,
				_rnibble(_sequence[(s->counter >> 1) & 7]),
				(s->counter & 0x08 ? 0x00 : s->mode)
			);
		}
		
		/* After 64 frames, advance to the next block and codeword */
		s->counter++;
	}
	
	/* Set a pointer to the VBI data to render on this line, or NULL if none */
	if(line >= VCS_VBI_FIELD_1_START &&
	   line <  VCS_VBI_FIELD_1_START + VCS_VBI_LINES_PER_FIELD)
	{
		/* Top field VBI */
		bline = &s->vbi[(line - VCS_VBI_FIELD_1_START) * VCS_VBI_BYTES_PER_LINE];
	}
	else if(line >= VCS_VBI_FIELD_2_START &&
	        line <  VCS_VBI_FIELD_2_START + VCS_VBI_LINES_PER_FIELD)
	{
		/* Bottom field VBI */
		bline = &s->vbi[(line - VCS_VBI_FIELD_2_START + VCS_VBI_LINES_PER_FIELD) * VCS_VBI_BYTES_PER_LINE];
	}
	
	if(bline)
	{
		int b, c;
		
		/* Videocrypt S's VBI data sits in the active video area. Clear it first */
		for(x = s->vid->active_left; x < s->vid->active_left + s->vid->active_width; x++)
		{
			s->vid->output[x * 2] = s->vid->black_level;
		}
		
		x = s->video_scale[VCS_VBI_LEFT];
		
		for(b = 0; b < VCS_VBI_BITS_PER_LINE; b++)
		{
			c = (bline[b / 8] >> (b % 8)) & 1;
			c = c ? s->vid->white_level : s->vid->black_level;
			
			for(; x < s->video_scale[VCS_VBI_LEFT + VCS_VBI_SAMPLES_PER_BIT * (b + 1)]; x++)
			{
				s->vid->output[x * 2] = c;
			}
		}
	}
	
}

