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

#ifndef _HACKTV_H
#define _HACKTV_H

#include <stdint.h>
#include "video.h"
#include "rf.h"

/* Return codes */
#define HACKTV_OK             0
#define HACKTV_ERROR         -1
#define HACKTV_OUT_OF_MEMORY -2

/* Standard audio sample rate */
#define HACKTV_AUDIO_SAMPLE_RATE 32000

/* Program state */
typedef struct {
	
	/* Configuration */
	char *output_type;
	char *output;
	char *mode;
	int samplerate;
	int pixelrate;
	float level;
	float deviation;
	float gamma;
	int interlace;
	av_fit_mode_t fit_mode;
	rational_t min_aspect;
	rational_t max_aspect;
	int repeat;
	int shuffle;
	int verbose;
	char *teletext;
	char *wss;
	char *videocrypt;
	char *videocrypt2;
	char *videocrypts;
	int syster;
	int systeraudio;
	char *eurocrypt;
	int acp;
	int vits;
	int vitc;
	int filter;
	int nocolour;
	int noaudio;
	int nonicam;
	int a2stereo;
	int scramble_video;
	int scramble_audio;
	uint64_t frequency;
	int amp;
	int gain;
	char *antenna;
	int file_type;
	int chid;
	int mac_audio_stereo;
	int mac_audio_quality;
	int mac_audio_protection;
	int mac_audio_companded;
	char *sis;
	int swap_iq;
	int64_t offset;
	char *passthru;
	int invert_video;
	char *raw_bb_file;
	int16_t raw_bb_blanking_level;
	int16_t raw_bb_white_level;
	int secam_field_id;
	int list_modes;
	int json;
	char *ffmt;
	char *fopts;
	
	/* Video encoder state */
	vid_t vid;
	
	/* RF sink interface */
	rf_t rf;
	
} hacktv_t;

#endif

