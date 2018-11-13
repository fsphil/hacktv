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

#ifndef _NICAM_H
#define _NICAM_H

#include <stdint.h>
#include "common.h"

/* NICAM bit and symbol rates */
#define NICAM_BIT_RATE    728000
#define NICAM_SYMBOL_RATE (NICAM_BIT_RATE / 2)

/* Audio sample rate for NICAM */
#define NICAM_AUDIO_RATE 32000

/* Length of a NICAM frame in bits, bytes and symbols */
#define NICAM_FRAME_BITS  728
#define NICAM_FRAME_BYTES (NICAM_FRAME_BITS / 8)
#define NICAM_FRAME_SYMS  (NICAM_FRAME_BITS / 2)

/* Length of a NICAM frame in audio samples */
#define NICAM_AUDIO_LEN (NICAM_AUDIO_RATE / 1000)

/* Frame alignment word (0b01001110) */
#define NICAM_FAW 0x4E

/* Modes of operation */
#define NICAM_MODE_STEREO    0x00 /* Single stereo audio signal */
#define NICAM_MODE_DUAL_MONO 0x02 /* Two independent mono audio signals */
#define NICAM_MODE_MONO_DATA 0x04 /* One mono audio signal and one data channel */
#define NICAM_MODE_DATA      0x06 /* One data channel */

/* Taps in J.17 pre-emphasis filter */
#define _J17_NTAPS 83

typedef struct {
	
	uint8_t mode;
	uint8_t reserve;
	
	unsigned int frame;
	
	uint8_t prn[NICAM_FRAME_BYTES - 1];
	
	int fir_p;
	int16_t fir_l[_J17_NTAPS];
	int16_t fir_r[_J17_NTAPS];
	
} nicam_enc_t;

typedef struct {
	
	nicam_enc_t enc;
	
	int16_t audio[NICAM_AUDIO_LEN * 2];
	
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
	
	uint8_t frame[NICAM_FRAME_BYTES];
	int frame_bit;
	
} nicam_mod_t;

extern void nicam_encode_init(nicam_enc_t *s, uint8_t mode, uint8_t reserve);
extern void nicam_encode_frame(nicam_enc_t *s, uint8_t frame[NICAM_FRAME_BYTES], const int16_t audio[NICAM_AUDIO_LEN * 2]);

extern int nicam_mod_init(nicam_mod_t *s, uint8_t mode, uint8_t reserve, unsigned int sample_rate, unsigned int frequency, double beta, double level);
extern void nicam_mod_input(nicam_mod_t *s, const int16_t audio[NICAM_AUDIO_LEN * 2]);
extern int nicam_mod_output(nicam_mod_t *s, int16_t *iq, size_t samples);
extern int nicam_mod_free(nicam_mod_t *s);

#endif

