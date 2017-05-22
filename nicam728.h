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

/* Audio sample rate for NICAM */
#define NICAM_AUDIO_RATE 32000

/* Length of a NICAM frame in bits */
#define NICAM_FRAME_LEN 728

/* Length of a NICAM frame in bytes */
#define NICAM_FRAME_BYTES (NICAM_FRAME_LEN / 8)

/* Length of a NICAM frame in audio samples */
#define NICAM_AUDIO_LEN (NICAM_AUDIO_RATE / 1000)

/* Frame alignment word (0b01001110) */
#define NICAM_FAW 0x4E

/* Modes of operation */
#define NICAM_MODE_STEREO    0x00 /* Single stereo audio signal */
#define NICAM_MODE_DUAL_MONO 0x02 /* Two independent mono audio signals */
#define NICAM_MODE_MONO_DATA 0x04 /* One mono audio signal and one data channel */
#define NICAM_MODE_DATA      0x06 /* One data channel */

typedef struct {
	
	uint8_t mode;
	uint8_t reserve;
	
	unsigned int frame;
	
	uint8_t prn[NICAM_FRAME_BYTES - 1];
	
} nicam_enc_t;

extern void nicam_encode_init(nicam_enc_t *s, uint8_t mode, uint8_t reserve);
extern void nicam_encode_frame(nicam_enc_t *s, uint8_t frame[NICAM_FRAME_BYTES], int16_t audio[NICAM_AUDIO_LEN * 2]);

#endif

