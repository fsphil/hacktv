/* Nagravision Syster encoder for hacktv                                 */
/*=======================================================================*/
/* Copyright 2018 Alex L. James                                          */
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

/* -=== Nagravision Syster encoder ===-
 * 
 * These functions implement the image scrambler for Nagravision Syster.
 * This system uses line shuffling to obscure the image.
 * 
 * There is some limited support for real hardware decoders.
*/

/* Syster VBI data
 * 
 * Some or all of the notes here might be wrong. They're based on
 * data recovered from VHS recordings of Premere (Germany).
 * 
 * Data is transmitted on two VBI lines per field, 224 bits / 28 bytes
 * each encoded as NRZ. The clock rate is 284 * fH. Bytes are transmitted
 * LSB first.
 * 
 * -----------------------------------------------
 * | sync (32) | seq (8) | data (168) | crc (16) |
 * -----------------------------------------------
 * 
 * sync: 10101010 00001011 00011000 00110110 / 55 D0 18 6C
 *  seq: Hamming code sync sequence: 15 FD 73 9B 5E B6 49 A1 02 EA
 * data: Payload data (21 bytes)
 *  crc: 16-bit CRC of the 22 bytes between sync and crc
 * 
 * Blocks (packets, frames?) of 210 bytes are transmitted in 10 parts,
 * each 21 bytes long. The seq field indicates which part is currently
 * being transmitted. A block begins with seq code 15 and ends with
 * code EA, and are always transmitted in this order without interruption.
 * 
 * The decoder never activates without a key inserted. There does not appear
 * to be an equivilent to the free-access mode in Videocrypt, which works
 * with or without a card inserted.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "video.h"
#include "vbidata.h"

/* The standard syster substitution table */

static const uint8_t _key_table[256] = {
	10, 11, 12, 13, 16, 17, 18, 19, 13, 14, 15, 16,  0,  1,  2,  3,
	21, 22, 23, 24, 18, 19, 20, 21, 23, 24, 25, 26, 26, 27, 28, 29,
	19, 20, 21, 22, 11, 12, 13, 14, 28, 29, 30, 31,  4,  5,  6,  7,
	22, 23, 24, 25,  5,  6,  7,  8, 31,  0,  1,  2, 27, 28, 29, 30,
	 3,  4,  5,  6,  8,  9, 10, 11, 14, 15, 16, 17, 25, 26, 27, 28,
	15, 16, 17, 18,  7,  8,  9, 10, 17, 18, 19, 20, 29, 30, 31,  0,
	24, 25, 26, 27, 20, 21, 22, 23,  1,  2,  3,  4,  6,  7,  8,  9,
	12, 13, 14, 15,  9, 10, 11, 12,  2,  3,  4,  5, 30, 31,  0,  1,
	24, 25, 26, 27,  2,  3,  4,  5, 31,  0,  1,  2,  7,  8,  9, 10,
	13, 14, 15, 16, 26, 27, 28, 29, 14, 15, 16, 17, 18, 19, 20, 21,
	22, 23, 24, 25,  5,  6,  7,  8, 19, 20, 21, 22, 12, 13, 14, 15,
	17, 18, 19, 20, 27, 28, 29, 30, 10, 11, 12, 13, 11, 12, 13, 14,
	 6,  7,  8,  9,  1,  2,  3,  4,  0,  1,  2,  3,  4,  5,  6,  7,
	 3,  4,  5,  6,  8,  9, 10, 11, 15, 16, 17, 18, 23, 24, 25, 26,
	29, 30, 31,  0, 25, 26, 27, 28,  9, 10, 11, 12, 21, 22, 23, 24,
	20, 21, 22, 23, 30, 31,  0,  1, 16, 17, 18, 19, 28, 29, 30, 31
};

static const uint8_t _vbi_sync[4] = {
	0x55, 0xD0, 0x18, 0x6C
};

static const uint8_t _vbi_sequence[10] = {
	0x15, 0xFD, 0x73, 0x9B, 0x5E, 0xB6, 0x49, 0xA1, 0x02, 0xEA
};

