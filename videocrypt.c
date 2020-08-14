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
 * Marco Wabbel for xtea algo and Funcard (ATMEL based) hex files - needed for xtea.
*/

#include <string.h>
#include <math.h>
#include "video.h"
#include "videocrypt-ca.h"
#include "videocrypt-blocks.h"

/* PPV card data */
/*                                   |--------CARD SERIAL-------|    Ka    Kb */
static uint8_t _ppv_card_data[7] = { 0x6D, 0xC1, 0x08, 0x44, 0x02, 0x28, 0x3D};

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
	int i, x;
	time_t t;
	srand((unsigned) time(&t));
	
	memset(s, 0, sizeof(vc_t));
	
	for(i = 0; i < 7; i++) s->_ppv_card_data[i] = _ppv_card_data[i];
	
	s->vid      = vid;
	s->counter  = 0;
	s->cw       = 0;
	
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
	else if(strcmp(mode, "sky03") == 0)
	{
		s->blocks    = _sky03_blocks;
		s->block_len = 2;
		_vc_seed_p03(&s->blocks[0]);
		_vc_seed_p03(&s->blocks[1]);
	}
	else if(strcmp(mode, "sky07") == 0)
	{
		s->blocks    = _sky07_blocks;
		s->block_len = 2;
		_vc_seed_p07(&s->blocks[0], VC_SKY7);
		_vc_seed_p07(&s->blocks[1], VC_SKY7);
		
		if(s->vid->conf.enableemm)
		{
			/*  
			 * 0x2C: allow Sky Multichannels
			 * 0x20: Enable card
			 */
			_vc_emm_p07(&s->blocks[0],0x2C,s->vid->conf.enableemm);
			_vc_emm_p07(&s->blocks[1],0x20,s->vid->conf.enableemm);
		}
		
		if(s->vid->conf.disableemm)
		{
			/*  
			 * 0x0C: switch off Sky Multichannels
			 * 0x00: Disable card 
			 */
			_vc_emm_p07(&s->blocks[0],0x0C,s->vid->conf.disableemm);
			_vc_emm_p07(&s->blocks[1],0x00,s->vid->conf.disableemm);
		}
	}
	else if(strcmp(mode, "sky09") == 0)
	{
		s->blocks    = _sky09_blocks;
		s->block_len = 2;
		_vc_seed_p09(&s->blocks[0]);
		_vc_seed_p09(&s->blocks[1]);
		
		if(s->vid->conf.enableemm)
		{
			/*  
			 * 0x2C: allow Sky Multichannels
			 * 0x20: Enable card
			 */
			_vc_emm_p09(&s->blocks[0],0x2C,s->vid->conf.enableemm);
			_vc_emm_p09(&s->blocks[1],0x20,s->vid->conf.enableemm);
		}
		
		if(s->vid->conf.disableemm)
		{
			/*  
			 * 0x0C: switch off Sky Multichannels
			 * 0x00: Disable card 
			 */
			_vc_emm_p09(&s->blocks[0],0x0C,s->vid->conf.disableemm);
			_vc_emm_p09(&s->blocks[1],0x00,s->vid->conf.disableemm);
		}
	}
	else if(strcmp(mode, "sky10") == 0)
	{
		s->blocks    = _sky10_blocks;
		s->block_len = 2;
	}			
	else if(strcmp(mode, "sky10ppv") == 0)
	{
		s->blocks    = _sky10ppv_blocks;
		s->block_len = 2;
	}
	else if(strcmp(mode, "sky11") == 0)
	{
		s->blocks    = _sky11_blocks;
		s->block_len = 2;
	}
	else if(strcmp(mode, "sky12") == 0)
	{
		s->blocks    = _sky12_blocks;
		s->block_len = 2;
	}
	else if (strcmp(mode, "tac1") == 0)
	{
		s->blocks    = _tac_blocks;
		s->block_len = 2;
		_vc_seed_p07(&s->blocks[0], VC_TAC1);
		_vc_seed_p07(&s->blocks[1], VC_TAC1);
	}
	else if (strcmp(mode, "tac2") == 0)
	{
		s->blocks    = _tac_blocks;
		s->block_len = 2;
		_vc_seed_p07(&s->blocks[0], VC_TAC2);
		_vc_seed_p07(&s->blocks[1], VC_TAC2);
		
		// /* Experimental EMMs for TAC cards */
		// if(s->vid->conf.enableemm)
		// {
		// 	/*  
		// 	 * 0x08: Unblock channel
		// 	 * 0x09: Enable card
		// 	 * 0x81: Set EXP date
		// 	 */
		// 	_vc_emm_p07(&s->blocks[0],0x08,s->vid->conf.enableemm);
		// 	_vc_emm_p07(&s->blocks[1],0x09,s->vid->conf.enableemm);
		// }
		
		// if(s->vid->conf.disableemm)
		// {
		// 	/*  
		// 	 * 0x28: Block channel
		// 	 * 0x29: Disable card
		// 	 */
		// 	_vc_emm_p07(&s->blocks[0],0x28,s->vid->conf.disableemm);
		// 	_vc_emm_p07(&s->blocks[1],0x29,s->vid->conf.disableemm);
		// }
	}
	else if (strcmp(mode, "xtea") == 0)
	{
		s->blocks    = _xtea_blocks;
		s->block_len = 2;
		_vc_seed_xtea(&s->blocks[0]);
		_vc_seed_xtea(&s->blocks[1]);
	}
	else if (strcmp(mode, "ppv") == 0)
	{
		s->blocks    = _ppv_blocks;
		s->block_len = 2;
		
		if(s->vid->conf.findkey)
		{
			/* Starting keys */
			s->_ppv_card_data[5] = 0x00; /* Key a */
			s->_ppv_card_data[6] = 0x00; /* Key b */
		}
		
		_vc_seed_ppv(&s->blocks[0], s->_ppv_card_data);
		_vc_seed_ppv(&s->blocks[1], s->_ppv_card_data);
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
	else if(strcmp(mode2, "conditional") == 0)
	{
		s->blocks2    = _vc2_blocks;
		s->block2_len = 2;
		_vc_seed_vc2(&s->blocks2[0], VC2_MC);
		_vc_seed_vc2(&s->blocks2[1], VC2_MC);
		
		/* If in simulcrypt mode, do the initial CW sync here */
		if(mode)
		{
			for(i = 0; i < 8; i++)
			{
				s->blocks2[1].messages[0][i + 17] = (s->blocks[0].codeword ^ s->blocks2[1].codeword) >> (8 * i) & 0xFF;
			}
		}
		
		if(s->vid->conf.enableemm)
		{
			/*  
			 * 0x1B: Enable card
			 */
			_vc2_emm(&s->blocks2[0],0x1B,s->vid->conf.enableemm, VC2_MC);
		}
		
		if(s->vid->conf.disableemm)
		{
			/*  
			 * 0x1A: Disable card
			 */
			_vc2_emm(&s->blocks2[0],0x1A,s->vid->conf.disableemm, VC2_MC);
		}
	}
	else
	{
		fprintf(stderr, "Unrecognised Videocrypt II mode '%s'.\n", mode2);
		return(VID_ERROR);
	}
	
	s->block2 = 0;
	
	/* Sample rate ratio */
	f = (double) s->vid->width / VC_WIDTH;
	
	/* Videocrypt timings appear to be calculated against the centre of the hsync pulse */
	l = (double) VC_SAMPLE_RATE * s->vid->conf.hsync_width / 2;
	
	/* Quick and dirty sample rate conversion array */
	for(x = 0; x < VC_WIDTH; x++)
	{
		s->video_scale[x] = round((l + x) * f);
	}
	
	/* Add one delay line */
	s->vid->olines += 1;
	
	return(VID_OK);
}

