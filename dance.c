/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2020 Philip Heron <phil@sanslogic.co.uk>                    */
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

/* DANCE audio encoder */

#include <stdio.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "dance.h"

/* Pre-calculated 50/10 μs pre-emphasis filter taps, 32kHz sample rate */
static const int16_t _50_10_us_a_taps[DANCE_A_50_10_US_NTAPS] = {
	1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 2, -2, 2, -2, 2,
	-3, 3, -3, 4, -5, 5, -6, 7, -10, 10, -19, 11, -55, -24, -298, -635,
	-4106, 20126, -4106, -635, -298, -24, -55, 11, -19, 10, -10, 7, -6, 5,
	-5, 4, -3, 3, -3, 2, -2, 2, -2, 2, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1,
	-1, 1, -1, 1, -1, 1
};

/* Pre-calculated 50/10 μs pre-emphasis filter taps, 48kHz sample rate */
static const int16_t _50_10_us_b_taps[DANCE_B_50_10_US_NTAPS] = {
	-1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -2, 2, -2, 2, -3, 2, -6, 1,
	-12, -5, -32, -34, -115, -193, -583, -1324, -4359, 23207, -4359, -1324,
	-583, -193, -115, -34, -32, -5, -12, 1, -6, 2, -3, 2, -2, 2, -2, 1, -1,
	1, -1, 1, -1, 1, -1, 1, -1, 1, -1
};

/* RF symbols */
static const int _step[4] = { 0, 3, 1, 2 };
static const int _syms[4] = { 0, 1, 3, 2 };

/* Ranges */
typedef struct {
	uint16_t mask;
	uint8_t pattern;
	int shift;
} _comp_range_t;

static const _comp_range_t _ranges[8] = {
	{ 0x8000, 0x00, 6 },
	{ 0xC000, 0x9C, 5 },
	{ 0xE000, 0x4E, 4 },
	{ 0xF000, 0xD2, 3 },
	{ 0xF800, 0x3A, 2 },
	{ 0xFC00, 0xA6, 2 },
	{ 0xFE00, 0x74, 2 },
	{ 0xFF00, 0xE8, 2 },
};

static void _prn(uint8_t prn[DANCE_FRAME_BYTES])
{
	/* Generate the full PRN sequence for a DANCE frame */
	uint16_t poly = 0x3FF;
	int x, i;
	uint8_t b;
	
	memset(prn, 0, DANCE_FRAME_BYTES);
	
	for(x = 2; x < DANCE_FRAME_BYTES; x++)
	{
		for(i = 0; i < 8; i++)
		{
			b = poly & 1;
			prn[x] = (prn[x] << 1) | b;
			b ^= (poly >> 3) & 1;
			poly = (poly >> 1) | (b << 9);
		}
	}
}

static void _interleave(uint8_t d[DANCE_FRAME_BYTES])
{
	uint8_t tmp[DANCE_FRAME_BYTES - 4];
	int x, y;
	
	memset(tmp, 0, DANCE_FRAME_BYTES - 4);
	d += 4;
	
	for(x = y = 0; x < DANCE_FRAME_BITS - 32; x++)
	{
		if((d[y >> 3] >> (7 - (y & 7))) & 1) tmp[x >> 3] |= 1 << (7 - (x & 7));
		if((y += 63) >= 2016) y -= 2015;
	}
	
	memcpy(d, tmp, DANCE_FRAME_BYTES - 4);
}

static const _comp_range_t *_find_range(const int16_t *pcm, int len, int step)
{
	int b;
	int16_t s;
	
	b = 7;
	
	while(b > 0 && len > 0)
	{
		s = (*pcm < 0) ? ~*pcm : *pcm;
		
		if(s & _ranges[b].mask) b--;
		else { pcm += step; len--; }
	}
	
	return(&_ranges[b]);
}

static void _pre_emphasis(_dance_fir_t *fir, int16_t *dst, const int16_t *src, int step, int len)
{
	int32_t l;
	int x, xi;
	
	/* Apply pre-emphasis */
	for(x = 0; x < len; x++)
	{
		fir->buf[fir->p] = src ? *src : 0;
		if(++fir->p == fir->ntaps) fir->p = 0;
		
		for(l = xi = 0; xi < fir->ntaps; xi++)
		{
			l += (int32_t) fir->buf[fir->p] * fir->taps[xi];
			if(++fir->p == fir->ntaps) fir->p = 0;
		}
		
		*(dst++) = l >> 15;
		src += step;
	}
}

