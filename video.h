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

typedef struct vid_t vid_t;

#include "videocrypt.h"

/* Return codes */
#define VID_OK             0
#define VID_ERROR         -1
#define VID_OUT_OF_MEMORY -2

/* Output modulation types */
#define VID_NONE 0
#define VID_AM   1
#define VID_VSB  2
#define VID_FM   3

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
	
	/* Output modulation */
	int modulation;
	
	/* VSB modulation options */
	double vsb_upper_bw;
	double vsb_lower_bw;
	
	/* FM modulation options */
	double fm_level;
	double fm_deviation;
	
	/* Overall signal level (pre-modulation) */
	double level;
	
	/* Level of each component. The total sum should be exactly 1.0 */
	double video_level;
	double fm_audio_level;
	double am_audio_level;
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
	
	int videocrypt;
	
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
	
	/* FM audio */
	double fm_mono_carrier;
	double fm_left_carrier;
	double fm_right_carrier;
	double fm_audio_preemph;
	double fm_audio_deviation;
	
	/* Stereo NICAM audio */
	double nicam_carrier;
	
	/* AM audio */
	double am_mono_carrier;
	double am_mono_bandwidth;
	
} vid_config_t;

typedef struct {
	const char *id;
	const vid_config_t *conf;
} vid_configs_t;

struct vid_t {
	
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
	
	/* Videocrypt state */
	vc_t vc;
	
	/* Audio state */
	int16_t *audiobuffer;
	size_t audiobuffer_samples;
	
	/* FM audio state */
	int fm_audio_lookup_width;
	int16_t *fm_audio_lookup;
	
	/* FM Mono state */
	int fm_mono_delta;
	int fm_mono_pi;
	int fm_mono_pq;
	
	/* FM Left state */
	int fm_left_delta;
	int fm_left_pi;
	int fm_left_pq;
	
	/* FM Right state */
	int fm_right_delta;
	int fm_right_pi;
	int fm_right_pq;
	
	/* AM state */
	int am_lookup_width;
	int16_t *am_lookup;
	int am_mono_delta;
	int am_mono_pi;
	int am_mono_pq;
	
	/* FM state */
	int fm_lookup_width;
	int16_t *fm_lookup;
	int fm_delta;
	int fm_pi;
	int fm_pq;
	
	/* Output line buffer */
	int16_t *output;
	
};

extern const vid_config_t vid_config_pal_i;
extern const vid_config_t vid_config_pal;
extern const vid_config_t vid_config_ntsc_m;
extern const vid_config_t vid_config_ntsc;
extern const vid_config_t vid_config_405_a;
extern const vid_config_t vid_config_405;
extern const vid_config_t vid_config_baird_240_am;
extern const vid_config_t vid_config_baird_240;
extern const vid_configs_t vid_configs[];

extern int vid_init(vid_t *s, unsigned int sample_rate, const vid_config_t * const conf);
extern void vid_free(vid_t *s);
extern int vid_av_close(vid_t *s);
extern void vid_info(vid_t *s);
extern size_t vid_get_framebuffer_length(vid_t *s);
extern int16_t *vid_next_line(vid_t *s, size_t *samples);

#endif