void vc_free(vc_t *s)
{
	/* Nothing */
}

void vc_render_line(vc_t *s, const char *mode, const char *mode2)
{
	int i, x;
	const uint8_t *bline = NULL;
	uint64_t cw;
	
	/* On the first line of each frame, generate the VBI data */
	if(s->vid->line == 1)
	{
		uint64_t iw;
		uint8_t crc;
		
		/* Videocrypt I */
		if(s->blocks)
		{
			if((s->counter & 7) == 0)
			{
				/* The active message is updated every 8th frame. The last
				 * message in the block is a duplicate of the first. */
				for(crc = x = 0; x < 31; x++)
				{
					crc += s->message[x] = s->blocks[s->block].messages[((s->counter >> 3) & 7) % 7][x];
				}
				
				s->message[x] = ~crc + 1;
			}
			
			if((s->counter & 4) == 0)
			{
				/* The first half of the message. Transmitted for 4 frames */
				_encode_vbi(
					s->vbi, s->message,
					_sequence[(s->counter >> 4) & 7],
					s->counter & 0xFF
				);
			}
			else
			{
				/* The second half of the message. Transmitted for 4 frames */
				_encode_vbi(
					s->vbi, s->message + 16,
					_rnibble(_sequence[(s->counter >> 4) & 7]),
					s->blocks[s->block].mode
				);
			}
		}
		
		/* Videocrypt II */
		if(s->blocks2)
		{
			if((s->counter & 1) == 0)
			{
				/* The active message is updated every 2nd frame */
				for(crc = x = 0; x < 31; x++)
				{
					crc += s->message2[x] = s->blocks2[s->block2].messages[(s->counter >> 1) & 7][x];
				}
				
				s->message2[x] = ~crc + 1;
			}
			
			if((s->counter & 1) == 0)
			{
				/* The first half of the message */
				_encode_vbi(
					s->vbi2, s->message2,
					_sequence2[(s->counter >> 1) & 7],
					s->counter & 0xFF
				);
			}
			else
			{
				/* The second half of the message */
				_encode_vbi(
					s->vbi2, s->message2 + 16,
					_rnibble(_sequence2[(s->counter >> 1) & 7]),
					(s->counter & 0x08 ? 0x00 : s->blocks2[s->block2].mode)
				);
			}
		}
		
		/* Reset the PRBS */
		iw = _generate_iw(s->cw, s->counter);
		s->sr1 = iw & VC_PRBS_SR1_MASK;
		s->sr2 = (iw >> 31) & VC_PRBS_SR2_MASK;
		
		s->counter++;
		
		/* After 64 frames, advance to the next VC1 block and codeword */
		if((s->counter & 0x3F) == 0)
		{
			/* Apply the current block codeword */
			if(s->blocks)
			{
				s->cw = s->blocks[s->block].codeword;
			}
			
			/* Generate new seeds */
			if(mode)
			{
				if(strcmp(mode,"tac1") == 0)  _vc_seed_p07(&s->blocks[s->block], VC_TAC1);
				if(strcmp(mode,"tac2") == 0)  _vc_seed_p07(&s->blocks[s->block], VC_TAC2);
				if(strcmp(mode,"sky07") == 0) _vc_seed_p07(&s->blocks[s->block], VC_SKY7);
				if(strcmp(mode,"sky09") == 0) _vc_seed_p09(&s->blocks[s->block]);
				if(strcmp(mode,"xtea") == 0)  _vc_seed_xtea(&s->blocks[s->block]);
				
				if(strcmp(mode,"ppv") == 0)
				{
					if(s->vid->conf.findkey)
					{
						if(s->_ppv_card_data[5] == 0xFF) s->_ppv_card_data[6]++;
						s->_ppv_card_data[5]++;
						
						fprintf(stderr, "\n\nTesting keys 0x%02X and 0x%02X...", (uint8_t) s->_ppv_card_data[5], (uint8_t) s->_ppv_card_data[6]);
						
						char fmt[24];
						sprintf(fmt,"KA - 0X%02X   KB - 0X%02X", (uint8_t) s->_ppv_card_data[5], (uint8_t) s->_ppv_card_data[6]);
						s->blocks[s->block].messages[strcmp(mode,"ppv") == 0 ? 1 : 0][0] = 0x20;
						s->blocks[s->block].messages[strcmp(mode,"ppv") == 0 ? 1 : 0][1] = 0x00;
						s->blocks[s->block].messages[strcmp(mode,"ppv") == 0 ? 1 : 0][2] = 0xF5;
						for(i = 0; i < 22; i++) s->blocks[s->block].messages[strcmp(mode,"ppv") == 0 ? 1 : 0][i + 3] = fmt[i];
						
					}
					
					_vc_seed_ppv(&s->blocks[s->block], s->_ppv_card_data);
				}
				
				if(s->vid->conf.showserial) s->blocks[s->block].messages[strcmp(mode,"ppv") == 0 ? 1 : 0][0] = 0x24;
				
			}
			
			/* Print ECM */
			if(s->vid->conf.showecm && mode)
			{
				fprintf(stderr, "\n\nVC1 ECM In:  ");
				for(i = 0; i < 31; i++) fprintf(stderr, "%02X ", s->blocks[s->block].messages[strcmp(mode,"ppv") == 0 ? 0 : 5][i]);
				fprintf(stderr,"\nVC1 ECM Out: ");
				for(i = 0; i < 8; i++) fprintf(stderr, "%02llX ", s->cw >> (8 * i) & 0xFF);
				
				if(s->vid->conf.enableemm || s->vid->conf.disableemm)
				{
					fprintf(stderr, "\nVC1 EMM In:  ");
					for(i = 0; i < 31; i++) fprintf(stderr, "%02X ", s->blocks[s->block].messages[2][i]);
				}
			}

			/* Move to the next block */
			if(++s->block == s->block_len)
			{
				s->block = 0;
			}
		}

		/* After 16 frames, apply the codeword */
		if((s->counter & 0x0F) == 0)
		{
			/* Apply the current block codeword only if not simulcrypting with VC1 */
			if(s->blocks2 && !mode)
			{
				s->cw = s->blocks2[s->block2].codeword;
			}

			if(mode2)
			{
				if(strcmp(mode2,"conditional") == 0) _vc_seed_vc2(&s->blocks2[s->block2], VC2_MC);
				
				/* OSD bytes 17 - 24 in OSD message 0x21 are used in seed generation in Videocrypt II. */
				/* XOR with VC1 seed for simulcrypt. */
				if(mode)
				{
					/* Hack to sync seeds with Videocrypt I */
					cw = (s->counter % 0x3F < 0x0F || s->counter % 0x3F > 0x2F ? s->blocks[s->block].codeword : s->cw) ^ s->blocks2[s->block2].codeword;
					for(i = 0; i < 8; i++)
					{
						s->blocks2[s->block2].messages[0][i + 17] = cw >> (8 * i) & 0xFF;
					}
				}
			}
			
			/* Print ECM */
			if(s->vid->conf.showecm && mode2)
			{
				fprintf(stderr, "\n\nVC2 ECM In:  ");
				for(i = 0; i < 31; i++) fprintf(stderr, "%02X ", s->blocks2[s->block2].messages[5][i]);
				fprintf(stderr,"\nVC2 ECM Out: ");
				for(i = 0; i < 8; i++) fprintf(stderr, "%02llX ", s->blocks2[s->block2].codeword >> (8 * i) & 0xFF);
				
				if(s->vid->conf.enableemm || s->vid->conf.disableemm)
				{
					fprintf(stderr, "\nVC2 EMM In:  ");
					for(i = 0; i < 31; i++) fprintf(stderr, "%02X ", s->blocks2[s->block2].messages[2][i]);
				}
			}
			
			/* Move to the next block after 64 frames */
			if(((s->counter & 0x3F) == 0) && (++s->block2 == s->block2_len))
			{
				s->block2 = 0;
			}
		}
	}
	
	/* Calculate VBI line, or < 0 if not */
	if(s->blocks &&
	   s->vid->line >= VC_VBI_FIELD_1_START &&
	   s->vid->line < VC_VBI_FIELD_1_START + VC_VBI_LINES_PER_FIELD)
	{
		/* Top VBI field */
		bline = &s->vbi[(s->vid->line - VC_VBI_FIELD_1_START) * VC_VBI_BYTES_PER_LINE];
	}
	else if(s->blocks &&
	        s->vid->line >= VC_VBI_FIELD_2_START &&
	        s->vid->line < VC_VBI_FIELD_2_START + VC_VBI_LINES_PER_FIELD)
	{
		/* Bottom VBI field */
		bline = &s->vbi[(s->vid->line - VC_VBI_FIELD_2_START + VC_VBI_LINES_PER_FIELD) * VC_VBI_BYTES_PER_LINE];
	}
	else if(s->blocks2 &&
	        s->vid->line >= VC2_VBI_FIELD_1_START &&
	        s->vid->line < VC2_VBI_FIELD_1_START + VC_VBI_LINES_PER_FIELD)
	{
		/* Top VBI field VC2 */
		bline = &s->vbi2[(s->vid->line - VC2_VBI_FIELD_1_START) * VC_VBI_BYTES_PER_LINE];
	}
	else if(s->blocks2 &&
	        s->vid->line >= VC2_VBI_FIELD_2_START &&
	        s->vid->line < VC2_VBI_FIELD_2_START + VC_VBI_LINES_PER_FIELD)
	{
		/* Bottom VBI field VC2 */
		bline = &s->vbi2[(s->vid->line - VC2_VBI_FIELD_2_START + VC_VBI_LINES_PER_FIELD) * VC_VBI_BYTES_PER_LINE];
	}
	
	/* Render the VBI line if necessary */
	if(bline)
	{
		int b, c;
		
		x = s->video_scale[VC_VBI_LEFT];
		
		for(b = 0; b < VC_VBI_BITS_PER_LINE; b++)
		{
			c = (bline[b / 8] >> (b % 8)) & 1;
			c = c ? s->vid->white_level : s->vid->black_level;
			
			for(; x < s->video_scale[VC_VBI_LEFT + VC_VBI_SAMPLES_PER_BIT * (b + 1)]; x++)
			{
				s->vid->output[x * 2] = c;
			}
		}
		
		*s->vid->vbialloc = 1;
	}
	
	/* Scramble the line if necessary */
	x = -1;
	
	if((s->vid->line >= VC_FIELD_1_START && s->vid->line < VC_FIELD_1_START + VC_LINES_PER_FIELD) ||
	   (s->vid->line >= VC_FIELD_2_START && s->vid->line < VC_FIELD_2_START + VC_LINES_PER_FIELD))
	{
		int i;
		
		x = (s->c >> 8) & 0xFF;
		
		for(i = 0; i < 16; i++)
		{
			int a;
			
			/* Update shift registers */
			s->sr1 = (s->sr1 >> 1) ^ (s->sr1 & 1 ? 0x7BB88888UL : 0);
			s->sr2 = (s->sr2 >> 1) ^ (s->sr2 & 1 ? 0x17A2C100UL : 0);
			
			/* Load the multiplexer address */
			a = _rev(s->sr2, 29) & 0x1F;
			if(a == 31) a = 30;
			
			/* Shift into result register */
			s->c = (s->c >> 1) | (((_rev(s->sr1, 31) >> a) & 1) << 15);
		}
		
		/* Line 336 is scrambled into line 335, a VBI line. Mark it
		 * as allocated to prevent teletext data appearing there */
		if(s->vid->line == 336)
		{
			s->vid->vbialloclist[s->vid->odelay - 1] = 1;
		}
	}
	
	/* Hack to preserve WSS signal data */
	if(s->vid->line == 24) x = -1;
	
	if(x != -1)
	{
		int cut;
		int lshift;
		int y;
		int16_t *delay = s->vid->oline[s->vid->odelay - 1];
		
		cut = 105 + (0xFF - x) * 2;
		lshift = 710 - cut;
		
		y = s->video_scale[VC_LEFT + lshift];
		for(x = s->video_scale[VC_LEFT]; x < s->video_scale[VC_LEFT + cut]; x++, y++)
		{
			delay[x * 2] = s->vid->output[y * 2];
		}
		
		y = s->video_scale[VC_LEFT];
		for(; x < s->video_scale[VC_RIGHT + VC_OVERLAP]; x++, y++)
		{
			delay[x * 2] = s->vid->output[y * 2];
		}
	}
	
	vid_adj_delay(s->vid, 1);
}

