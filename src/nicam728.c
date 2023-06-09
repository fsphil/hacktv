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
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "nicam728.h"

/* Pre-calculated J.17 pre-emphasis filter taps, 32kHz sample rate */
static const int32_t _j17_taps[_J17_NTAPS] = {
	-1, 0, -1, -1, -1, -1, -1, -1, -1, -1, -2, -2, -3, -3, -3, -3, -5, -5,
	-6, -7, -9, -10, -13, -14, -18, -21, -27, -32, -42, -51, -69, -86, -120,
	-159, -233, -332, -524, -814, -1402, -2372, -4502, 25590, -4502, -2372,
	-1402, -814, -524, -332, -233, -159, -120, -86, -69, -51, -42, -32, -27,
	-21, -18, -14, -13, -10, -9, -7, -6, -5, -5, -3, -3, -3, -3, -2, -2, -1,
	-1, -1, -1, -1, -1, -1, -1, 0, -1
};

/* RF symbols */
static const int _step[4] = { 0, 3, 1, 2 };
static const int _syms[4] = { 0, 1, 3, 2 };

/* NICAM scaling factors */

typedef struct {
	int factor;
	int shift;
	int coding_range;
	int protection_range;
} _scale_factor_t;

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

void _process_audio(nicam_enc_t *s, int16_t dst[NICAM_AUDIO_LEN * 2], const int16_t src[NICAM_AUDIO_LEN * 2])
{
	const _scale_factor_t *scale[2];
	int32_t l, r;
	int x, xi;
	
	/* Apply J.17 pre-emphasis filter */
	for(x = 0; x < NICAM_AUDIO_LEN; x++)
	{
		s->fir_l[s->fir_p] = src ? src[x * 2 + 0] : 0;
		s->fir_r[s->fir_p] = src ? src[x * 2 + 1] : 0;
		if(++s->fir_p == _J17_NTAPS) s->fir_p = 0;
		
		for(l = r = xi = 0; xi < _J17_NTAPS; xi++)
		{
			l += (int32_t) s->fir_l[s->fir_p] * _j17_taps[xi];
			r += (int32_t) s->fir_r[s->fir_p] * _j17_taps[xi];
			if(++s->fir_p == _J17_NTAPS) s->fir_p = 0;
		}
		
		dst[x * 2 + 0] = l >> 15;
		dst[x * 2 + 1] = r >> 15;
	}
	
	/* Calculate the scale factors for each channel */
	scale[0] = _scale_factor(dst + 0, 2);
	scale[1] = _scale_factor(dst + 1, 2);
	
	/* Scale and append each sample to the frame */
	for(xi = x = 0; x < NICAM_AUDIO_LEN * 2; x++)
	{
		/* Shift down the selected range */
		dst[x] = (dst[x] >> scale[x & 1]->shift) & 0x3FF;
		
		/* Add the parity bit (6 MSBs only) */
		dst[x] |= _parity(dst[x] >> 4) << 10;
		
		/* Add scale-factor code if necessary */
		if(x < 54)
		{
			dst[x] ^= ((scale[x & 1]->factor >> (2 - (x / 2 % 3))) & 1) << 10;
		}
	}
}

void nicam_encode_init(nicam_enc_t *s, uint8_t mode, uint8_t reserve)
{
	memset(s, 0, sizeof(nicam_enc_t));
	
	s->mode = mode;
	s->reserve = reserve;
	
	_prn(s->prn);
}

