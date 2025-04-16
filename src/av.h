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
#include <pthread.h>
#include "common.h"

/* Return codes */
#define AV_OK             0
#define AV_ERROR         -1
#define AV_OUT_OF_MEMORY -2
#define AV_EOF           -3

typedef struct {
	
	/* Dimensions */
	int width;
	int height;
	
	/* 32-bit RGBx framebuffer */
	uint32_t *framebuffer;
	int pixel_stride;
	int line_stride;
	
	/* The pixel aspect ratio */
	r64_t pixel_aspect_ratio;
	
	/* Interlace flag */
	int interlaced;
	
	/* CC608 subtitle data */
	uint8_t cc608[2];
	
} av_frame_t;



/* AV module callbacks:
 *
 * av_read_video(): Returns AV_OK when a frame is available, or AV_EOF if
 *                  the source has no further video frames.
 *                  Any return code that is not AV_OK is treated as AV_EOF */

typedef int (*av_read_video_t)(void *ctx, av_frame_t *frame);

/* av_read_audio(): Returns AV_OK when audio samples are available, or AV_EOF if
 *                  the source has no further audio samples.
 *                  Any return code that is not AV_OK is treated as AV_EOF */

typedef int (*av_read_audio_t)(void *ctx, int16_t **samples, size_t *nsamples);

/* av_close(): The source is being closed. The return code is ignored */

typedef int (*av_close_t)(void *ctx);



/* Frame fit/crop modes */
typedef enum {
	AV_FIT_STRETCH,
	AV_FIT_FILL,
	AV_FIT_FIT,
	AV_FIT_NONE,
} av_fit_mode_t;

typedef struct {
	
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	
	/* Video settings */
	int width;
	int height;
	r64_t frame_rate;
	r64_t display_aspect_ratios[2];
	av_fit_mode_t fit_mode;
	r64_t min_display_aspect_ratio;
	r64_t max_display_aspect_ratio;
	av_frame_t default_frame;
	
	/* Video state */
	unsigned int frames;
	
	/* Audio settings */
	r64_t sample_rate;
	
	/* Audio state */
	unsigned int samples;
	
	/* AV source data and callbacks */
	void *av_source_ctx;
	av_read_video_t read_video;
	av_read_audio_t read_audio;
	av_close_t close;
	
} av_t;

extern void av_frame_init(av_frame_t *frame, int width, int height, uint32_t *framebuffer, int pstride, int lstride);

extern int av_read_video(av_t *s, av_frame_t *frame);
extern int av_read_audio(av_t *s, int16_t **samples, size_t *nsamples);
extern int av_eof(av_t *s);
extern int av_close(av_t *s);

extern r64_t av_display_aspect_ratio(av_frame_t *frame);
extern void av_set_display_aspect_ratio(av_frame_t *frame, r64_t display_aspect_ratio);

extern r64_t av_calculate_frame_size(av_t *s, r64_t resolution, r64_t pixel_aspect_ratio);

extern void av_hflip_frame(av_frame_t *frame);
extern void av_vflip_frame(av_frame_t *frame);
extern void av_rotate_frame(av_frame_t *frame, int a);
extern void av_crop_frame(av_frame_t *frame, int x, int y, int width, int height);

#include "av_test.h"
#include "av_ffmpeg.h"

#endif