static const uint8_t _block_sequence[320] = {
	0x20,0x05, 0x21,0x05,
	0x30,0x05, 0x31,0x05,
	0x10,0x05, 0x11,0x05,
	0x50,0x05, 0x51,0x05,
	0x70,0x01, 0x71,0x00,
	0x6E,0x05, 0x6F,0x05,
	0x40,0x05, 0x41,0x05,
	0x60,0x05, 0x61,0x05,
	0x00,0x05, 0x01,0x05,
	0x72,0x01, 0x73,0x0F,
	0x22,0x05, 0x23,0x05,
	0x32,0x05, 0x33,0x05,
	0x12,0x05, 0x13,0x05,
	0x52,0x05, 0x53,0x05,
	0x72,0x01, 0x73,0x00,
	0x70,0x05, 0x71,0x05,
	0x42,0x05, 0x43,0x05,
	0x62,0x05, 0x63,0x05,
	0x02,0x05, 0x03,0x05,
	0x74,0x01, 0x75,0x0F,
	
	0x24,0x05, 0x25,0x05,
	0x34,0x05, 0x35,0x05,
	0x14,0x05, 0x15,0x05,
	0x54,0x05, 0x55,0x05,
	0x74,0x01, 0x75,0x00,
	0x72,0x05, 0x73,0x05,
	0x44,0x05, 0x45,0x05,
	0x64,0x05, 0x65,0x05,
	0x04,0x05, 0x05,0x05,
	0x76,0x01, 0x77,0x0F,
	0x26,0x05, 0x27,0x05,
	0x36,0x05, 0x37,0x05,
	0x16,0x05, 0x17,0x05,
	0x56,0x05, 0x57,0x05,
	0x76,0x01, 0x77,0x00,
	0x74,0x05, 0x75,0x05,
	0x46,0x05, 0x47,0x05,
	0x66,0x05, 0x67,0x05,
	0x06,0x05, 0x07,0x05,
	0x78,0x01, 0x79,0x0F,
	
	0x28,0x05, 0x29,0x05,
	0x38,0x05, 0x39,0x05,
	0x18,0x05, 0x19,0x05,
	0x58,0x05, 0x59,0x05,
	0x78,0x01, 0x79,0x00,
	0x76,0x05, 0x77,0x05,
	0x48,0x05, 0x49,0x05,
	0x68,0x05, 0x69,0x05,
	0x08,0x05, 0x09,0x05,
	0x7A,0x01, 0x7B,0x0F,
	0x2A,0x05, 0x2B,0x05,
	0x3A,0x05, 0x3B,0x05,
	0x1A,0x05, 0x1B,0x05,
	0x5A,0x05, 0x5B,0x05,
	0x7A,0x01, 0x7B,0x00,
	0x78,0x05, 0x79,0x05,
	0x4A,0x05, 0x4B,0x05,
	0x6A,0x05, 0x6B,0x05,
	0x0A,0x05, 0x0B,0x05,
	0x7C,0x01, 0x7D,0x0F,
	
	0x2C,0x05, 0x2D,0x05,
	0x3C,0x05, 0x3D,0x05,
	0x1C,0x05, 0x1D,0x05,
	0x5C,0x05, 0x5D,0x05,
	0x7C,0x01, 0x7D,0x00,
	0x7A,0x05, 0x7B,0x05,
	0x4C,0x05, 0x4D,0x05,
	0x6C,0x05, 0x6D,0x05,
	0x0C,0x05, 0x0D,0x05,
	0x7E,0x01, 0x7F,0x0F,
	0x2E,0x05, 0x2F,0x05,
	0x3E,0x05, 0x3F,0x05,
	0x1E,0x05, 0x1F,0x05,
	0x5E,0x05, 0x5F,0x05,
	0x7E,0x01, 0x7F,0x00,
	0x7C,0x05, 0x7D,0x05,
	0x4E,0x05, 0x4F,0x05,
	0x6E,0x05, 0x6F,0x05,
	0x0E,0x05, 0x0F,0x05,
	0x00,0x01, 0x01,0x0F,
};

static const uint8_t _filler[8] = {
	0x3E, 0x68, 0x61, 0x63, 0x6B, 0x74, 0x76, 0x3E,
};

