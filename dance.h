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

#ifndef _DANCE_H
#define _DANCE_H

#include <stdint.h>
#include "common.h"

/* DANCE bit and symbol rates */
#define DANCE_BIT_RATE    2048000
#define DANCE_SYMBOL_RATE (DANCE_BIT_RATE / 2)

/* Modes of operation */
#define DANCE_MODE_A 0x00 /* 4x 32kHz 14/10-bit companded channels */
#define DANCE_MODE_B 0x01 /* 2x 48kHz 16-bit linear channel */

/* Channel modes */
#define DANCE_MODE_STEREO 0x00
#define DANCE_MODE_2_MONO 0x01
#define DANCE_MODE_1_MONO 0x02
#define DANCE_MODE_NONE   0x03

/* Audio sample rates for DANCE */
#define DANCE_A_AUDIO_RATE 32000
#define DANCE_B_AUDIO_RATE 48000

/* Length of a DANCE frame in bits, bytes and symbols */
#define DANCE_FRAME_BITS  2048
#define DANCE_FRAME_BYTES (DANCE_FRAME_BITS / 8)
#define DANCE_FRAME_SYMS  (DANCE_FRAME_BITS / 2)

/* Length of a DANCE frame in audio samples */
#define DANCE_A_AUDIO_LEN (DANCE_A_AUDIO_RATE / 1000)
#define DANCE_B_AUDIO_LEN (DANCE_B_AUDIO_RATE / 1000)
#define DANCE_AUDIO_LEN   DANCE_B_AUDIO_LEN

#define DANCE_A_50_10_US_NTAPS 77
#define DANCE_B_50_10_US_NTAPS 59
#define DANCE_50_10_US_NTAPS   DANCE_A_50_10_US_NTAPS

typedef struct {
	int p;
	int16_t buf[DANCE_50_10_US_NTAPS];
	const int16_t *taps;
	int ntaps;
} _dance_fir_t;

typedef struct {
	
	uint8_t mode_12;
	uint8_t mode_34;
	unsigned int frame;
	uint8_t prn[DANCE_FRAME_BYTES];
	uint8_t frames[2][DANCE_FRAME_BYTES];
	
	/* FIR filters */
	const int16_t *fir_taps;
	int fir_ntaps;
	_dance_fir_t fir[4];
	
} dance_enc_t;

typedef struct {
	
	dance_enc_t enc;
	
	int16_t audio[DANCE_AUDIO_LEN * 2];
	
	int ntaps;
	int16_t *taps;
	int16_t *hist;
	
	int dsym; /* Differential symbol */
	
	cint16_t *bb;
	cint16_t *bb_start;
	cint16_t *bb_end;
	int bb_len;
	
	int sps;
	int ds;
	int dsl;
	int decimation;
	
	cint16_t *cc;
	cint16_t *cc_start;
	cint16_t *cc_end;
	
	uint8_t frame[DANCE_FRAME_BYTES];
	int frame_bit;
	
} dance_mod_t;

extern void dance_encode_init(dance_enc_t *s);
extern void dance_encode_frame_a(
	dance_enc_t *s, uint8_t *frame,
	const int16_t *a1, int a1step,
	const int16_t *a2, int a2step,
	const int16_t *a3, int a3step,
	const int16_t *a4, int a4step
);
extern void dance_encode_frame_b(
	dance_enc_t *s, uint8_t *frame,
	const int16_t *a1, int a1step,
	const int16_t *a2, int a2step
);

extern int dance_mod_init(dance_mod_t *s, uint8_t mode, unsigned int sample_rate, unsigned int frequency, double beta, double level);
extern void dance_mod_input(dance_mod_t *s, const int16_t *audio);
extern int dance_mod_output(dance_mod_t *s, int16_t *iq, size_t samples);
extern int dance_mod_free(dance_mod_t *s);

#endif