void dance_encode_init(dance_enc_t *s)
{
	memset(s, 0, sizeof(dance_enc_t));
	
	s->mode_12 = DANCE_MODE_STEREO;
	s->mode_34 = DANCE_MODE_NONE;
	
	_prn(s->prn);
}

/* Pack bits into buffer LSB first */
static size_t _bits(uint8_t *data, size_t offset, uint64_t bits, size_t nbits)
{
	uint8_t b;
	
	for(; nbits; nbits--, offset++, bits >>= 1)
	{
		b = 1 << (7 - (offset & 7));
		if(bits & 1) data[offset >> 3] |= b;
		else data[offset >> 3] &= ~b;
	}
	
	return(offset);
}

/* Pack bits into buffer MSB first */
static size_t _rbits(uint8_t *data, size_t offset, uint64_t bits, size_t nbits)
{
	uint64_t m = (uint64_t) 1 << (nbits - 1);
	uint8_t b;
	
	for(; nbits; nbits--, offset++, bits <<= 1)
	{
		b = 1 << (7 - (offset & 7));
		if(bits & m) data[offset >> 3] |= b;
		else data[offset >> 3] &= ~b;
	}
	
	return(offset);
}

/* BCH (63,56) */
static size_t _bch_encode(uint8_t *data, size_t offset)
{
	uint16_t code = 0x0000;
	size_t i;
	int b;
	
	for(i = offset; i < offset + 56; i++)
	{
		b = (data[i >> 3] >> (7 - (i & 7))) & 1;
		b = (b ^ code) & 1;
		
		code >>= 1;
		
		if(b) code ^= 0x51;
	}
	
	return(_bits(data, i, code, 63 - 56));
}

void dance_encode_frame_a(
	dance_enc_t *s, uint8_t *frame,
	const int16_t *a1, int a1step,
	const int16_t *a2, int a2step,
	const int16_t *a3, int a3step,
	const int16_t *a4, int a4step)
{
	const int16_t *a[4] = { a1, a2, a3, a4 };
	int step[4] = { a1step, a2step, a3step, a4step };
	int i, x, c;
	const _comp_range_t *r[4];
	int16_t audio[4][DANCE_A_AUDIO_LEN];
	uint8_t *f1, *f2;
	
	/* Get a pointer to the current and next frames */
	f1 = s->frames[s->frame & 1];
	f2 = s->frames[(s->frame + 1) & 1];
	
	/* Create the DANCE frame header */
	f1[0]  = 0x13;
	f1[1]  = 0x5E;
	f1[2]  = DANCE_MODE_A << 7;
	f1[2] |= s->mode_12 << 5;
	f1[2] |= s->mode_34 << 3;
	f1[3]  = 0 << 0; /* Unmuted */
	
	/* Apply pre-emphasis and find the companding range for each channel */
	for(c = 0; c < 4; c++)
	{
		s->fir[c].taps = _50_10_us_a_taps;
		s->fir[c].ntaps = DANCE_A_50_10_US_NTAPS;
		
		_pre_emphasis(&s->fir[c], audio[c], a[c], step[c], DANCE_A_AUDIO_LEN);
		r[c] = _find_range(audio[c], DANCE_A_AUDIO_LEN, 1);
	}
	
	/* Write out the range codes and audio samples */
	for(i = 0; i < 32; i++)
	{
		/* Write out the range codes (one bit at a time) */
		x = _rbits(&f1[4], i * 63, r[i >> 3]->pattern >> (7 - (i & 7)), 1);
		
		/* Write the audio samples (into the next frame) */
		for(c = 0; c < 4; c++)
		{
			x = _rbits(&f2[4], x, audio[c][i] >> r[c]->shift, 10);
		}
		
		/* Write additional data (packets, etc. Not used yet) */
		x = _rbits(&f2[4], x, 0, 15);
		
		/* Apply error correction codes */
		_bch_encode(&f1[4], i * 63);
	}
	
	/* Apply interleave */
	_interleave(f1);
	
	/* Copy completed frame, apply the PRN */
	for(x = 0; x < DANCE_FRAME_BYTES; x++)
	{
		frame[x] = f1[x] ^ s->prn[x];
	}
	
	/* Increment the frame counter */
	s->frame++;
}