static const uint8_t _permutations[] = {
/*	   s,   r,    s,   r, */
	0x3E,0x05, 0x4A,0xFE,
	0x70,0x1C, 0x00,0x5D,
	0x12,0xFE, 0x0C,0xC7,
	0x32,0x00, 0x37,0x72,
	0x29,0xBC, 0x62,0x26,
	0x39,0x68, 0x3B,0xD4,
	0x13,0x12, 0x0C,0x1E,
	0x08,0x66, 0x48,0x51,
	0x56,0x03, 0x36,0x43,
	0x44,0xB3, 0x1E,0xF6,
	0x71,0x78, 0x0F,0xC8,
	0x01,0xF9, 0x60,0x1B,
	0x06,0x6A, 0x28,0x89,
	0x2D,0x7C, 0x02,0xDB,
	0x58,0x11, 0x02,0x2B,
	0x1F,0xD1, 0x75,0xBE,
	0x06,0xE5, 0x04,0x40,
	0x68,0xCD, 0x58,0x2F,
	0x53,0x85, 0x1F,0xDA,
	0x6C,0xAF, 0x65,0x2E,
	0x15,0x63, 0x55,0x14,
	0x0C,0xDD, 0x7D,0x83,
	0x37,0xE2, 0x46,0xCB,
	0x00,0x83, 0x39,0x42,
	0x33,0xFF, 0x1A,0x00,
};

static uint16_t _crc(const uint8_t *data, size_t length)
{
	uint16_t crc = 0x0000;
	const uint16_t poly = 0xC003;
	int b;
	
	while(length--)
	{
		crc ^= *(data++);
		
		for(b = 0; b < 8; b++)
		{
			crc = (crc & 1 ? (crc >> 1) ^ poly : crc >> 1);
		}
	}
	
	return(crc);
}

static void _update_field_order(ng_t *s)
{
	int i, j;
	int b[32];
	
	/* This function generates the scrambled line order for the
	 * next field based on _key_table, s->s and s->r parameters.
	 *
	 * Based on work by Markus G. Kuhn from his publication
	 * 'Analysis of the Nagravision Video Scrambling Method', 1998-07-09
	*/
	
	for(i = 0; i < 32; i++)
	{
		b[i] = -32 + i;
	}
	
	for(i = 0; i < 287; i++)
	{
		j = i <= 254 ? _key_table[(s->r + (2 * s->s + 1) * i) & 0xFF] : i - 255;
		b[j] = s->order[b[j] + 32] = i;
	}
}

int ng_init(ng_t *s, vid_t *vid)
{
	int i;
	
	memset(s, 0, sizeof(ng_t));
	
	s->vid = vid;
	
	/* Calculate the high level for the VBI data, 66% of the white level */
	i = round((vid->y_level_lookup[0xFFFFFF] - vid->y_level_lookup[0x000000]) * 0.66);
	s->lut = vbidata_init(
		NG_VBI_WIDTH, s->vid->width,
		i,
		VBIDATA_FILTER_RC, 0.7
	);
	
	if(!s->lut)
	{
		return(VID_OUT_OF_MEMORY);
	}
	
	s->vbi_seq = 0;
	s->block_seq = 0;
	s->block_seq2 = 0;
	
	/* Initial seeds. Updated every field. */
	s->s = 0;
	s->r = 0;
	_update_field_order(s);
	
	/* Allocate memory for the delay */
	s->vid->delay += NG_DELAY_LINES;
	s->delay = calloc(2 * vid->width * NG_DELAY_LINES, sizeof(int16_t));
	if(!s->delay)
	{
		return(VID_OUT_OF_MEMORY);
	}
	
	/* Setup the delay line pointers */
	for(i = 0; i < NG_DELAY_LINES; i++)
	{
		s->delay_line[i] = &s->delay[2 * vid->width * i];
	}
	
	return(VID_OK);
}

void ng_free(ng_t *s)
{
	free(s->delay);
	free(s->lut);
}

