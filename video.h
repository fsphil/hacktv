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
#include "nicam728.h"
#include "fir.h"

typedef struct vid_t vid_t;

#include "teletext.h"
#include "wss.h"
#include "videocrypt.h"
#include "videocrypts.h"
#include "syster.h"
#include "acp.h"

/* Return codes */
#define VID_OK             0
#define VID_ERROR         -1
#define VID_OUT_OF_MEMORY -2

/* Frame type */
#define VID_RASTER_625 0
#define VID_RASTER_525 1
#define VID_RASTER_405 2
#define VID_RASTER_819 3
#define VID_BAIRD_240  4
#define VID_BAIRD_30   5
#define VID_APOLLO_320 6
#define VID_MAC        7

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
#define VID_APOLLO_FSC 4

/* AV source function prototypes */
typedef uint32_t *(*vid_read_video_t)(void *private, float *ratio);
typedef int16_t *(*vid_read_audio_t)(void *private, size_t *samples);
typedef int (*vid_eof_t)(void *private);
typedef int (*vid_close_t)(void *private);



/* RF modulation */

typedef struct {
	int16_t level;
	int32_t counter;
	cint32_t phase;
	cint32_t *lut;
} _mod_fm_t;

typedef struct {
	int16_t level;
	int32_t counter;
	cint32_t phase;
	cint32_t delta;
} _mod_am_t;



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
	int type;
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
	
	char *teletext;
	char *logo;
	int timestamp;
	int position;
	char *mode;
	
	char *wss;
	
	char *videocrypt;
	char *videocrypt2;
	char *videocrypts;
	int d11;
	char *key;
	int syster;
	int systeraudio;
	int acp;
	
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
	
	double fsc_flag_width;
	double fsc_flag_left;
	double fsc_flag_level;
	
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
	double nicam_beta;
	
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
	vid_eof_t av_eof;
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
	
	int16_t white_level;
	int16_t black_level;
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
	
	_mod_fm_t fm_secam_cr;
	_mod_fm_t fm_secam_cb;

	int fsc_flag_left;
	int fsc_flag_width;
	int16_t fsc_flag_level;
	
	/* Video state */
	uint32_t *framebuffer;
	
	/* The frame and line number being rendered next */
	int bframe;
	int bline;
	
	/* The frame and line number returned by vid_next_line() */
	int frame;
	int line;
	
	/* Current frame's aspect ratio */
	float ratio;
	
	/* Video filter */
	int16_t *video_filter_taps;
	fir_int16_t video_filter;
	
	/* Teletext state */
	tt_t tt;
	
	/* WSS state */
	wss_t wss;
	
	/* Videocrypt state */
	vc_t vc;
	vcs_t vcs;
	
	/* Nagravision Syster state */
	ng_t ng;
	
	/* ACP state */
	acp_t acp;
	
	/* Audio state */
	int audio;
	int16_t *audiobuffer;
	size_t audiobuffer_samples;
	
	/* FM Mono/Stereo audio state */
	_mod_fm_t fm_mono;
	_mod_fm_t fm_left;
	_mod_fm_t fm_right;
	
	/* NICAM stereo audio state */
	nicam_mod_t nicam;
	int16_t nicam_buf[NICAM_AUDIO_LEN * 2];
	size_t nicam_buf_len;
	
	/* AM Mono audio state */
	_mod_am_t am_mono;
	
	/* FM Video state */
	_mod_fm_t fm_video;
	
	/* Output line(s) buffer */
	int olines;		/* The number of lines */
	int16_t **oline;	/* Pointer to each line */
	int16_t *output;	/* Pointer to the current line */
	int odelay;		/* Index of the current line */
};

extern const vid_configs_t vid_configs[];

extern int vid_init(vid_t *s, unsigned int sample_rate, const vid_config_t * const conf);
extern void vid_free(vid_t *s);
extern int vid_av_close(vid_t *s);
extern void vid_info(vid_t *s);
extern int vid_init_filter(vid_t *s);
extern size_t vid_get_framebuffer_length(vid_t *s);
extern int16_t *vid_adj_delay(vid_t *s, int lines);
extern int16_t *vid_next_line(vid_t *s, size_t *samples);

#endif