void nicam_encode_frame(nicam_enc_t *s, uint8_t frame[NICAM_FRAME_BYTES], const int16_t audio[NICAM_AUDIO_LEN * 2])
{
	int16_t j17_audio[NICAM_AUDIO_LEN * 2];
	int x, xi;
	
	/* Encode the audio */
	_process_audio(s, j17_audio, audio);
	
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
	
	/* Pack the encoded audio into the frame */
	for(xi = x = 0; x < NICAM_AUDIO_LEN * 2; x++)
	{
		int b;
		
		for(b = 0; b < 11; b++, j17_audio[x] >>= 1)
		{
			/* Apply the bit to the interleaved location */
			if(j17_audio[x] & 1)
			{
				frame[3 + (xi / 8)] |= 1 << (7 - (xi % 8));
			}
			
			/* Calculate next interleaved bit location */
			xi += 16;
			if(xi >= NICAM_FRAME_BITS - 24)
			{
				xi -= NICAM_FRAME_BITS - 24 - 1;
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

void nicam_encode_mac_packet(nicam_enc_t *s, uint8_t pkt[91], const int16_t audio[NICAM_AUDIO_LEN * 2])
{
	/* Creates a 90 byte companded sound coding block, first level protection */
	int16_t j17[NICAM_AUDIO_LEN * 2];
	int i, x;
	
	/* Encode the audio */
	_process_audio(s, j17, audio);
	
	/* PT Packet Type */
	pkt[0] = 0xC7;
	
	/* Unallocated */
	pkt[1] = 0x00;
	pkt[2] = 0x00;
	
	/* Pack the 11-bit compressed samples into the packet */
	for(x = 3, i = 0; i < NICAM_AUDIO_LEN * 2; i += 8, x += 11)
	{
		pkt[x +  0] =                     (j17[i + 0] >> 0);
		pkt[x +  1] = (j17[i + 1] << 3) | (j17[i + 0] >> 8);
		pkt[x +  2] = (j17[i + 2] << 6) | (j17[i + 1] >> 5);
		pkt[x +  3] =                     (j17[i + 2] >> 2);
		pkt[x +  4] = (j17[i + 3] << 1) | (j17[i + 2] >> 10);
		pkt[x +  5] = (j17[i + 4] << 4) | (j17[i + 3] >> 7);
		pkt[x +  6] = (j17[i + 5] << 7) | (j17[i + 4] >> 4);
		pkt[x +  7] =                     (j17[i + 5] >> 1);
		pkt[x +  8] = (j17[i + 6] << 2) | (j17[i + 5] >> 9);
		pkt[x +  9] = (j17[i + 7] << 5) | (j17[i + 6] >> 6);
		pkt[x + 10] =                     (j17[i + 7] >> 3);
	}
	
	/* Increment the frame counter (not used for MAC) */
	s->frame++;
}



static double _hamming(double x)
{
	if(x < -1 || x > 1) return(0);
	return(0.54 - 0.46 * cos((M_PI * (1.0 + x))));
}

static double _rrc(double x, double b, double t)
{
	double r;
	
	/* Based on the Wikipedia page, https://en.wikipedia.org/w/index.php?title=Root-raised-cosine_filter&oldid=787851747 */
	
	if(x == 0)
	{
		r = (1.0 / t) * (1.0 + b * (4.0 / M_PI - 1));
	}
	else if(fabs(x) == t / (4.0 * b))
	{
		r = b / (t * sqrt(2.0)) * ((1.0 + 2.0 / M_PI) * sin(M_PI / (4.0 * b)) + (1.0 - 2.0 / M_PI) * cos(M_PI / (4.0 * b)));
	}
	else
	{
		double t1 = (4.0 * b * (x / t));
		double t2 = (sin(M_PI * (x / t) * (1.0 - b)) + 4.0 * b * (x / t) * cos(M_PI * (x / t) * (1.0 + b)));
		double t3 = (M_PI * (x / t) * (1.0 - t1 * t1));
		
		r = (1.0 / t) * (t2 / t3);
	}
	
	return(r);
}

int nicam_mod_init(nicam_mod_t *s, uint8_t mode, uint8_t reserve, unsigned int sample_rate, unsigned int frequency, double beta, double level)
{
	double sps;
	double t;
	double r;
	int x, n;
	
	memset(s, 0, sizeof(nicam_mod_t));
	
	/* Samples per symbol */
	sps = (double) sample_rate / 364000.0;
	
	/* Calculate the number of taps needed to cover 5 symbols, rounded up to odd number */
	s->ntaps = ((unsigned int) (sps * 5) + 1) | 1;
	
	s->taps = malloc(sizeof(int16_t) * s->ntaps);
	if(!s->taps)
	{
		return(-1);
	}
	
	/* Generate the filter taps */
	n = s->ntaps / 2;
	for(x = -n; x <= n; x++)
	{
		t = ((double) x) / sps;
		
		r  = _rrc(t, beta, 1.0) * _hamming((double) x / n);
		r *= M_SQRT1_2 * INT16_MAX * level;
		
		s->taps[x + n] = lround(r);
	}
	
	/* Allocate memory for the baseband buffer */
	s->bb_start = calloc(s->ntaps, sizeof(cint16_t));
	s->bb_end   = s->bb_start + s->ntaps;
	s->bb       = s->bb_start;
	s->bb_len   = 0;
	
	if(!s->bb_start)
	{
		return(-1);
	}
	
	/* Setup values for the sample rate error correction */
	n = gcd(sample_rate, NICAM_SYMBOL_RATE);
	
	s->decimation = NICAM_SYMBOL_RATE / n;
	s->sps = (sample_rate + NICAM_SYMBOL_RATE - 1) / NICAM_SYMBOL_RATE;
	s->dsl = (s->sps * s->decimation) % (sample_rate / n);
	s->ds  = 0;
	
	/* Setup the mixer signal */
	n = gcd(sample_rate, frequency);
	x = sample_rate / n;
	s->cc_start = sin_cint16(x, frequency / n, 1.0);
	s->cc_end   = s->cc_start + x;
	s->cc       = s->cc_start;
	
	if(!s->cc)
	{
		return(-1);
	}
	
	/* Initialise the encoder */
	nicam_encode_init(&s->enc, mode, reserve);
	s->frame_bit = NICAM_FRAME_BITS;
	
	return(0);
}

int nicam_mod_free(nicam_mod_t *s)
{
	free(s->cc_start);
	free(s->bb_start);
	free(s->taps);
	
	return(0);
}

void nicam_mod_input(nicam_mod_t *s, const int16_t audio[NICAM_AUDIO_LEN * 2])
{
	memcpy(s->audio, audio, sizeof(int16_t) * NICAM_AUDIO_LEN * 2);
}

int nicam_mod_output(nicam_mod_t *s, int16_t *iq, size_t samples)
{
	cint16_t *ciq = (cint16_t *) iq;
	int x, i;
	int16_t r;
	
	for(x = 0; x < samples;)
	{
		/* Output and clear the buffer */
		for(; x < samples && s->bb_len; x++, s->bb_len--)
		{
			cint16_mula(ciq++, s->bb, s->cc);
			
			s->bb->i = 0;
			s->bb->q = 0;
			
			if(++s->bb == s->bb_end)
			{
				s->bb = s->bb_start;
			}
			
			if(++s->cc == s->cc_end)
			{
				s->cc = s->cc_start;
			}
		}
		
		if(s->bb_len > 0)
		{
			break;
		}
		
		if(s->frame_bit == NICAM_FRAME_BITS)
		{
			/* Encode the next frame */
			nicam_encode_frame(&s->enc, s->frame, s->audio);
			s->frame_bit = 0;
		}
		
		/* Read out the next 2-bit symbol, USB first */
		s->dsym += _step[(s->frame[s->frame_bit >> 3] >> (6 - (s->frame_bit & 0x07))) & 0x03];
		s->dsym &= 0x03;
		s->frame_bit += 2;
		
		/* Encode the symbol */
		for(i = 0; i < s->ntaps; i++)
		{
			r = s->taps[i];
			s->bb->i += (_syms[s->dsym] & 1 ? r : -r);
			s->bb->q += (_syms[s->dsym] & 2 ? r : -r);
			
			if(++s->bb == s->bb_end)
			{
				s->bb = s->bb_start;
			}
		}
		
		/* Calculate length of the next block */
		s->bb_len = s->sps;
		
		s->ds += s->dsl;
		if(s->ds >= s->decimation)
		{
			s->bb_len--;
			s->ds -= s->decimation;
		}
	}
	
	return(0);
}