void dance_encode_frame_b(
	dance_enc_t *s, uint8_t *frame,
	const int16_t *a1, int a1step,
	const int16_t *a2, int a2step
)
{
	const int16_t *a[2] = { a1, a2 };
	int step[2] = { a1step, a2step };
	int i, x, c, sa;
	const _comp_range_t *r[4];
	int16_t audio[2][DANCE_B_AUDIO_LEN];
	uint8_t *f1, *f2;
	
	/* Get a pointer to the current and next frames */
	f1 = s->frames[s->frame & 1];
	f2 = s->frames[(s->frame + 1) & 1];
	
	/* Create the DANCE frame header */
	f1[0]  = 0x13;
	f1[1]  = 0x5E;
	f1[2]  = DANCE_MODE_B << 7;
	f1[2] |= s->mode_12 << 5;
	f1[2] |= DANCE_MODE_NONE << 3;
	f1[3]  = 0 << 0; /* Unmuted */
	
	/* Apply pre-emphasis and find the companding range for each channel */
	for(c = 0; c < 2; c++)
	{
		s->fir[c].taps = _50_10_us_b_taps;
		s->fir[c].ntaps = DANCE_B_50_10_US_NTAPS;
		
		_pre_emphasis(&s->fir[c], audio[c], a[c], step[c], DANCE_B_AUDIO_LEN);
		r[c] = _find_range(audio[c], DANCE_B_AUDIO_LEN, 1);
	}
	
	/* Channels 3 and 4 are not used in mode B. Set the range codes to zero */
	r[2] = r[3] = &_ranges[0];
	
	/* Write out the range codes and audio samples */
	for(sa = i = 0; i < 32; i++)
	{
		/* Write out the range codes (one bit at a time) */
		x = _rbits(&f1[4], i * 63, r[i >> 3]->pattern >> (7 - (i & 7)), 1);
		
		/* Write the audio samples (into the next frame) */
		for(c = 0; c < 3; c++, sa++)
		{
			x = _rbits(&f2[4], x, audio[sa & 1][sa >> 1], 16);
		}
		
		/* Write additional data (packets, etc. Not used yet) */
		x = _rbits(&f2[4], x, 0, 7);
		
		/* Apply error correction codes */
		_bch_encode(&f1[4], i * 63);
	}
	
	/* Apply interleave */
	_interleave(f1);
	
	/* Copy completed frame, apply the PRN */
	for(x = 0; x < DANCE_FRAME_BYTES; x++)
	{
		frame[x] = f1[x] ^ s->prn[x];
	}
	
	/* Increment the frame counter */
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

int dance_mod_init(dance_mod_t *s, uint8_t mode, unsigned int sample_rate, unsigned int frequency, double beta, double level)
{
	double sps;
	double t;
	double r;
	int x, n;
	
	memset(s, 0, sizeof(dance_mod_t));
	
	/* Samples per symbol */
	sps = (double) sample_rate / DANCE_SYMBOL_RATE;
	
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
	n = gcd(sample_rate, DANCE_SYMBOL_RATE);
	
	s->decimation = DANCE_SYMBOL_RATE / n;
	s->sps = (sample_rate + DANCE_SYMBOL_RATE - 1) / DANCE_SYMBOL_RATE;
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
	dance_encode_init(&s->enc);
	s->frame_bit = DANCE_FRAME_BITS;
	
	return(0);
}

int dance_mod_free(dance_mod_t *s)
{
	free(s->cc_start);
	free(s->bb_start);
	free(s->taps);
	
	return(0);
}

void dance_mod_input(dance_mod_t *s, const int16_t *audio)
{
	memcpy(s->audio, audio, sizeof(int16_t) * DANCE_AUDIO_LEN * 2);
}

int dance_mod_output(dance_mod_t *s, int16_t *iq, size_t samples)
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
		
		if(s->frame_bit == DANCE_FRAME_BITS)
		{
			/* Encode the next frame */
			dance_encode_frame_a(
				&s->enc, s->frame,
				s->audio + 0, 2,
				s->audio + 1, 2,
				NULL, 0, NULL, 0
			);
			s->frame_bit = 0;
		}
		
		/* Read out the next 2-bit symbol, MSB first */
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

