/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2023 Philip Heron <phil@sanslogic.co.uk>                    */
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

#ifndef _AV_H
#define _AV_H

#include <stdint.h>
#include "common.h"

/* Return codes */
#define AV_OK             0
#define AV_ERROR         -1
#define AV_OUT_OF_MEMORY -2

typedef struct {
	
	/* Dimensions */
	int width;
	int height;
	
	/* 32-bit RGBx framebuffer */
	uint32_t *framebuffer;
	int pixel_stride;
	int line_stride;
	
	/* The image aspect ratio */
	float ratio;
	
	/* Interlace flag */
	int interlaced;
	
} av_frame_t;

extern const av_frame_t av_frame_default;

typedef int (*av_read_video_t)(void *ctx, av_frame_t *frame);
typedef int16_t *(*av_read_audio_t)(void *ctx, size_t *samples);
typedef int (*av_eof_t)(void *ctx);
typedef int (*av_close_t)(void *ctx);

typedef struct {
	
	/* Video settings */
	int width;
	int height;
	rational_t frame_rate;
	av_frame_t default_frame;
	
	/* Video state */
	unsigned int frames;
	
	/* Audio settings */
	rational_t sample_rate;
	
	/* Audio state */
	unsigned int samples;
	
	/* AV source data and callbacks */
	void *av_source_ctx;
	av_read_video_t read_video;
	av_read_audio_t read_audio;
	av_eof_t eof;
	av_close_t close;
	
} av_t;

extern int av_read_video(av_t *s, av_frame_t *frame);
extern int16_t *av_read_audio(av_t *s, size_t *samples);
extern int av_eof(av_t *s);
extern int av_close(av_t *s);

extern void av_hflip_frame(av_frame_t *frame);
extern void av_vflip_frame(av_frame_t *frame);
extern void av_rotate_frame(av_frame_t *frame, int a);

#include "av_test.h"
#include "av_ffmpeg.h"

#endif