void ng_render_line(ng_t *s)
{
	int j = 0;
	int x, f, i;
	int line;
	int16_t *dline;
	
	/* Calculate which line is about to be transmitted due to the delay */
	line = s->vid->line - NG_DELAY_LINES;
	if(line < 0) line += s->vid->conf.lines;
	
	/* Calculate the field and field line */
	f = (line < NG_FIELD_2_START ? 1 : 2);
	i = line - (f == 1 ? NG_FIELD_1_START : NG_FIELD_2_START);
	
	if(i >= 0 && i < NG_LINES_PER_FIELD)
	{
		/* Adjust for the decoder's 32 line delay */
		i += 32;
		if(i >= NG_LINES_PER_FIELD)
		{
			i -= NG_LINES_PER_FIELD;
			f = (f == 1 ? 2 : 1);
		}
		
		/* Reinitialise the seeds if this is a new field */
		if(i == 0)
		{
			int framesync = (s->vid->frame % 25) * 4;
			
			/* Check is field is odd or even */
			if(s->vid->line == 310)
			{
				s->s = _permutations[framesync + 0];
				s->r = _permutations[framesync + 1];
			}
			else
			{
				s->s = _permutations[framesync + 2];
				s->r = _permutations[framesync + 3];
			}
			
			_update_field_order(s);
		}
		
		/* Calculate which line in the delay buffer to copy image data from */
		j = (f == 1 ? NG_FIELD_1_START : NG_FIELD_2_START) + s->order[i];
		if(j < line) j += s->vid->conf.lines;
		j -= line;
		
		if(j < 0 || j >= NG_DELAY_LINES)
		{
			/* We should never get to this point */
			fprintf(stderr, "*** Nagravision Syster scrambler is trying to read an invalid line ***\n");
			j = 0;
		}
	}
	
	/* Swap the active line with the oldest line in the delay buffer,
	 * with active video offset in j if necessary. */
	for(x = 0; x < s->vid->width * 2; x += 2)
	{
		int16_t t = s->vid->output[x];
		s->vid->output[x] = s->delay_line[x >= s->vid->active_left * 2 ? j : 0][x];
		s->delay_line[0][x] = t;
	}
	
	/* Advance the delay buffer */
	dline = s->delay_line[0];
	for(x = 0; x < NG_DELAY_LINES - 1; x++)
	{
		s->delay_line[x] = s->delay_line[x + 1];
	}
	s->delay_line[x] = dline;
	
	/* Render the VBI data
	 * These lines where used by Premiere */
	if(line == 14 || line == 15 || line == 327 || line == 328)
	{
		uint8_t vbi[NG_VBI_BYTES];
		uint16_t crc;
		
		/* Prepare the VBI line */
		memcpy(&vbi[0], _vbi_sync, sizeof(_vbi_sync));
		vbi[4] = _vbi_sequence[s->vbi_seq];
		
		if(s->vbi_seq == 0)
		{
			vbi[5] = 0x72; /* Table ID (0x72 = Premiere / Canal+ Old, 0x48 = Clear, 0x7A or FA = Free access?) */
			vbi[6] = ((_block_sequence[s->block_seq] >> 4) + s->block_seq2) & 0x07;
			vbi[7] = (_block_sequence[s->block_seq] << 4) | _block_sequence[s->block_seq + 1];
			
			s->block_seq += 2;
			if(s->block_seq == sizeof(_block_sequence))
			{
				s->block_seq = 0;
				
				if(++s->block_seq2 == 8)
				{
					s->block_seq2 = 0;
				}
			}
			
			/* Control word */
			vbi[ 8] = 0x00;
			vbi[ 9] = 0x00;
			vbi[10] = 0x00;
			vbi[11] = 0x00;
			vbi[12] = 0x00;
			vbi[13] = 0x00;
			vbi[14] = 0x00;
			vbi[15] = 0x00;
			
			/* Fill the remainder with random data */
			for(i = 16; i < 26; i++)
			{
				vbi[i] = _filler[i & 0x07];
			}
		}
		else
		{
			/* Fill remaining parts of the segment with random data */
			for(i = 5; i < 26; i++)
			{
				vbi[i] = _filler[i & 0x07];
			}
		}
		
		if(++s->vbi_seq == 10)
		{
			s->vbi_seq = 0;
		}
		
		/* Calculate and apply the CRC */
		crc = _crc(&vbi[4], 22);
		vbi[26] = (crc & 0x00FF) >> 0;
		vbi[27] = (crc & 0xFF00) >> 8;
		
		/* Render the line */
		vbidata_render_nrz(s->lut, vbi, -48, NG_VBI_BYTES * 8, VBIDATA_LSB_FIRST, s->vid->output, 2);
	}
}

