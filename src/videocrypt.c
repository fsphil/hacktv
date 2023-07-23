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
 * This is a Videocrypt I/II encoder. It scrambles the image using a technique
 * called "line cut-and-rotate", and inserts the necessary data into the
 * VBI area of the image to activate the Videocrypt hardware unscrambler.
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
 * Alex L. James for providing an active Sky subscriber card, VBI samples,
 * Videocrypt 2 information and testing.
 *
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "video.h"
#include "vbidata.h"

/* Packet header sequences */
static const uint8_t _sequence[8] = {
	0x87,0x96,0xA5,0xB4,0xC3,0xD2,0xE1,0x87,
};

static const uint8_t _sequence2[8] = {
	0x80,0x91,0xA2,0xB3,0xC4,0xD5,0xE6,0xF7,
};

/* Hamming codes */
static const uint8_t _hamming[16] = {
	0x15,0x02,0x49,0x5E,0x64,0x73,0x38,0x2F,
	0xD0,0xC7,0x8C,0x9B,0xA1,0xB6,0xFD,0xEA,
};

/* Blocks for VC1 free-access decoding */
static const _vc_block_t _fa_blocks[] = { { 0x05, VC_PRBS_CW_FA } };

/* Blocks for VC1 conditional-access sample, taken from MTV UK and modified, */
/* requires an active Sky card to decode */
static const _vc_block_t _mtv_blocks[] = {
	{
		0x07, 0xB2DD55A7BCE178EUL,
		{
			{ 0x20 },
			{ 0x00 },
			{ 0x00 },
			{ 0x00 },
			{ 0x00 },
			{ 0x00 },
			{ 0xF8,0x19,0x10,0x83,0x20,0x85,0x60,0xAF,0x8F,0xF0,0x49,0x34,0x86,0xC4,0x6A,0xCA,0xC3,0x21,0x4D,0x44,0xB3,0x24,0x36,0x57,0xEC,0xA7,0xCE,0x12,0x38,0x91,0x3E }
		}
	},
	{
		0x07, 0xF9885DA50770B80UL,
		{
			/* Modify the following line to change the channel name displayed by the decoder.
			 * The third byte is 0x60 + number of characters, followed by the ASCII characters themselves. */
			{ 0x20,0x00,0x69,0x20,0x20,0x20,0x48,0x41,0x43,0x4B,0x54,0x56 },
			{ 0x00 },
			{ 0x00 },
			{ 0x00 },
			{ 0x00 },
			{ 0x00 },
			{ 0xF8,0x19,0x10,0x83,0x20,0xD1,0xB5,0xA9,0x1F,0x82,0xFE,0xB3,0x6B,0x0A,0x82,0xC3,0x30,0x7B,0x65,0x9C,0xF2,0xBD,0x5C,0xB0,0x6A,0x3B,0x64,0x0F,0xA2,0x66,0xBB }
		}
	},
};

/* Blocks for VC2 free-access decoding */
static const _vc2_block_t _fa2_blocks[] = { { 0x9C, VC_PRBS_CW_FA } };

/* Reverse bits in an 8-bit value */
static uint8_t _reverse(uint8_t b)
{
	b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
	b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
	b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
	return(b);
}

/* Reverse bits in an x-bit value */
static uint64_t _rev(uint64_t b, int x)
{
	uint64_t r = 0;
	
	while(x--)
	{
		r = (r << 1) | (b & 1);
		b >>= 1;
	}
	

	return(r);
}

/* Reverse nibbles in a byte */
static inline uint8_t _rnibble(uint8_t a)
{
	return((a >> 4) | (a << 4));
}

/* Generate IW for PRBS */
static uint64_t _generate_iw(uint64_t cw, uint8_t fcnt)
{
	uint64_t iw;
	
	/* FCNT is repeated 8 times, each time inverted */
	iw  = ((fcnt ^ 0xFF) << 8) | fcnt;
	iw |= (iw << 16) | (iw << 32) | (iw << 48);
	
	return((iw ^ cw) & VC_PRBS_CW_MASK);
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
	uint8_t crc;
	
	crc = vbi[0] = a;
	for(x = 0; x < 8; x++)
	{
		crc += vbi[1 + x] = data[0 + x];
	}
	vbi[9] = crc;
	
	crc = vbi[10] = b;
	for(x = 0; x < 8; x++)
	{
		crc += vbi[11 + x] = data[8 + x];
	}
	vbi[19] = crc;
	
	/* Hamming code the VBI data */
	for(x = 19; x >= 0; x--)
	{
		vbi[x * 2 + 1] = _hamming[vbi[x] & 0x0F];
		vbi[x * 2 + 0] = _hamming[vbi[x] >> 4];
	}
	
	/* Interleave the VBI data */
	_interleave(vbi);
}

