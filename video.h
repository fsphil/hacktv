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

#ifndef _VIDEO_H
#define _VIDEO_H

#include <stdint.h>

/* Return codes */
#define VID_OK             0
#define VID_ERROR         -1
#define VID_OUT_OF_MEMORY -2

/* Colour modes */
#define VID_MONOCHROME 0
#define VID_PAL        1
#define VID_NTSC       2
#define VID_SECAM      3

/* AV source function prototypes */
typedef uint32_t *(*vid_read_video_t)(void *private);
typedef int16_t *(*vid_read_audio_t)(void *private, size_t *samples);
typedef int (*vid_close_t)(void *private);

typedef struct {
	
	/* Output type */
	int output_type;
	
	/* Overall signal level */
	double level;
	
	/* These three should add up to 1.0 */
	double video_level;
	double mono_level;
	double nicam_level;
	
	/* Video */
	int frame_rate_num;
	int frame_rate_den;
	
	int lines;
	int active_lines;
	
	double hsync_width;
	double vsync_short_width;
	double vsync_long_width;
	
	double white_level;
	double black_level;
	double blanking_level;
	double sync_level;
	
	double active_width;
	double active_left;
	
	double gamma;
	
	/* RGB weights, should add up to 1.0 */
	double rw_co;
	double gw_co;
	double bw_co;
	
	int colour_mode;
	double colour_carrier;
	int colour_lookup_lines;
	
	double burst_width;
	double burst_left;
	double burst_level;
	
	double iu_co;
	double iv_co;
	double qu_co;
	double qv_co;
	
	/* Mono FM audio */
	double mono_carrier;
	double mono_preemph;
	double mono_deviation;
	
	/* Stereo NICAM audio */
	double nicam_carrier;
	
} vid_config_t;

typedef struct {
	const char *id;
	const vid_config_t *conf;
} vid_configs_t;

typedef struct {
	
	/* Source interface */
	void *av_private;
	vid_read_video_t av_read_video;
	vid_read_audio_t av_read_audio;
	vid_close_t av_close;
	
	/* Signal configuration */
	vid_config_t conf;

	/* Video setup */
	int sample_rate;
	
	int width;
	int half_width;
	int active_width;
	int active_left;
	
	int hsync_width;
	int vsync_short_width;
	int vsync_long_width;
	
	int16_t blanking_level;
	int16_t sync_level;
	
	int16_t *y_level_lookup;
	int16_t *i_level_lookup;
	int16_t *q_level_lookup;
	
	int colour_lookup_width;
	int16_t *colour_lookup;
	
	int burst_left;
	int burst_width;
	int16_t burst_level;
	
	/* Video state */
	uint32_t *framebuffer;
	
	unsigned int frame;
	unsigned int line;
	
	/* Audio state */
	int16_t *audiobuffer;
	size_t audiobuffer_samples;
	
	/* Mono audio state */
	int mono_lookup_width;
	int16_t *mono_lookup;
	int mono_delta;
	int mono_pi;
	int mono_pq;
	
	/* Output line buffer */
	int16_t *output;
	
} vid_t;

extern const vid_config_t vid_config_pal_i;
extern const vid_config_t vid_config_pal;
extern const vid_config_t vid_config_ntsc_m;
extern const vid_config_t vid_config_ntsc;
extern const vid_config_t vid_config_405_a;
extern const vid_config_t vid_config_405;
extern const vid_configs_t vid_configs[];

extern int vid_init(vid_t *s, unsigned int sample_rate, const vid_config_t * const conf);
extern void vid_free(vid_t *s);
extern int vid_av_close(vid_t *s);
extern void vid_info(vid_t *s);
extern size_t vid_get_framebuffer_length(vid_t *s);
extern int16_t *vid_next_line(vid_t *s, size_t *samples);

#endif

