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

/* Return codes */
#define HACKTV_OK             0
#define HACKTV_ERROR         -1
#define HACKTV_OUT_OF_MEMORY -2

/* Integer types */
#define HACKTV_INT16_COMPLEX 0
#define HACKTV_INT16_REAL    1

/* File output types */
#define HACKTV_UINT8  0
#define HACKTV_INT8   1
#define HACKTV_UINT16 2
#define HACKTV_INT16  3
#define HACKTV_INT32  4
#define HACKTV_FLOAT  5 /* 32-bit float */

/* Standard audio sample rate */
#define HACKTV_AUDIO_SAMPLE_RATE 32000

/* AV source function prototypes */
typedef uint32_t *(*hacktv_av_read_video_t)(void *private, float *ratio);
typedef int16_t *(*hacktv_av_read_audio_t)(void *private, size_t *samples);
typedef int (*hacktv_av_close_t)(void *private);

/* RF output function prototypes */
typedef int (*hacktv_rf_write_t)(void *private, int16_t *iq_data, size_t samples);
typedef int (*hacktv_rf_close_t)(void *private);

/* Program state */
typedef struct {
	
	/* Configuration */
	char *output_type;
	char *output;
	char *mode;
	int samplerate;
	float level;
	float deviation;
	float gamma;
	int interlace;
	int repeat;
	int verbose;
	char *d11;
	char *systercnr;
	char *teletext;
	char *logo;
	char *wss;
	int letterbox;
	int pillarbox;
	char *videocrypt;
	char *videocrypt2;
	char *videocrypts;
	int findkey;
	int showserial;
	char *syster;
	int systeraudio;
	char *eurocrypt;
	int acp;
	int vits;
	int filter;
	int subtitles;
	int txsubtitles;
	int nocolour;
	int noaudio;
	int nonicam;
	int scramble_video;
	int scramble_audio;
	uint64_t frequency;
	int amp;
	int gain;
	char *antenna;
	int file_type;
	int timestamp;
	int position;
	uint32_t enableemm;
	uint32_t disableemm;
	int showecm;
	int chid;
	int64_t offset;
	char *passthru;
	float volume;
	int downmix;
	int fmaudiotest;
	
	/* Video encoder state */
	vid_t vid;
	
	/* RF sink interface */
	void *rf_private;
	hacktv_rf_write_t rf_write;
	hacktv_rf_close_t rf_close;
	
} hacktv_t;

#endif

