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

/* NICAM-728 stereo encoder
 * 
 * Based on the BBC RD document "NICAM 728 - DIGITAL
 * TWO-CHANNEL STEREO FOR TERRESTRIAL TELEVISION";
 * http://downloads.bbc.co.uk/rd/pubs/reports/1990-06.pdf
 * 
 * http://www.etsi.org/deliver/etsi_en/300100_300199/300163/01.02.01_60/en_300163v010201p.pdf
 * 
 * NICAM was designed for 14-bit PCM samples, but for
 * simplicity this encoder expects 16-bit samples.
*/

#include <stdint.h>
#include "nicam728.h"

typedef struct {
	int factor;
	int shift;
	int coding_range;
	int protection_range;
} _scale_factor_t;

/* A list of the scaling factors and their parameters */
static const _scale_factor_t _scale_factors[8] = {
	{ 0, 2, 5, 7 }, /* 0b000 */
	{ 1, 2, 5, 7 }, /* 0b001 */
	{ 2, 2, 5, 6 }, /* 0b010 */
	{ 4, 2, 5, 5 }, /* 0b100 */
	{ 3, 3, 4, 4 }, /* 0b011 */
	{ 5, 4, 3, 3 }, /* 0b101 */
	{ 6, 5, 2, 2 }, /* 0b110 */
	{ 7, 6, 1, 1 }, /* 0b111 */
};

static const _scale_factor_t *_scale_factor(int16_t *pcm, int step)
{
	int i, b;
	int16_t s;
	
	/* Calculate the optimal scale factor for this audio block */
	b = 1;
	
	/* Test each sample if it requires a larger range */
	for(i = 0; b < 7 && i < NICAM_AUDIO_LEN; i++)
	{
		/* Negative values use the same scales */
		s = (*pcm < 0) ? ~*pcm : *pcm;
		
		/* Test if the scale factor needs to be increased */
		while(b < 7 && s >> (b + 8))
		{
			b++;
		}
		
		pcm += step;
	}
	
	return(&_scale_factors[b]);
}

static void _prn(uint8_t prn[NICAM_FRAME_BYTES - 1])
{
	/* Generate the full PRN sequence for a NICAM-728 packet
	 * First 20 bits of the sequence should be:
	 * 0000 0111 1011 1110 0010 ....
	 * 07 BE 2. ...
	*/
	
	int poly = 0x1FF;
	int x, i;
	
	for(x = 0; x < NICAM_FRAME_BYTES - 1; x++)
	{
		prn[x] = 0x00;
		
		for(i = 0; i < 8; i++)
		{
			uint8_t b;
			
			b = poly & 1;
			b ^= (poly >> 4) & 1;
			
			poly >>= 1;
			poly |= b << 8;
			
			prn[x] <<= 1;
			prn[x] |= b;
		}
	}
}

static uint8_t _parity(unsigned int value)
{
	uint8_t p = 0;
	
	while(value)
	{
		p ^= value & 1;
		value >>= 1;
	}
	
	return(p);
}

void nicam_encode_init(nicam_enc_t *s, uint8_t mode, uint8_t reserve)
{
	s->mode = mode;
	s->reserve = reserve;
	s->frame = 0;
	_prn(s->prn);
}

void nicam_encode_frame(nicam_enc_t *s, uint8_t frame[NICAM_FRAME_BYTES], int16_t audio[NICAM_AUDIO_LEN * 2])
{
	const _scale_factor_t *scale[2];
	int x, xi;
	
	/* Initialise the NICAM frame header with the FAW (Frame Alignment Word) */
	frame[0] = NICAM_FAW;
	
	/* Set the application control bits */
	frame[1]  = (((~s->frame) >> 3) & 1) << 7; /* C0 frame flag-bit. Toggled every 8 frames */
	frame[1] |= ((s->mode >> 2) & 1)     << 6; /* C1 */
	frame[1] |= ((s->mode >> 1) & 1)     << 5; /* C2 */
	frame[1] |= ((s->mode >> 0) & 1)     << 4; /* C3 */
	frame[1] |= (s->reserve & 1)         << 3; /* C4 reserve sound switching flag */
	
	/* The additional bits AD0-AD10 and audio are all zero */
	for(x = 2; x < NICAM_FRAME_BYTES; x++)
	{
		frame[x] = 0;
	}
	
	/* Calculate the scale factors for each channel */
	scale[0] = _scale_factor(audio + 0, 2);
	scale[1] = _scale_factor(audio + 1, 2);
	
	/* Scale and append each sample to the frame */
	for(xi = x = 0; x < NICAM_AUDIO_LEN * 2; x++)
	{
		int16_t a;
		int b;
		
		/* Shift down the selected range */
		a = (audio[x] >> scale[x & 1]->shift) & 0x3FF;
		
		/* Add the parity bit (6 MSBs only) */
		a |= _parity(a >> 4) << 10;
		
		/* Add scale-factor code if necessary */
		if(x < 54)
		{
			a ^= ((scale[x & 1]->factor >> (2 - (x / 2 % 3))) & 1) << 10;
		}
		
		/* Pack the compressed samples into the frame */
		for(b = 0; b < 11; b++, a >>= 1)
		{
			/* Apply the bit to the interleaved location */
			if(a & 1)
			{
				frame[3 + (xi / 8)] |= 1 << (7 - (xi % 8));
			}
			
			/* Calculate next interleaved bit location */
			xi += 16;
			if(xi >= NICAM_FRAME_LEN - 24)
			{
				xi -= NICAM_FRAME_LEN - 24 - 1;
			}
		}
	}
	
	/* Apply the PRN */
	for(x = 0; x < NICAM_FRAME_BYTES - 1; x++)
	{
		frame[x + 1] ^= s->prn[x];
	}
	
	/* Increment the frame counter */
	s->frame++;
}