int vc_init(vc_t *s, vid_t *vid, const char *mode, const char *mode2)
{
	double f, l;
	int x;
	
	memset(s, 0, sizeof(vc_t));
	
	/* Generate the VBI data symbols */
	s->lut = vbidata_init_step(
		40,
		vid->width,
		round((vid->white_level - vid->black_level) * 1.00),
		(double) vid->pixel_rate / VC_SAMPLE_RATE * VC_VBI_SAMPLES_PER_BIT,
		vid->pixel_rate * 375e-9,
		vid->pixel_rate * 10.86e-6
	);
	
	if(!s->lut)
	{
		return(VID_OUT_OF_MEMORY);
	}
	
	s->counter  = 0;
	s->cw       = VC_PRBS_CW_FA;
	
	/* Videocrypt I setup */
	if(mode == NULL)
	{
		s->blocks    = NULL;
		s->block_len = 0;
	}
	else if(strcmp(mode, "free") == 0)
	{
		s->blocks    = _fa_blocks;
		s->block_len = 1;
	}
	else if(strcmp(mode, "conditional") == 0)
	{
		s->blocks    = _mtv_blocks;
		s->block_len = 2;
	}
	else
	{
		fprintf(stderr, "Unrecognised Videocrypt I mode '%s'.\n", mode);
		return(VID_ERROR);
	}
	
	s->block = 0;
	
	/* Videocrypt II setup */
	if(mode2 == NULL)
	{
		s->blocks2    = NULL;
		s->block2_len = 0;
	}
	else if(strcmp(mode2, "free") == 0)
	{
		s->blocks2    = _fa2_blocks;
		s->block2_len = 1;
	}
	else
	{
		fprintf(stderr, "Unrecognised Videocrypt II mode '%s'.\n", mode2);
		return(VID_ERROR);
	}
	
	s->block2 = 0;
	
	/* Sample rate ratio */
	f = (double) vid->width / VC_WIDTH;
	
	/* Videocrypt timings appear to be calculated against the centre of the hsync pulse */
	l = (double) VC_SAMPLE_RATE * vid->conf.hsync_width / 2;
	
	/* Quick and dirty sample rate conversion array */
	for(x = 0; x < VC_WIDTH; x++)
	{
		s->video_scale[x] = round((l + x) * f);
	}
	
	return(VID_OK);
}

void vc_free(vc_t *s)
{
	free(s->lut);
}

