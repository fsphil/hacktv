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

#include <stdint.h>
#include <string.h>
#include "spdif.h"

uint32_t spdif_bitrate(uint32_t sample_rate)
{
	return(sample_rate * 128);
}

static void _spdif_subframe(uint8_t out[8], int sample, uint8_t aux, int16_t pcm, uint8_t v, uint8_t u, uint8_t c)
{
	uint32_t sf;
	int i, p;
	
	/* Build the subframe */
	sf  = (aux &    0xF) <<  4; /* 4-bit Aux */
	sf |= (pcm & 0xFFFF) << 12; /* 16-bit PCM */
	sf |= (v   &    0x1) << 28; /* Validity bit */
	sf |= (u   &    0x1) << 29; /* User data bit */
	sf |= (c   &    0x1) << 30; /* Channel status bit */
	
	/* Calculate the parity bit */
	for(i = 0; i < 31; i++)
	{
		sf ^= ((sf >> i) & 1) << 31;
	}
	
	/* Generate biphase bitstream (MSB first) */
	memset(out, 0, 8);
	out[0] = (sample & 1 ? 0xE4 : (sample ? 0xE2 : 0xE8));
	for(p = 1, i = 4; i < 32; i++)
	{
		out[i >> 2] |= p << (7 - ((i & 3) << 1));
		p ^= (sf >> i) & 1;
		
		out[i >> 2] |= p << (6 - ((i & 3) << 1));
		p ^= 1;
	}
}

void spdif_block(uint8_t b[SPDIF_BLOCK_BYTES], const int16_t pcm[SPDIF_BLOCK_SAMPLES])
{
	uint8_t cs[24];
	int i;
	
	memset(cs, 0, 24);
	cs[0] |= 0 << 7; /* Consumer (S/PDIF) */
	cs[0] |= 0 << 6; /* Normal */
	cs[0] |= 1 << 5; /* Copy permit */
	cs[0] |= 0 << 4; /* 2 channels */
	cs[0] |= 0 << 2; /* No pre-emphasis */
	
	for(i = 0; i < SPDIF_BLOCK_SAMPLES; i++, b += 8)
	{
		_spdif_subframe(
			b,
			i,
			0,
			*(pcm++),
			0,
			0,
			(cs[i >> 4] >> (7 - ((i >> 1) & 7))) & 1
		);
	}
}