int vc_render_line(vid_t *s, void *arg, int nlines, vid_line_t **lines)
{
	vc_t *v = arg;
	int x;
	const uint8_t *bline = NULL;
	vid_line_t *l = lines[0];
	
	/* On the first line of each frame, generate the VBI data */
	if(l->line == 1)
	{
		uint64_t iw;
		uint8_t crc;
		
		/* Videocrypt I */
		if(v->blocks)
		{
			if((v->counter & 7) == 0)
			{
				/* The active message is updated every 8th frame. The last
				 * message in the block is a duplicate of the first. */
				for(crc = x = 0; x < 31; x++)
				{
					crc += v->message[x] = v->blocks[v->block].messages[((v->counter >> 3) & 7) % 7][x];
				}
				
				v->message[x] = ~crc + 1;
			}
			
			if((v->counter & 4) == 0)
			{
				/* The first half of the message. Transmitted for 4 frames */
				_encode_vbi(
					v->vbi, v->message,
					_sequence[(v->counter >> 4) & 7],
					v->counter & 0xFF
				);
			}
			else
			{
				/* The second half of the message. Transmitted for 4 frames */
				_encode_vbi(
					v->vbi, v->message + 16,
					_rnibble(_sequence[(v->counter >> 4) & 7]),
					v->blocks[v->block].mode
				);
			}
		}
		
		/* Videocrypt II */
		if(v->blocks2)
		{
			if((v->counter & 1) == 0)
			{
				/* The active message is updated every 2nd frame */
				for(crc = x = 0; x < 31; x++)
				{
					crc += v->message2[x] = v->blocks2[v->block2].messages[(v->counter >> 1) & 7][x];
				}
				
				v->message2[x] = ~crc + 1;
			}
			
			if((v->counter & 1) == 0)
			{
				/* The first half of the message */
				_encode_vbi(
					v->vbi2, v->message2,
					_sequence2[(v->counter >> 1) & 7],
					v->counter & 0xFF
				);
			}
			else
			{
				/* The second half of the message */
				_encode_vbi(
					v->vbi2, v->message2 + 16,
					_rnibble(_sequence2[(v->counter >> 1) & 7]),
					(v->counter & 0x08 ? 0x00 : v->blocks2[v->block2].mode)
				);
			}
		}
		
		/* Reset the PRBS */
		iw = _generate_iw(v->cw, v->counter);
		v->sr1 = iw & VC_PRBS_SR1_MASK;
		v->sr2 = (iw >> 31) & VC_PRBS_SR2_MASK;
		
		v->counter++;
		
		/* After 64 frames, advance to the next VC1 block and codeword */
		if((v->counter & 0x3F) == 0)
		{
			/* Apply the current block codeword */
			if(v->blocks)
			{
				v->cw = v->blocks[v->block].codeword;
			}
			
			/* Move to the next block */
			if(++v->block == v->block_len)
			{
				v->block = 0;
			}
		}
		
		/* After 16 frames, advance to the next VC2 block and codeword */
		if((v->counter & 0x0F) == 0)
		{
			/* Apply the current block codeword */
			if(v->blocks2)
			{
				v->cw = v->blocks2[v->block2].codeword;
			}
			
			/* Move to the next block */
			if(++v->block2 == v->block2_len)
			{
				v->block2 = 0;
			}
		}
	}
	
	/* Calculate VBI line, or < 0 if not */
	if(v->blocks &&
	   l->line >= VC_VBI_FIELD_1_START &&
	   l->line < VC_VBI_FIELD_1_START + VC_VBI_LINES_PER_FIELD)
	{
		/* Top VBI field */
		bline = &v->vbi[(l->line - VC_VBI_FIELD_1_START) * VC_VBI_BYTES_PER_LINE];
	}
	else if(v->blocks &&
	        l->line >= VC_VBI_FIELD_2_START &&
	        l->line < VC_VBI_FIELD_2_START + VC_VBI_LINES_PER_FIELD)
	{
		/* Bottom VBI field */
		bline = &v->vbi[(l->line - VC_VBI_FIELD_2_START + VC_VBI_LINES_PER_FIELD) * VC_VBI_BYTES_PER_LINE];
	}
	else if(v->blocks2 &&
	        l->line >= VC2_VBI_FIELD_1_START &&
	        l->line < VC2_VBI_FIELD_1_START + VC_VBI_LINES_PER_FIELD)
	{
		/* Top VBI field VC2 */
		bline = &v->vbi2[(l->line - VC2_VBI_FIELD_1_START) * VC_VBI_BYTES_PER_LINE];
	}
	else if(v->blocks2 &&
	        l->line >= VC2_VBI_FIELD_2_START &&
	        l->line < VC2_VBI_FIELD_2_START + VC_VBI_LINES_PER_FIELD)
	{
		/* Bottom VBI field VC2 */
		bline = &v->vbi2[(l->line - VC2_VBI_FIELD_2_START + VC_VBI_LINES_PER_FIELD) * VC_VBI_BYTES_PER_LINE];
	}
	
	/* Render the VBI line if necessary */
	if(bline)
	{
		vbidata_render(v->lut, bline, 0, 40, VBIDATA_LSB_FIRST, l);
		l->vbialloc = 1;
	}
	
	/* Scramble the line if necessary */
	x = -1;
	
	if((l->line >= VC_FIELD_1_START && l->line < VC_FIELD_1_START + VC_LINES_PER_FIELD) ||
	   (l->line >= VC_FIELD_2_START && l->line < VC_FIELD_2_START + VC_LINES_PER_FIELD))
	{
		int i;
		
		x = (v->c >> 8) & 0xFF;
		
		for(i = 0; i < 16; i++)
		{
			int a;
			
			/* Update shift registers */
			v->sr1 = (v->sr1 >> 1) ^ (v->sr1 & 1 ? 0x7BB88888UL : 0);
			v->sr2 = (v->sr2 >> 1) ^ (v->sr2 & 1 ? 0x17A2C100UL : 0);
			
			/* Load the multiplexer address */
			a = _rev(v->sr2, 29) & 0x1F;
			if(a == 31) a = 30;
			
			/* Shift into result register */
			v->c = (v->c >> 1) | (((_rev(v->sr1, 31) >> a) & 1) << 15);
		}
		
		/* Line 336 is scrambled into line 335, a VBI line. Mark it
		 * as allocated to prevent teletext data appearing there */
		if(l->line == 335)
		{
			l->vbialloc = 1;
		}
	}
	
	/* Hack to preserve WSS signal data */
	if(l->line == 23) x = -1;
	
	if(x != -1)
	{
		int cut;
		int lshift;
		int y;
		int16_t *delay = lines[1]->output;
		
		cut = 105 + (0xFF - x) * 2;
		lshift = 710 - cut;
		
		y = v->video_scale[VC_LEFT + lshift];
		for(x = v->video_scale[VC_LEFT]; x < v->video_scale[VC_LEFT + cut]; x++, y++)
		{
			l->output[x * 2] = delay[y * 2];
		}
		
		y = v->video_scale[VC_LEFT];
		for(; x < v->video_scale[VC_RIGHT + VC_OVERLAP]; x++, y++)
		{
			l->output[x * 2] = delay[y * 2];
		}
	}
	
	return(1);
}

