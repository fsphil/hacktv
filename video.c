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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "video.h"
#include "nicam728.h"
#include "dance.h"
#include "hacktv.h"
#include <sys/time.h>

/* 
 * Video generation
 * 
 * The output from this encoder is a 16-bit IQ signal which
 * hopefully contains an accurate video and audio signal for
 * display on old analogue TV sets.
 * 
 * The encoder makes liberal use of lookup tables:
 * 
 * - 3x for RGB > gamma corrected Y, I and Q levels.
 * 
 * - A temporary gamma table used while generating the above.
 * 
 * - PAL colour carrier (4 full frames in length + 1 line) or
 *   NTSC colour carrier (2 full lines + 1 line).
*/

const vid_config_t vid_config_pal_i = {
	
	/* System I (PAL) */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.modulation     = VID_VSB,
	.vsb_upper_bw   = 5500000, /* Hz */
	.vsb_lower_bw   = 1250000, /* Hz */
	
	.level          = 1.0, /* Overall signal level */
	
	.video_level    = 0.71, /* Power level of video */
	.fm_mono_level  = 0.22, /* FM audio carrier power level */
	.nicam_level    = 0.07 / 2, /* NICAM audio carrier power level */
	
	.type           = VID_RASTER_625,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 625,
	.hline          = 313,
	
	.active_lines   = 576,
	.active_width   = 0.00005195, /* 51.95µs */
	.active_left    = 0.00001040, /* |-->| 10.40µs */
	
	.hsync_width       = 0.00000470, /* 4.70 ±0.20µs */
	.vsync_short_width = 0.00000235, /* 2.35 ±0.10µs */
	.vsync_long_width  = 0.00002730, /* 2.73 ±0.20µs */
	
	.white_level    = 0.20,
	.black_level    = 0.76,
	.blanking_level = 0.76,
	.sync_level     = 1.00,
	
	.colour_mode    = VID_PAL,
	.burst_width    = 0.00000225, /* 2.25 ±0.23µs */
	.burst_rise     = 0.00000030, /* 0.30 ±0.10µs */
	.burst_left     = 0.00000560, /* |-->| 5.6 ±0.1µs */
	.burst_level    = 3.0 / 7.0, /* 3 / 7 of white - blanking level */
	.colour_carrier = 4433618.75,
	.colour_lookup_lines = 625 * 4, /* The carrier repeats after 4 frames */
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	.iu_co          = 0.000,
	.iv_co          = 0.877,
	.qu_co          = 0.493,
	.qv_co          = 0.000,
	
	.fm_mono_carrier   = 6000000 - 400, /* Hz */
	.fm_mono_deviation = 50000, /* +/- Hz */
	.fm_mono_preemph   = VID_50US,
	
	.nicam_carrier  = 6552000, /* Hz */
	.nicam_beta     = 1.0,
};

const vid_config_t vid_config_pal_bg = {
	
	/* System B/G (PAL) */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.modulation     = VID_VSB,
	.vsb_upper_bw   = 5000000, /* Hz */
	.vsb_lower_bw   =  750000, /* Hz */
	
	.level          = 1.0, /* Overall signal level */
	
	.video_level    = 0.71, /* Power level of video */
	.fm_mono_level  = 0.22, /* FM audio carrier power level */
	.nicam_level    = 0.07 / 2, /* NICAM audio carrier power level */
	
	.type           = VID_RASTER_625,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 625,
	.hline          = 313,
	
	.active_lines   = 576,
	.active_width   = 0.00005195, /* 51.95µs */
	.active_left    = 0.00001040, /* |-->| 10.40µs */
	
	.hsync_width       = 0.00000470, /* 4.70 ±0.20µs */
	.vsync_short_width = 0.00000235, /* 2.35 ±0.10µs */
	.vsync_long_width  = 0.00002730, /* 2.73 ±0.20µs */
	
	.white_level    = 0.20,
	.black_level    = 0.76,
	.blanking_level = 0.76,
	.sync_level     = 1.00,
	
	.colour_mode    = VID_PAL,
	.burst_width    = 0.00000225, /* 2.25 ±0.23µs */
	.burst_rise     = 0.00000030, /* 0.30 ±0.10µs */
	.burst_left     = 0.00000560, /* |-->| 5.6 ±0.1µs */
	.burst_level    = 3.0 / 7.0, /* 3 / 7 of white - blanking level */
	.colour_carrier = 4433618.75,
	.colour_lookup_lines = 625 * 4, /* The carrier repeats after 4 frames */
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	.iu_co          = 0.000,
	.iv_co          = 0.877,
	.qu_co          = 0.493,
	.qv_co          = 0.000,
	
	.fm_mono_carrier   = 5500000, /* Hz */
	.fm_mono_deviation = 50000, /* +/- Hz */
	.fm_mono_preemph   = VID_50US, /* Seconds */
	
	.nicam_carrier  = 5850000, /* Hz */
	.nicam_beta     = 0.4,
};

const vid_config_t vid_config_pal_fm = {
	
	/* PAL FM (satellite) */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.modulation     = VID_FM,
	.fm_level       = 1.0,
	.fm_deviation   = 16e6, /* 16 MHz/V */
	
	.level          = 0.8, /* Overall signal level */
	
	.video_level    = 1.00, /* Power level of video */
	.fm_mono_level  = 0.06, /* FM audio carrier power level */
	//.fm_left_level  = 0.04, /* FM stereo left audio carrier power level */
	//.fm_right_level = 0.04, /* FM stereo right audio carrier power level */
	
	.type           = VID_RASTER_625,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 625,
	.hline          = 313,
	
	.active_lines   = 576,
	.active_width   = 0.00005195, /* 51.95µs */
	.active_left    = 0.00001040, /* |-->| 10.40µs */
	
	.hsync_width       = 0.00000470, /* 4.70 ±0.20µs */
	.vsync_short_width = 0.00000235, /* 2.35 ±0.10µs */
	.vsync_long_width  = 0.00002730, /* 2.73 ±0.20µs */
	
	.white_level    =  0.50,
	.black_level    = -0.20,
	.blanking_level = -0.20,
	.sync_level     = -0.50,
	
	.colour_mode    = VID_PAL,
	.burst_width    = 0.00000225, /* 2.25 ±0.23µs */
	.burst_rise     = 0.00000030, /* 0.30 ±0.10µs */
	.burst_left     = 0.00000560, /* |-->| 5.6 ±0.1µs */
	.burst_level    = 3.0 / 7.0, /* 3 / 7 of white - blanking level */
	.colour_carrier = 4433618.75,
	.colour_lookup_lines = 625 * 4, /* The carrier repeats after 4 frames */
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	.iu_co          = 0.000,
	.iv_co          = 0.877,
	.qu_co          = 0.493,
	.qv_co          = 0.000,
	
	.fm_mono_carrier   = 6500000, /* Hz */
	.fm_mono_deviation = 85000, /* +/- Hz */
	.fm_mono_preemph   = VID_50US, /* Seconds */
	
	//.fm_left_carrier   = 7020000, /* Hz */
	//.fm_left_deviation = 50000, /* +/- Hz */
	//.fm_left_preemph   = VID_75US, /* Seconds */
	
	//.fm_right_carrier   = 7200000, /* Hz */
	//.fm_right_deviation = 50000, /* +/- Hz */
	//.fm_right_preemph   = VID_75US, /* Seconds */
};

const vid_config_t vid_config_pal = {
	
	/* Composite PAL */
	.output_type    = HACKTV_INT16_REAL,
	
	.level          = 1.0, /* Overall signal level */
	.video_level    = 1.0, /* Power level of video */
	
	.video_bw       = 6.0e6,
	
	.type           = VID_RASTER_625,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 625,
	.hline          = 313,
	
	.active_lines   = 576,
	.active_width   = 0.00005195, /* 51.95µs */
	.active_left    = 0.00001040, /* |-->| 10.40µs */
	
	.hsync_width       = 0.00000470, /* 4.70 ±0.20µs */
	.vsync_short_width = 0.00000235, /* 2.35 ±0.10µs */
	.vsync_long_width  = 0.00002730, /* 2.73 ±0.20µs */
	
	.white_level = 1.00 * 0.85 * 2 - 1,
	.black_level = 0.30 * 0.85 * 2 - 1,
	.blanking_level = 0.30 * 0.85 * 2 - 1,
	.sync_level = 0.00 * 0.85 * 2 - 1,
	
	.colour_mode    = VID_PAL,
	.burst_width    = 0.00000225, /* 2.25 ±0.23µs */
	.burst_rise     = 0.00000030, /* 0.30 ±0.10µs */
	.burst_left     = 0.00000560, /* |-->| 5.6 ±0.1µs */
	.burst_level    = 3.0 / 7.0, /* 3 / 7 of white - blanking level */
	.colour_carrier = 4433618.75,
	.colour_lookup_lines = 625 * 4, /* The carrier repeats after 4 frames */
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	.iu_co          = 0.000,
	.iv_co          = 0.877,
	.qu_co          = 0.493,
	.qv_co          = 0.000,
};

const vid_config_t vid_config_pal_m = {
	
	/* System M (525 PAL) */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.modulation     = VID_VSB,
	.vsb_upper_bw   = 4200000, /* Hz */
	.vsb_lower_bw   =  750000, /* Hz */
	
	.level          = 1.0, /* Overall signal level */
	
	.video_level    = 0.83, /* Power level of video */
	.fm_mono_level  = 0.17, /* FM audio carrier power level */
	
	.type           = VID_RASTER_525,
	.frame_rate_num = 30000,
	.frame_rate_den = 1001,
	.lines          = 525,
	.hline          = 263,
	
	.active_lines   = 480,
	.active_width   = 0.00005280, /* 52.80µs */
	.active_left    = 0.00000920, /* |-->| 9.2 +0.2 -0.1µs */
	
	.hsync_width       = 0.00000470, /* 4.70 ±0.10µs */
	.vsync_short_width = 0.00000230, /* 2.30 ±0.10μs */
	.vsync_long_width  = 0.00002710, /* 27.1μs */
	
	.white_level    = 0.2000,
	.black_level    = 0.7280,
	.blanking_level = 0.7712,
	.sync_level     = 1.0000,
	
	.colour_mode    = VID_PAL,
	.burst_width    = 0.00000252, /* 2.52 ±0.28 μs */
	.burst_rise     = 0.00000030, /* 0.30 ±0.10µs */
	.burst_left     = 0.00000530, /* |-->| 5.3 ±0.1μs */
	.burst_level    = 33.0 / 73.0, /* Approximation */
	.colour_carrier = 227.25 * 525 * (30.0 / 1.001),
	.colour_lookup_lines = 4, /* The carrier repeats after 4 lines */
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	.iu_co          = 0.000,
	.iv_co          = 0.877,
	.qu_co          = 0.493,
	.qv_co          = 0.000,
	
	.fm_mono_carrier   = 4500000, /* Hz */
	.fm_mono_deviation = 25000, /* +/- Hz */
	.fm_mono_preemph   = VID_75US, /* Seconds */
};

const vid_config_t vid_config_525pal = {
	
	/* Composite 525PAL */
	.output_type    = HACKTV_INT16_REAL,
	
	.level          = 1.0, /* Overall signal level */
	.video_level    = 1.0, /* Power level of video */
	
	.video_bw       = 6.0e6,
	
	.type           = VID_RASTER_525,
	.frame_rate_num = 30000,
	.frame_rate_den = 1001,
	.lines          = 525,
	.hline          = 263,
	
	.active_lines   = 480,
	.active_width   = 0.00005280, /* 52.80µs */
	.active_left    = 0.00000920, /* |-->| 9.2 +0.2 -0.1µs */
	
	.hsync_width       = 0.00000470, /* 4.70 ±0.10µs */
	.vsync_short_width = 0.00000230, /* 2.3 ± 0.10μs */
	.vsync_long_width  = 0.00002710, /* 27.1μs */
	
	.white_level    =  0.70,
	.black_level    =  0.00,
	.blanking_level =  0.00,
	.sync_level     = -0.30,
	
	.colour_mode    = VID_PAL,
	.burst_width    = 0.00000252, /* 2.52 ±0.28 μs */
	.burst_rise     = 0.00000030, /* 0.30 ±0.10µs */
	.burst_left     = 0.00000530, /* |-->| 5.3 ±0.1μs */
	.burst_level    = 33.0 / 73.0, /* Approximation */
	.colour_carrier = 227.25 * 525 * (30.0 / 1.001),
	.colour_lookup_lines = 4, /* The carrier repeats after 4 lines */
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	.iu_co          = 0.000,
	.iv_co          = 0.877,
	.qu_co          = 0.493,
	.qv_co          = 0.000,
};

const vid_config_t vid_config_secam_l = {
	
	/* System L (SECAM) */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.modulation     = VID_VSB,
	.vsb_upper_bw   = 6000000, /* Hz */
	.vsb_lower_bw   = 1250000, /* Hz */
	
	.level          = 1.0, /* Overall signal level */
	
	.video_level    = 0.80, /* Power level of video */
	.am_audio_level = 0.10, /* AM audio carrier power level */
	.nicam_level    = 0.04, /* NICAM audio carrier power level */
	
	.type           = VID_RASTER_625,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 625,
	.hline          = 313,
	
	.active_lines   = 576,
	.active_width   = 0.00005195, /* 51.95µs */
	.active_left    = 0.00001040, /* |-->| 10.40µs */
	
	.hsync_width       = 0.00000470, /* 4.70 ±0.20µs */
	.vsync_short_width = 0.00000235, /* 2.35 ±0.10µs */
	.vsync_long_width  = 0.00002730, /* 2.73 ±0.20µs */
	
	.white_level    = 1.00,
	.black_level    = 0.30,
	.blanking_level = 0.30,
	.sync_level     = 0.00,
	
	.colour_mode    = VID_SECAM,
	.burst_level    = 0.23,
	.burst_left     = 0.00000560, /* |-->| 5.6 ±0.1µs */
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	.iu_co          = 2.000,
	.iv_co          = 0.000,
	.qu_co          = 0.000,
	.qv_co          = -2.000,
	
	.am_mono_carrier = 6500000, /* Hz */
	
	.nicam_carrier  = 5850000, /* Hz */
	.nicam_beta     = 0.4,
};

const vid_config_t vid_config_secam_dk = {
	
	/* System D/K (SECAM) */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.modulation     = VID_VSB,
	.vsb_upper_bw   = 6000000, /* Hz */
	.vsb_lower_bw   =  750000, /* Hz */
	
	.level          = 1.0, /* Overall signal level */
	
	.video_level    = 0.70, /* Power level of video */
	.fm_mono_level  = 0.20, /* FM audio carrier power level */
	
	.type           = VID_RASTER_625,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 625,
	.hline          = 313,
	
	.active_lines   = 576,
	.active_width   = 0.00005195, /* 51.95µs */
	.active_left    = 0.00001040, /* |-->| 10.40µs */
	
	.hsync_width       = 0.00000470, /* 4.70 ±0.20µs */
	.vsync_short_width = 0.00000235, /* 2.35 ±0.10µs */
	.vsync_long_width  = 0.00002730, /* 2.73 ±0.20µs */
	
	.white_level    = 0.20,
	.black_level    = 0.76,
	.blanking_level = 0.76,
	.sync_level     = 1.00,
	
	.colour_mode    = VID_SECAM,
	.burst_level    = 0.23,
	.burst_left     = 0.00000560, /* |-->| 5.6 ±0.1µs */
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	.iu_co          = 2.000,
	.iv_co          = 0.000,
	.qu_co          = 0.000,
	.qv_co          = -2.000,
	
	.fm_mono_carrier   = 6500000, /* Hz */
	.fm_mono_deviation = 50000, /* +/- Hz */
	.fm_mono_preemph   = VID_50US, /* Seconds */
};

const vid_config_t vid_config_secam_fm = {
	
	/* SECAM FM (satellite) */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.modulation     = VID_FM,
	.fm_level       = 1.0,
	.fm_deviation   = 16e6, /* 16 MHz/V */
	
	.level          = 1.0, /* Overall signal level */
	
	.video_level    = 1.00, /* Power level of video */
	.fm_mono_level  = 0.05, /* FM audio carrier power level */
	//.fm_left_level  = 0.025, /* FM stereo left audio carrier power level */
	//.fm_right_level = 0.025, /* FM stereo right audio carrier power level */
	
	.type           = VID_RASTER_625,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 625,
	.hline          = 313,
	
	.active_lines   = 576,
	.active_width   = 0.00005195, /* 51.95µs */
	.active_left    = 0.00001040, /* |-->| 10.40µs */
	
	.hsync_width       = 0.00000470, /* 4.70 ±0.20µs */
	.vsync_short_width = 0.00000235, /* 2.35 ±0.10µs */
	.vsync_long_width  = 0.00002730, /* 2.73 ±0.20µs */
	
	.white_level    =  0.50,
	.black_level    = -0.20,
	.blanking_level = -0.20,
	.sync_level     = -0.50,
	
	.colour_mode    = VID_SECAM,
	.burst_level    = 0.23,
	.burst_left     = 0.00000560, /* |-->| 5.6 ±0.1µs */
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	.iu_co          = 2.000,
	.iv_co          = 0.000,
	.qu_co          = 0.000,
	.qv_co          = -2.000,
	
	.fm_mono_carrier   = 6500000, /* Hz */
	.fm_mono_deviation = 85000, /* +/- Hz */
	.fm_mono_preemph   = VID_50US, /* Seconds */
	
	//.fm_left_carrier   = 7020000, /* Hz */
	//.fm_left_deviation = 50000, /* +/- Hz */
	//.fm_left_preemph   = VID_50US, /* Seconds */
	
	//.fm_right_carrier   = 7200000, /* Hz */
	//.fm_right_deviation = 50000, /* +/- Hz */
	//.fm_right_preemph   = VID_50US, /* Seconds */
};

const vid_config_t vid_config_secam = {
	
	/* Composite SECAM */
	.output_type    = HACKTV_INT16_REAL,
	
	.level          = 1.0, /* Overall signal level */
	.video_level    = 1.0, /* Power level of video */
	
	.video_bw       = 6.0e6,
	
	.type           = VID_RASTER_625,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 625,
	.hline          = 313,
	
	.active_lines   = 576,
	.active_width   = 0.00005195, /* 51.95µs */
	.active_left    = 0.00001040, /* |-->| 10.40µs */
	
	.hsync_width       = 0.00000470, /* 4.70 ±0.20µs */
	.vsync_short_width = 0.00000235, /* 2.35 ±0.10µs */
	.vsync_long_width  = 0.00002730, /* 2.73 ±0.20µs */
	
	.white_level    =  0.70,
	.black_level    =  0.00,
	.blanking_level =  0.00,
	.sync_level     = -0.30,
	
	.colour_mode    = VID_SECAM,
	.burst_level    = 0.23,
	.burst_left     = 0.00000560, /* |-->| 5.6 ±0.1µs */
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	.iu_co          = 2.000,
	.iv_co          = 0.000,
	.qu_co          = 0.000,
	.qv_co          = -2.000,
};

const vid_config_t vid_config_ntsc_m = {
	
	/* System M (NTSC) */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.modulation     = VID_VSB,
	.vsb_upper_bw   = 4200000, /* Hz */
	.vsb_lower_bw   =  750000, /* Hz */
	
	.level          = 1.0, /* Overall signal level */
	
	.video_level    = 0.83, /* Power level of video */
	.fm_mono_level  = 0.17, /* FM audio carrier power level */
	
	.type           = VID_RASTER_525,
	.frame_rate_num = 30000,
	.frame_rate_den = 1001,
	.lines          = 525,
	.hline          = 263,
	
	.active_lines   = 480,
	.active_width   = 0.00005290, /* 52.90µs */
	.active_left    = 0.00000920, /* |-->| 9.20µs */
	
	.hsync_width       = 0.00000470, /*  4.70 ±1.00µs */
	.vsync_short_width = 0.00000230, /*  2.30 ±0.10µs */
	.vsync_long_width  = 0.00002710, /* 27.10 µs */
	
	.white_level    = 0.125000,
	.black_level    = 0.703125,
	.blanking_level = 0.750000,
	.sync_level     = 1.000000,
	
	.colour_mode    = VID_NTSC,
	.burst_width    = 0.00000250, /* 2.5 ±0.28µs */
	.burst_rise     = 0.00000030, /* 0.30 ±0.10µs */
	.burst_left     = 0.00000530, /* |-->| 5.3 ±0.1µs */
	.burst_level    = 4.0 / 10.0, /* 4/10 of white - blanking level */
	.colour_carrier = 5000000.0 * 63 / 88,
	.colour_lookup_lines = 2, /* The carrier repeats after 2 lines */
	
	.rw_co          =  0.299, /* R weight */
	.gw_co          =  0.587, /* G weight */
	.bw_co          =  0.114, /* B weight */
	.iu_co          =  0.000,
	.iv_co          =  0.877,
	.qu_co          =  0.493,
	.qv_co          =  0.000,
	
	.fm_mono_carrier   = 4500000, /* Hz */
	.fm_mono_deviation = 25000, /* +/- Hz */
	.fm_mono_preemph   = VID_75US,
};

const vid_config_t vid_config_ntsc_fm = {
	
	/* NTSC FM (satellite) */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.modulation     = VID_FM,
	.fm_level       = 1.0,
	.fm_deviation   = 16e6, /* 16 MHz/V */
	
	.level          = 1.0, /* Overall signal level */
	
	.video_level    = 1.00, /* Power level of video */
	.fm_mono_level  = 0.05, /* FM audio carrier power level */
	//.fm_left_level  = 0.025, /* FM stereo left audio carrier power level */
	//.fm_right_level = 0.025, /* FM stereo right audio carrier power level */
	
	.type           = VID_RASTER_525,
	.frame_rate_num = 30000,
	.frame_rate_den = 1001,
	.lines          = 525,
	.hline          = 263,
	
	.active_lines   = 480,
	.active_width   = 0.00005290, /* 52.90µs */
	.active_left    = 0.00000920, /* |-->| 9.20µs */
	
	.hsync_width       = 0.00000470, /*  4.70 ±1.00µs */
	.vsync_short_width = 0.00000230, /*  2.30 ±0.10µs */
	.vsync_long_width  = 0.00002710, /* 27.10 µs */
	
	.white_level    =  0.5000,
	.black_level    = -0.1607,
	.blanking_level = -0.2143,
	.sync_level     = -0.5000,
	
	.colour_mode    = VID_NTSC,
	.burst_width    = 0.00000250, /* 2.5 ±0.28µs */
	.burst_rise     = 0.00000030, /* 0.30 ±0.10µs */
	.burst_left     = 0.00000530, /* |-->| 5.3 ±0.1µs */
	.burst_level    = 4.0 / 10.0, /* 4/10 of white - blanking level */
	.colour_carrier = 5000000.0 * 63 / 88,
	.colour_lookup_lines = 2, /* The carrier repeats after 2 lines */
	
	.rw_co          =  0.299, /* R weight */
	.gw_co          =  0.587, /* G weight */
	.bw_co          =  0.114, /* B weight */
	.iu_co          =  0.000,
	.iv_co          =  0.877,
	.qu_co          =  0.493,
	.qv_co          =  0.000,
	
	.fm_mono_carrier   = 6500000, /* Hz */
	.fm_mono_deviation = 85000, /* +/- Hz */
	.fm_mono_preemph   = VID_50US, /* Seconds */
	
	//.fm_left_carrier   = 7020000, /* Hz */
	//.fm_left_deviation = 50000, /* +/- Hz */
	//.fm_left_preemph   = VID_50US, /* Seconds */
	
	//.fm_right_carrier   = 7200000, /* Hz */
	//.fm_right_deviation = 50000, /* +/- Hz */
	//.fm_right_preemph   = VID_50US, /* Seconds */
};

const vid_config_t vid_config_ntsc_bs_fm = {
	
	/* Digital Subcarrier/NTSC FM (satellite) */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.modulation     = VID_FM,
	.fm_level       = 1.0,
	.fm_deviation   = 17.0e6, /* 17.0 MHz/V */
	
	.level          = 1.0, /* Overall signal level */
	
	.video_level    = 1.00, /* Power level of video */
	.dance_level    = 0.19, /* DANCE audio carrier power level */
	
	.type           = VID_RASTER_525,
	.frame_rate_num = 30000,
	.frame_rate_den = 1001,
	.lines          = 525,
	.hline          = 263,
	
	.active_lines   = 480,
	.active_width   = 0.00005290, /* 52.90µs */
	.active_left    = 0.00000920, /* |-->| 9.20µs */
	
	.hsync_width       = 0.00000470, /*  4.70 ±1.00µs */
	.vsync_short_width = 0.00000230, /*  2.30 ±0.10µs */
	.vsync_long_width  = 0.00002710, /* 27.10 µs */
	
	.white_level    =  0.5000,
	.black_level    = -0.2143,
	.blanking_level = -0.2143,
	.sync_level     = -0.5000,
	
	.colour_mode    = VID_NTSC,
	.burst_width    = 0.00000250, /* 2.5 ±0.28µs */
	.burst_rise     = 0.00000030, /* 0.30 ±0.10µs */
	.burst_left     = 0.00000530, /* |-->| 5.3 ±0.1µs */
	.burst_level    = 4.0 / 10.0, /* 4/10 of white - blanking level */
	.colour_carrier = 5000000.0 * 63 / 88,
	.colour_lookup_lines = 2, /* The carrier repeats after 2 lines */
	
	.rw_co          =  0.299, /* R weight */
	.gw_co          =  0.587, /* G weight */
	.bw_co          =  0.114, /* B weight */
	.iu_co          =  0.000,
	.iv_co          =  0.877,
	.qu_co          =  0.493,
	.qv_co          =  0.000,
	
	.dance_carrier  = 5000000.0 * 63 / 88 * 8 / 5, /* Hz */
	.dance_beta     = 1.0,
};

const vid_config_t vid_config_ntsc = {
	
	/* Composite NTSC */
	.output_type    = HACKTV_INT16_REAL,
	
	.level          = 1.0, /* Overall signal level */
	.video_level    = 1.0, /* Power level of video */
	
	.video_bw       = 6.0e6,
	
	.type           = VID_RASTER_525,
	.frame_rate_num = 30000,
	.frame_rate_den = 1001,
	.lines          = 525,
	.hline          = 263,
	
	.active_lines   = 480,
	.active_width   = 0.00005290, /* 52.90µs */
	.active_left    = 0.00000920, /* |-->| 9.20µs */
	
	.hsync_width       = 0.00000470, /*  4.70 ±1.00µs */
	.vsync_short_width = 0.00000230, /*  2.30 ±0.10µs */
	.vsync_long_width  = 0.00002710, /* 27.10 µs */
	
	.white_level    =  100.0 / 140,
	.black_level    =    7.5 / 140,
	.blanking_level =    0.0 / 140,
	.sync_level     =  -40.0 / 140,
	
	.colour_mode    = VID_NTSC,
	.burst_width    = 0.00000250, /* 2.5 ±0.28µs */
	.burst_rise     = 0.00000030, /* 0.30 ±0.10µs */
	.burst_left     = 0.00000530, /* |-->| 5.3 ±0.1µs */
	.burst_level    = 4.0 / 10.0, /* 4/10 of white - blanking level */
	.colour_carrier = 5000000.0 * 63 / 88,
	.colour_lookup_lines = 2, /* The carrier repeats after 2 lines */
	
	.rw_co          =  0.299, /* R weight */
	.gw_co          =  0.587, /* G weight */
	.bw_co          =  0.114, /* B weight */
	.iu_co          =  0.000,
	.iv_co          =  0.877,
	.qu_co          =  0.493,
	.qv_co          =  0.000,
};

const vid_config_t vid_config_d2mac_am = {
	
	/* D2-MAC AM */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.modulation     = VID_VSB,
	.vsb_upper_bw   = 8400000, /* Hz */
	.vsb_lower_bw   = 0, /* Hz */
	
	.type           = VID_MAC,
	.chid           = 0xE8B5,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 625,
	.hline          = 313,
	
	.active_lines   = 576,
	.active_left    = 586.0 / MAC_CLOCK_RATE,
	.active_width   = 702.0 / MAC_CLOCK_RATE,
	
	.level          = 1.00, /* Overall signal level */
	.video_level    = 0.85, /* Chrominance may clip if this is set to 1 */
	
	.white_level    =  0.10,
	.black_level    =  1.00,
	.blanking_level =  0.55,
	.sync_level     =  0.55,
	
	.mac_mode       = MAC_MODE_D2,
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	.iu_co          = 0.000,
	.iv_co          = 0.927,
	.qu_co          = 0.733,
	.qv_co          = 0.000,
};

const vid_config_t vid_config_d2mac_fm = {
	
	/* D2-MAC FM (Satellite) */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.modulation     = VID_FM,
	.fm_level       = 1.0,
	.fm_deviation   = 13.5e6, /* 13.5 MHz/V */
	
	.type           = VID_MAC,
	.chid           = 0xE8B5,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 625,
	.hline          = 313,
	
	.active_lines   = 576,
	.active_left    = 586.0 / MAC_CLOCK_RATE,
	.active_width   = 702.0 / MAC_CLOCK_RATE,
	
	.level          = 1.0, /* Overall signal level */
	.video_level    = 1.0, /* Power level of video */
	
	.white_level    =  0.50,
	.black_level    = -0.50,
	.blanking_level =  0.00,
	.sync_level     =  0.00,
	
	.mac_mode       = MAC_MODE_D2,
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	.iu_co          = 0.000,
	.iv_co          = 0.927,
	.qu_co          = 0.733,
	.qv_co          = 0.000,
};

const vid_config_t vid_config_d2mac = {
	
	/* D2-MAC */
	.output_type    = HACKTV_INT16_REAL,
	
	.video_bw       = 6.0e6,
	
	.type           = VID_MAC,
	.chid           = 0xE8B5,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 625,
	.hline          = 313,
	
	.active_lines   = 576,
	.active_left    = 586.0 / MAC_CLOCK_RATE,
	.active_width   = 702.0 / MAC_CLOCK_RATE,
	
	.level          = 1.0, /* Overall signal level */
	.video_level    = 1.0, /* Power level of video */
	
	.white_level    =  1.00,
	.black_level    = -1.00,
	.blanking_level =  0.00,
	.sync_level     =  0.00,
	
	.mac_mode       = MAC_MODE_D2,
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	.iu_co          = 0.000,
	.iv_co          = 0.927,
	.qu_co          = 0.733,
	.qv_co          = 0.000,
};

const vid_config_t vid_config_dmac_am = {
	
	/* D-MAC AM */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.modulation     = VID_AM,
	
	.type           = VID_MAC,
	.chid           = 0xE8B5,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 625,
	.hline          = 313,
	
	.active_lines   = 576,
	.active_left    = 586.0 / MAC_CLOCK_RATE,
	.active_width   = 702.0 / MAC_CLOCK_RATE,
	
	.level          = 1.00, /* Overall signal level */
	.video_level    = 0.85, /* Chrominance may clip if this is set to 1 */
	
	.white_level    =  0.10,
	.black_level    =  1.00,
	.blanking_level =  0.55,
	.sync_level     =  0.55,
	
	.mac_mode       = MAC_MODE_D,
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	.iu_co          = 0.000,
	.iv_co          = 0.927,
	.qu_co          = 0.733,
	.qv_co          = 0.000,
};

const vid_config_t vid_config_dmac_fm = {
	
	/* D2-MAC FM (Satellite) */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.modulation     = VID_FM,
	.fm_level       = 1.0,
	.fm_deviation   = 13.5e6, /* 13.5 MHz/V */
	
	.type           = VID_MAC,
	.chid           = 0xE8B5,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 625,
	.hline          = 313,
	
	.active_lines   = 576,
	.active_left    = 586.0 / MAC_CLOCK_RATE,
	.active_width   = 702.0 / MAC_CLOCK_RATE,
	
	.level          = 1.0, /* Overall signal level */
	.video_level    = 1.0, /* Power level of video */
	
	.white_level    =  0.50,
	.black_level    = -0.50,
	.blanking_level =  0.00,
	.sync_level     =  0.00,
	
	.mac_mode       = MAC_MODE_D,
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	.iu_co          = 0.000,
	.iv_co          = 0.927,
	.qu_co          = 0.733,
	.qv_co          = 0.000,
};

const vid_config_t vid_config_dmac = {
	
	/* D-MAC */
	.output_type    = HACKTV_INT16_REAL,
	
	.video_bw       = 8.4e6,
	
	.type           = VID_MAC,
	.chid           = 0xE8B5,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 625,
	.hline          = 313,
	
	.active_lines   = 576,
	.active_left    = 586.0 / MAC_CLOCK_RATE,
	.active_width   = 702.0 / MAC_CLOCK_RATE,
	
	.level          = 1.0, /* Overall signal level */
	.video_level    = 1.0, /* Power level of video */
	
	.white_level    =  1.00,
	.black_level    = -1.00,
	.blanking_level =  0.00,
	.sync_level     =  0.00,
	
	.mac_mode       = MAC_MODE_D,
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	.iu_co          = 0.000,
	.iv_co          = 0.927,
	.qu_co          = 0.733,
	.qv_co          = 0.000,
};

const vid_config_t vid_config_819_e = {
	
	/* System E (819 line monochrome, French variant) */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.modulation     = VID_VSB,
	.vsb_upper_bw   =  2000000, /* Hz */
	.vsb_lower_bw   = 10400000, /* Hz */
	
	.level          = 1.0, /* Overall signal level */
	.video_level    = 0.8, /* Power level of video */
	.am_audio_level = 0.2, /* Power level of audio */
	
	.type           = VID_RASTER_819,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 819,
	.hline          = 409,
	
	.active_lines   = 720, /* Normally 738 */
	.active_width   = 0.00003944, /* 39.44µs */
	.active_left    = 0.00000890, /* |-->| 8.9µs */
	
	.hsync_width      = 0.00000250, /* 2.50 ±0.10µs */
	.vsync_long_width = 0.00002000, /* 20.0 ±1.00µs */
	
	.white_level    = 1.00,
	.black_level    = 0.35,
	.blanking_level = 0.30,
	.sync_level     = 0.00,
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	
	/* AM modulated */
	.am_mono_carrier   = 11.15e6, /* Hz */
	.am_mono_bandwidth =   10000, /* Hz */
};

const vid_config_t vid_config_819 = {
	
	/* 819 line video, French variant */
	.output_type    = HACKTV_INT16_REAL,
	
	.video_bw       = 10.4e6,
	
	.level          = 1.0, /* Overall signal level */
	.video_level    = 1.0, /* Power level of video */
	
	.type           = VID_RASTER_819,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 819,
	.hline          = 409,
	
	.active_lines   = 720, /* Normally 738 */
	.active_width   = 0.00003944, /* 39.44µs */
	.active_left    = 0.00000890, /* |-->| 8.9µs */
	
	.hsync_width      = 0.00000250, /* 2.50 ±0.10µs */
	.vsync_long_width = 0.00002000, /* 20.0 ±1.00µs */
	
	.white_level    =  0.70,
	.black_level    =  0.05,
	.blanking_level =  0.00,
	.sync_level     = -0.30,
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
};

const vid_config_t vid_config_405_a = {
	
	/* System A (405 line monochrome) */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.modulation     = VID_VSB,
	.vsb_upper_bw   =  750000, /* Hz */
	.vsb_lower_bw   = 3000000, /* Hz */
	
	.level          = 1.0, /* Overall signal level */
	.video_level    = 0.8, /* Power level of video */
	.am_audio_level = 0.2, /* Power level of audio */
	
	.type           = VID_RASTER_405,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 405,
	.hline          = 203,
	
	.active_lines   = 376,
	.active_width   = 0.00008030, /* 80.3µs */
	.active_left    = 0.00001680, /* |-->| 16.8µs */
	
	.hsync_width       = 0.00000900, /* 9.00 ±1.00µs */
	.vsync_long_width  = 0.00004000, /* 40.0 ±2.00µs */
	
	.white_level    = 1.00,
	.black_level    = 0.30,
	.blanking_level = 0.30,
	.sync_level     = 0.00,
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	
	/* AM modulated */
	.am_mono_carrier = -3500000, /* Hz */
	.am_mono_bandwidth = 10000, /* Hz */
};

const vid_config_t vid_config_405 = {
	
	/* 405 line video */
	.output_type    = HACKTV_INT16_REAL,
	
	.level          = 1.0, /* Overall signal level */
	.video_level    = 1.0, /* Power level of video */
	
	.video_bw       = 3.0e6,
	
	.type           = VID_RASTER_405,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 405,
	.hline          = 203,
	
	.active_lines   = 376,
	.active_width   = 0.00008030, /* 80.3µs */
	.active_left    = 0.00001680, /* |-->| 16.8µs */
	
	.hsync_width       = 0.00000900, /* 9.00 ±1.00µs */
	.vsync_long_width  = 0.00004000, /* 40.0 ±2.00µs */
	
	.white_level    =  0.70,
	.black_level    =  0.00,
	.blanking_level =  0.00,
	.sync_level     = -0.30,
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
};

const vid_config_t vid_config_baird_240_am = {
	
	/* Baird 240 line, AM modulation */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.modulation     = VID_AM,
	
	.level          = 1.0, /* Overall signal level */
	.video_level    = 1.0, /* Power level of video */
	
	.type           = VID_BAIRD_240,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 240,
	
	.active_lines   = 220,
	.active_width   = 0.00015,     /* 150µs */
	.active_left    = 0.000016667, /* |-->| 16.667µs */
	
	.hsync_width      = 0.000013333, /* 13.333µs */
	.vsync_long_width = 0.000166667, /* 166.667µs */
	
	.white_level    = 1.00,
	.black_level    = 0.40,
	.blanking_level = 0.40,
	.sync_level     = 0.00,
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
};

const vid_config_t vid_config_baird_240 = {
	
	/* Baird 240 line */
	.output_type    = HACKTV_INT16_REAL,
	
	.level          = 1.0, /* Overall signal level */
	.video_level    = 1.0, /* Power level of video */
	
	.type           = VID_BAIRD_240,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 240,
	
	.active_lines   = 220,
	.active_width   = 0.00015,     /* 150µs */
	.active_left    = 0.000016667, /* |-->| 16.667µs */
	
	.hsync_width      = 0.000013333, /* 13.333µs */
	.vsync_long_width = 0.000166667, /* 166.667µs */
	
	.white_level    = 1.00,
	.black_level    = 0.40,
	.blanking_level = 0.40,
	.sync_level     = 0.00,
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
};

const vid_config_t vid_config_baird_30_am = {
	
	/* Baird 30 line, AM modulation */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.modulation     = VID_AM,
	
	.level          = 1.0, /* Overall signal level */
	.video_level    = 1.0, /* Power level of video */
	
	.type           = VID_BAIRD_30,
	.frame_rate_num = 25,
	.frame_rate_den = 2,
	.lines          = 30,
	
	.active_lines   = 30,
	.active_width   = 0.002666667, /* 2.667ms */
	.active_left    = 0,
	
	.white_level    = 1.00,
	.black_level    = 0.00,
	.blanking_level = 0.00,
	.sync_level     = 0.00,
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
};

const vid_config_t vid_config_baird_30 = {
	
	/* Baird 30 line */
	.output_type    = HACKTV_INT16_REAL,
	
	.level          = 1.0, /* Overall signal level */
	.video_level    = 1.0, /* Power level of video */
	
	.type           = VID_BAIRD_30,
	.frame_rate_num = 25,
	.frame_rate_den = 2,
	.lines          = 30,
	
	.active_lines   = 30,
	.active_width   = 0.002666667, /* 2.667ms */
	.active_left    = 0,
	
	.white_level    =  1.00,
	.black_level    = -1.00,
	.blanking_level = -1.00,
	.sync_level     = -1.00,
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
};

const vid_config_t vid_config_nbtv_32_am = {
	
	/* NBTV Club standard, AM modulation (negative) */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.modulation     = VID_AM,
	
	.level          = 1.0, /* Overall signal level */
	.video_level    = 1.0, /* Power level of video */
	
	.type           = VID_NBTV_32,
	.frame_rate_num = 25,
	.frame_rate_den = 2,
	.lines          = 32,
	
	.active_lines   = 32,
	.active_width   = 2.5e-3 - 0.1e-3, /* 2.5ms - hsync */
	.active_left    = 0.1e-3,
	
	.hsync_width    = 0.1e-3, /* 0.1 to 0.25ms */
	
	.white_level    = 0.10,
	.black_level    = 0.73,
	.blanking_level = 0.73,
	.sync_level     = 1.00,
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
};

const vid_config_t vid_config_nbtv_32 = {
	
	/* NBTV Club standard */
	.output_type    = HACKTV_INT16_REAL,
	
	.level          = 1.0, /* Overall signal level */
	.video_level    = 1.0, /* Power level of video */
	
	.type           = VID_NBTV_32,
	.frame_rate_num = 25,
	.frame_rate_den = 2,
	.lines          = 32,
	
	.active_lines   = 32,
	.active_width   = 2.5e-3 - 0.1e-3, /* 2.5ms - hsync */
	.active_left    = 0.1e-3,
	
	.hsync_width    = 0.1e-3, /* 0.1 to 0.25ms */
	
	.white_level    = 1.00,
	.black_level    = 0.30,
	.blanking_level = 0.30,
	.sync_level     = 0.00,
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
};

const vid_config_t vid_config_apollo_colour_fm = {
	
	/* Unified S-Band, Apollo Colour Lunar Television */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.level          = 1.000, /* Overall signal level */
	.video_level    = 1.000, /* Power level of video */
	.fm_mono_level  = 0.150, /* Power level of audio */
	
	.modulation     = VID_FM,
	.fm_level       = 1.0,
	.fm_deviation   = 2e6, /* 2 MHz/V */
	
	.type           = VID_RASTER_525,
	.frame_rate_num = 30000,
	.frame_rate_den = 1001,
	.lines          = 525,
	.hline          = 263,
	
	.active_lines   = 480,
	.active_width   = 0.00005290, /* 52.90µs */
	.active_left    = 0.00000920, /* |-->| 9.20µs */
	
	.hsync_width       = 0.00000470, /*  4.70 ±1.00µs */
	.vsync_short_width = 0.00000230, /*  2.30 ±0.10µs */
	.vsync_long_width  = 0.00002710, /* 27.10 µs */
	
	.white_level    =  0.5000,
	.black_level    = -0.1475,
	.blanking_level = -0.2000,
	.sync_level     = -0.5000,
	
	.colour_mode    = VID_APOLLO_FSC,
	.fsc_flag_width = 0.00002000, /* 20.00µs */
	.fsc_flag_left  = 0.00001470, /* |-->| 14.70µs */
	.fsc_flag_level = 1.00,
	
	.rw_co          =  0.299, /* R weight */
	.gw_co          =  0.587, /* G weight */
	.bw_co          =  0.114, /* B weight */
	
	/* The audio carrier overlaps the video signal and
	 * requires the video to either be low pass filtered
	 * to 750kHz (Apollo 10 to 14) or cancelled out
	 * in post-processing (Apollo 15-17). */
	
	.fm_mono_carrier   = 1250000, /* Hz */
	.fm_mono_deviation = 25000, /* +/- Hz */
	.fm_mono_preemph   = VID_NONE,
};

const vid_config_t vid_config_apollo_colour = {
	
	/* Apollo Colour Lunar Television */
	.output_type    = HACKTV_INT16_REAL,
	
	.level          = 1.0, /* Overall signal level */
	.video_level    = 1.0, /* Power level of video */
	
	.type           = VID_RASTER_525,
	.frame_rate_num = 30000,
	.frame_rate_den = 1001,
	.lines          = 525,
	.hline          = 263,
	
	.active_lines   = 480,
	.active_width   = 0.00005290, /* 52.90µs */
	.active_left    = 0.00000920, /* |-->| 9.20µs */
	
	.hsync_width       = 0.00000470, /*  4.70 ±1.00µs */
	.vsync_short_width = 0.00000230, /*  2.30 ±0.10µs */
	.vsync_long_width  = 0.00002710, /* 27.10 µs */
	
	.white_level    =  0.70,
	.black_level    =  0.0525,
	.blanking_level =  0.00,
	.sync_level     = -0.30,
	
	.colour_mode    = VID_APOLLO_FSC,
	.fsc_flag_width = 0.00002000, /* 20.00µs */
	.fsc_flag_left  = 0.00001470, /* |-->| 14.70µs */
	.fsc_flag_level = 1.00,
	
	.rw_co          =  0.299, /* R weight */
	.gw_co          =  0.587, /* G weight */
	.bw_co          =  0.114, /* B weight */
};

const vid_config_t vid_config_apollo_mono_fm = {
	
	/* Unified S-Band, Apollo Lunar Television 10 fps video (Mode 1) */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.level          = 1.000, /* Overall signal level */
	.video_level    = 1.000, /* Power level of video */
	.fm_mono_level  = 0.150, /* Power level of audio */
	
	.modulation     = VID_FM,
	.fm_level       = 1.0,
	.fm_deviation   = 2e6, /* 2 MHz/V */
	
	.type           = VID_APOLLO_320,
	.frame_rate_num = 10,
	.frame_rate_den = 1,
	.lines          = 320,
	.active_lines   = 312,
	.active_width   = 0.00028250, /* 282.5µs */
	.active_left    = 0.00002500, /* |-->| 25.0µs */
	
	.hsync_width       = 0.00002000, /* 20.00µs */
	.vsync_long_width  = 0.00026750, /* 267.5µs */
	
	/* Hacky: hacktv can't generate a single pulse wider than half a line,
	 * which we need here. Use the vsync short pulse to complete the long */
	.vsync_short_width = 1.0 / 10.0 / 320.0 / 2.0 - 45e-6,
	
	/* The Apollo TV camera supports a pulse and tone sync mode. The
	 * pulse mode is a normal negative pulse, and the tone mode uses
	 * a 409600 Hz tone. I'm not sure which was used for the live
	 * transmissions from the surface. This implementation uses the
	 * negative pulse mode. */
	
	.white_level    =  0.50,
	.black_level    = -0.20,
	.blanking_level = -0.20,
	.sync_level     = -0.50,
	
	/* These are copied from the NTSC values */
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	
	.fm_mono_carrier   = 1250000, /* Hz */
	.fm_mono_deviation = 25000, /* +/- Hz */
	.fm_mono_preemph   = VID_NONE,
};

const vid_config_t vid_config_apollo_mono = {
	
	/* Apollo Lunar Television 10 fps video (Mode 1) */
	.output_type    = HACKTV_INT16_REAL,
	
	.level          = 1.0, /* Overall signal level */
	.video_level    = 1.0, /* Power level of video */
	
	.type           = VID_APOLLO_320,
	.frame_rate_num = 10,
	.frame_rate_den = 1,
	.lines          = 320,
	.active_lines   = 312,
	.active_width   = 0.00028250, /* 282.5µs */
	.active_left    = 0.00002500, /* |-->| 25.0µs */
	
	.hsync_width       = 0.00002000, /* 20.00µs */
	.vsync_long_width  = 0.00026750, /* 267.5µs */
	
	/* Hacky: hacktv can't generate a single pulse wider than half a line,
	 * which we need here. Use the vsync short pulse to complete the long */
	.vsync_short_width = 1.0 / 10.0 / 320.0 / 2.0 - 45e-6,
	
	/* The Apollo TV camera supports a pulse and tone sync mode. The
	 * pulse mode is a normal negative pulse, and the tone mode uses
	 * a 409600 Hz tone. I'm not sure which was used for the live
	 * transmissions from the surface. This implementation uses the
	 * negative pulse mode. */
	
	.white_level    =  0.70,
	.black_level    =  0.00,
	.blanking_level =  0.00,
	.sync_level     = -0.30,
	
	/* These are copied from the NTSC values */
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
};

const vid_config_t vid_config_cbs405_m = {
	
	/* System M (CBS 405-line Colour) */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.modulation     = VID_VSB,
	.vsb_upper_bw   = 4200000, /* Hz */
	.vsb_lower_bw   =  750000, /* Hz */
	
	.level          = 1.0, /* Overall signal level */
	
	.video_level    = 0.83, /* Power level of video */
	.fm_mono_level  = 0.17, /* FM audio carrier power level */
	
	.type           = VID_CBS_405,
	.frame_rate_num = 72,
	.frame_rate_den = 1,
	.lines          = 405,
	.hline          = 203,
	
	.active_lines   = 376, /* Estimate */
	.active_width   = 0.00002812, /* 28.12µs */
	.active_left    = 0.00000480, /* |-->| 4.80µs */
	
	.hsync_width       = 0.000002743, /*  2.743µs */
	.vsync_short_width = 0.000001372, /*  1.372µs */
	.vsync_long_width  = 0.000014746, /* 14.746µs */
	
	.white_level    = 0.159, /* 15% +0/-15 */
	.black_level    = 0.595, /* 75% +25/-25 */
	.blanking_level = 0.595, /* 75% +25/-25 */
	.sync_level     = 1.000, /* 100% */
	
	.colour_mode    = VID_CBS_FSC,
	.fsc_flag_left  = 0.000008573, /* |-->| 8.573µs */
	
	.gamma          =  1.0,
	.rw_co          =  0.299, /* R weight */
	.gw_co          =  0.587, /* G weight */
	.bw_co          =  0.114, /* B weight */
	
	.fm_mono_carrier   = 4500000, /* Hz */
	.fm_mono_deviation = 25000, /* +/- Hz */
	.fm_mono_preemph   = 0.000075, /* Seconds */
};

const vid_config_t vid_config_cbs405 = {
	
	/* CBS 405-line Colour */
	.output_type    = HACKTV_INT16_REAL,
	
	.level          = 1.0, /* Overall signal level */
	.video_level    = 1.0, /* Power level of video */
	
	.type           = VID_CBS_405,
	.frame_rate_num = 72,
	.frame_rate_den = 1,
	.lines          = 405,
	.hline          = 203,
	
	.active_lines   = 376, /* Estimate */
	.active_width   = 0.00002812, /* 28.12µs */
	.active_left    = 0.00000480, /* |-->| 4.80µs */
	
	.hsync_width       = 0.000002743, /*  2.743µs */
	.vsync_short_width = 0.000001372, /*  1.372µs */
	.vsync_long_width  = 0.000014746, /* 14.746µs */
	
	.white_level    =  0.70,
	.black_level    =  0.00,
	.blanking_level =  0.00,
	.sync_level     = -0.30,
	
	.colour_mode    = VID_CBS_FSC,
	.fsc_flag_left  = 0.000008573, /* |-->| 8.573µs */
	
	.gamma          =  1.0,
	.rw_co          =  0.299, /* R weight */
	.gw_co          =  0.587, /* G weight */
	.bw_co          =  0.114, /* B weight */
};

const vid_configs_t vid_configs[] = {
	{ "i",             &vid_config_pal_i            },
	{ "b",             &vid_config_pal_bg           },
	{ "g",             &vid_config_pal_bg           },
	{ "pal-fm",        &vid_config_pal_fm           },
	{ "pal",           &vid_config_pal              },
	{ "pal-m",         &vid_config_pal_m            },
	{ "525pal",        &vid_config_525pal           },
	{ "l",             &vid_config_secam_l          },
	{ "d",             &vid_config_secam_dk         },
	{ "k",             &vid_config_secam_dk         },
	{ "secam-fm",      &vid_config_secam_fm         },
	{ "secam",         &vid_config_secam            },
	{ "m",             &vid_config_ntsc_m           },
	{ "ntsc-fm",       &vid_config_ntsc_fm          },
	{ "ntsc-bs",       &vid_config_ntsc_bs_fm       },
	{ "ntsc",          &vid_config_ntsc             },
	{ "d2mac-am",      &vid_config_d2mac_am         },
	{ "d2mac-fm",      &vid_config_d2mac_fm         },
	{ "d2mac",         &vid_config_d2mac            },
	{ "dmac-am",       &vid_config_dmac_am          },
	{ "dmac-fm",       &vid_config_dmac_fm          },
	{ "dmac",          &vid_config_dmac             },
	{ "e",             &vid_config_819_e            },
	{ "819",           &vid_config_819              },
	{ "a",             &vid_config_405_a            },
	{ "405",           &vid_config_405              },
	{ "240-am",        &vid_config_baird_240_am     },
	{ "240",           &vid_config_baird_240        },
	{ "30-am",         &vid_config_baird_30_am      },
	{ "30",            &vid_config_baird_30         },
	{ "nbtv-am",       &vid_config_nbtv_32_am       },
	{ "nbtv",          &vid_config_nbtv_32          },
	{ "apollo-fsc-fm", &vid_config_apollo_colour_fm },
	{ "apollo-fsc",    &vid_config_apollo_colour    },
	{ "apollo-fm",     &vid_config_apollo_mono_fm   },
	{ "apollo",        &vid_config_apollo_mono      },
	{ "m-cbs405",      &vid_config_cbs405_m         },
	{ "cbs405",        &vid_config_cbs405           },
	{ NULL,            NULL },
};

/* Video filter process */
typedef struct {
	
	int lines;
	int offset;
	
	fir_int16_t fir;
	
} _vid_filter_process_t;

/* Test taps for a CCIR-405 625 line video pre-emphasis filter at 28 MHz */
const int16_t fm_625_28_taps[] = {
	-1,-6,-4,8,19,6,-31,-47,-2,73,79,-35,-167,-135,88,264,115,-302,-537,-221,386,510,-277,-1362,-1545,-446,647,-226,-3473,-6776,-6617,-1514,6078,11329,10904,5674,-213,-2937,-1902,512,1641,842,-541,-1003,-351,469,601,104,-357,-336,9,242,168,-46,-146,-72,44,76,24,-29,-32,-4,14,10,0,-4,-1
};

/* Test taps for a CCIR-405 625 line video pre-emphasis filter at 20.25 MHz */
const int16_t fm_625_2025_taps[] = {
	-5,3,12,-11,-20,29,26,-59,-24,104,1,-161,49,218,-147,-273,284,271,-516,-267,704,-5,-1212,-1,1051,-1396,-2808,204,-319,-8341,-10028,4147,17888,12980,-921,-4238,1253,2195,-1265,-1186,1141,579,-951,-200,736,-25,-528,139,346,-178,-203,169,101,-136,-37,96,3,-60,11,32,-12,-14,9,5,-4,-1
};

/* Test taps for a CCIR-405 625 line video pre-emphasis filter at 14 MHz */
const int16_t fm_625_14_taps[] = {
	-3,8,-13,11,3,-26,49,-55,28,33,-109,159,-140,30,147,-319,382,-260,-55,456,-772,764,-391,-443,1154,-1973,1170,-1141,-3425,1273,-13749,-4376,26113,8595,-6484,3767,-1150,-777,1709,-1686,1025,-161,-521,810,-701,346,48,-313,380,-277,95,69,-154,150,-88,13,39,-56,44,-19,-2,12,-12,7,-2
};

/* Test taps for a CCIR-405 525 line video pre-emphasis filter at 18 MHz */
const int16_t fm_525_taps[] = {
	3,0,-10,-3,19,1,-42,-10,63,3,-121,-30,151,0,-286,-83,297,-34,-623,-232,483,-196,-1365,-683,592,-819,-3270,-2143,332,-3015,-9777,-7232,7609,19200,14206,691,-4737,-379,2731,247,-1821,-173,1286,125,-930,-91,675,66,-487,-47,346,33,-241,-22,162,15,-105,-9,65,5,-37,-3,19,1,-8,0,2
};

/* Test taps for D/D2-MAC pre-emphasis at 20.25 MHz */
const int16_t fm_mac_taps[] = {
	-2,5,-8,10,-11,8,1,-16,38,-65,93,-115,124,-111,69,5,-112,239,-382,498,-596,585,-543,267,-54,-622,837,-2115,1741,-4241,1702,-7544,-1383,32606,4417,-4102,3619,-3012,2335,-1645,995,-429,-21,342,-531,603,-580,490,-363,225,-98,-5,75,-114,124,-113,90,-62,35,-14,-1,9,-11,10,-7,4,-2
};

/* 32khz low pass filter taps, with no, 50us and 75us pre-emphasis taps */
const int32_t fm_audio_flat_taps[65] = {
	0, 1, -4, 8, -14, 21, -28, 34, -36, 31, -16, -10, 49, -99, 158, -220, 275, -314, 323, -292, 208, -63, -147, 421, -751, 1123, -1520, 1917, -2290, 2612, -2861, 3018, 29695, 3018, -2861, 2612, -2290, 1917, -1520, 1123, -751, 421, -147, -63, 208, -292, 323, -314, 275, -220, 158, -99, 49, -10, -16, 31, -36, 34, -28, 21, -14, 8, -4, 1, 0
};

const int32_t fm_audio_50us_taps[65] = {
	66, -84, 104, -118, 114, -76, -17, 186, -448, 812, -1269, 1790, -2321, 2781, -3064, 3049, -2610, 1636, -51, -2161, 4936, -8110, 11401, -14403, 16587, -17302, 15790, -11200, 2590, 11061, -30873, 58092, -94151, 62881, 50994, -43868, 36151, -28284, 20686, -13723, 7677, -2737, -1015, 3592, -5091, 5674, -5539, 4903, -3972, 2927, -1913, 1029, -334, -155, 448, -577, 584, -514, 405, -288, 184, -104, 50, -18, 4
};

const int32_t fm_audio_75us_taps[65] = {
	102, -130, 160, -180, 172, -110, -38, 304, -713, 1277, -1982, 2782, -3591, 4282, -4693, 4637, -3923, 2383, 93, -3525, 7808, -12679, 17698, -22234, 25467, -26393, 23847, -16517, 2960, 18394, -49266, 91575, -147527, 81111, 77348, -66394, 54574, -42563, 30997, -20426, 11278, -3828, -1804, 5646, -7854, 8676, -8422, 7419, -5981, 4384, -2843, 1507, -462, -267, 699, -885, 888, -776, 608, -431, 274, -154, 73, -27, 6
};

static double _dlimit(double v, double min, double max)
{
	if(v < min) return(min);
	if(v > max) return(max);
	return(v);
}

static int16_t *_burstwin(unsigned int sample_rate, double width, double rise, double level, int *len)
{
	int16_t *win;
	double t;
	int i;
	
	*len = ceil(sample_rate * (width + rise));
	win = malloc(*len * sizeof(int16_t));
	if(!win)
	{
		return(NULL);
	}
	
	for(i = 0; i < *len; i++)
	{
		t = 1.0 / sample_rate * i;
		
		if(t < rise)
		{
			win[i] = round((0.5 - cos(t / rise * M_PI) / 2) * level * INT16_MAX);
		}
		else if(t >= width)
		{
			t -= width;
			win[i] = round((0.5 + cos(t / rise * M_PI) / 2) * level * INT16_MAX);
		}
		else
		{
			win[i] = round(level * INT16_MAX);
		}
	}
	
	return(win);
}

static int16_t *_colour_subcarrier_phase(vid_t *s, int frame, int line, int phase)
{
	int p;
	
	frame = (frame - 1) & 3;
	line = line - 1;
	
	/* Limit phase offset to 0 > 359 */
	if((phase %= 360) < 0)
	{
		phase += 360;
	}
	
	/* Find the position in the table for 0 degrees at the start of this line */
	p = (s->conf.lines * frame + line) * s->width;
	
	/* And apply the offset for the required phase */
	if(phase == 0)
	{
		p += 0;
	}
	else if(phase == 45)
	{
		p += s->colour_lookup_width * 3 / 8;
	}
	else if(phase == 90)
	{
		p += s->colour_lookup_width * 6 / 8;
	}
	else if(phase == 135)
	{
		p += s->colour_lookup_width * 1 / 8;
	}
	else if(phase == 180)
	{
		p += s->colour_lookup_width * 4 / 8;
	}
	else if(phase == 225)
	{
		p += s->colour_lookup_width * 7 / 8;
	}
	else if(phase == 270)
	{
		p += s->colour_lookup_width * 2 / 8;
	}
	else if(phase == 315)
	{
		p += s->colour_lookup_width * 5 / 8;
	}
	
	/* Keep the position within the buffer */
	p %= s->colour_lookup_width;
	
	/* Return a pointer to the line */
	return(&s->colour_lookup[p]);
}

void vid_get_colour_subcarrier(vid_t *s, int frame, int line, int16_t **pb, int16_t **pi, int16_t **pq)
{
	int16_t *b = NULL;
	int16_t *i = NULL;
	int16_t *q = NULL;
	int odd = (frame + line + 1) & 1;
	
	if(s->conf.colour_mode == VID_PAL)
	{
		b = _colour_subcarrier_phase(s, frame, line, odd ? -135 : 135);
		i = _colour_subcarrier_phase(s, frame, line, odd ? -90 : 90);
		q = _colour_subcarrier_phase(s, frame, line, 0);
	}
	else if(s->conf.colour_mode == VID_NTSC)
	{
		b = _colour_subcarrier_phase(s, frame, line, 180);
		i = _colour_subcarrier_phase(s, frame, line, 90);
		q = _colour_subcarrier_phase(s, frame, line, 0);
	}
	
	if(pb) *pb = b;
	if(pi) *pi = i;
	if(pq) *pq = q;
}

/* FM modulator
 * deviation = peak deviation in Hz (+/-) from frequency */
static int _init_fm_modulator(_mod_fm_t *fm, int sample_rate, double frequency, double deviation, double level)
{
	int r;
	double d;
	
	fm->level   = round(INT16_MAX * level);
	fm->counter = INT16_MAX;
	fm->phase.i = INT16_MAX;
	fm->phase.q = 0;
	fm->lut     = malloc(sizeof(cint32_t) * (UINT16_MAX + 1));
	
	if(!fm->lut)
	{
		return(VID_OUT_OF_MEMORY);
	}
	
	for(r = INT16_MIN; r <= INT16_MAX; r++)
	{
		d = 2.0 * M_PI / sample_rate * (frequency + (double) r / INT16_MAX * deviation);
		
		fm->lut[r - INT16_MIN].i = lround(cos(d) * INT32_MAX);
		fm->lut[r - INT16_MIN].q = lround(sin(d) * INT32_MAX);
	}
	
	return(VID_OK);
}

static void inline _fm_modulator_add(_mod_fm_t *fm, int16_t *dst, int16_t sample)
{
	cint32_mul(&fm->phase, &fm->phase, &fm->lut[sample - INT16_MIN]);
	
	dst[0] += ((fm->phase.i >> 16) * fm->level) >> 15;
	dst[1] += ((fm->phase.q >> 16) * fm->level) >> 15;
	
	/* Correct the amplitude after INT16_MAX samples */
	if(--fm->counter == 0)
	{
		double ra = atan2(fm->phase.q, fm->phase.i);
		
		fm->phase.i = lround(cos(ra) * INT32_MAX);
		fm->phase.q = lround(sin(ra) * INT32_MAX);
		
		fm->counter = INT16_MAX;
	}
}

static void inline _fm_modulator(_mod_fm_t *fm, int16_t *dst, int16_t sample)
{
	cint32_mul(&fm->phase, &fm->phase, &fm->lut[sample - INT16_MIN]);
	
	dst[0] = ((fm->phase.i >> 16) * fm->level) >> 15;
	dst[1] = ((fm->phase.q >> 16) * fm->level) >> 15;
	
	/* Correct the amplitude after INT16_MAX samples */
	if(--fm->counter == 0)
	{
		double ra = atan2(fm->phase.q, fm->phase.i);
		
		fm->phase.i = lround(cos(ra) * INT32_MAX);
		fm->phase.q = lround(sin(ra) * INT32_MAX);
		
		fm->counter = INT16_MAX;
	}
}

static void _free_fm_modulator(_mod_fm_t *fm)
{
	free(fm->lut);
}

/* AM modulator */
static int _init_am_modulator(_mod_am_t *am, int sample_rate, double frequency, double level)
{
	double d;
	
	am->level   = round(INT16_MAX * level);
	am->counter = INT16_MAX;
	am->phase.i = INT16_MAX;
	am->phase.q = 0;
	
	d = 2.0 * M_PI / sample_rate * frequency;
	am->delta.i = lround(cos(d) * INT32_MAX);
	am->delta.q = lround(sin(d) * INT32_MAX);
	
	return(VID_OK);
}

static void inline _am_modulator_add(_mod_am_t *am, int16_t *dst, int16_t sample)
{
	cint32_mul(&am->phase, &am->phase, &am->delta);
	
	sample = ((int32_t) sample + INT16_MIN) / 2;
	
	dst[0] += ((((am->phase.i >> 16) * sample) >> 16) * am->level) >> 15;
	dst[1] += ((((am->phase.q >> 16) * sample) >> 16) * am->level) >> 15;
	
	/* Correct the amplitude after INT16_MAX samples */
	if(--am->counter == 0)
	{
		double ra = atan2(am->phase.q, am->phase.i);
		
		am->phase.i = lround(cos(ra) * INT32_MAX);
		am->phase.q = lround(sin(ra) * INT32_MAX);
		
		am->counter = INT16_MAX;
	}
}

static void _free_am_modulator(_mod_am_t *am)
{
	/* Nothing */
}

/* AV source callback handlers */
static uint32_t *_av_read_video(vid_t *s, float *ratio)
{
	if(s->av_read_video)
	{
		return(s->av_read_video(s->av_private, ratio));
	}
	
	return(NULL);
}

static int16_t *_av_read_audio(vid_t *s, size_t *samples)
{
	if(s->av_read_audio)
	{
		return(s->av_read_audio(s->av_private, samples));
	}
	
	return(NULL);
}

static int _av_eof(vid_t *s)
{
	if(s->av_eof)
	{
		return(s->av_eof(s->av_private));
	}
	
	return(0);
}

int vid_av_close(vid_t *s)
{
	int r;
	
	r = s->av_close ? s->av_close(s->av_private) : VID_ERROR;
	
	s->av_private = NULL;
	s->av_read_video = NULL;
	s->av_read_audio = NULL;
	s->av_eof = NULL;
	s->av_close = NULL;
	
	return(r);
}

void _test_sample_rate(const vid_config_t *conf, unsigned int sample_rate)
{
	int m, r;
	
	/* Test if the chosen sample rate results in an even number of
	 * samples per line. If not, display a warning and show the
	 * previous and next valid sample rates. */
	
	/* Calculate lowest valid sample rate */
	m = conf->lines * conf->frame_rate_num;
	m /= r = gcd(m, conf->frame_rate_den);
	if(conf->frame_rate_den / r & 1) m *= 2;
	
	/* Is the chosen sample rate good? */
	if(sample_rate % m == 0) return;
	
	/* Not really. Suggest some good sample rates */
	r = sample_rate / m;
	fprintf(stderr, "Warning: Sample rate %u may not work well with this mode.\n", sample_rate);
	fprintf(stderr, "Next valid sample rates: %u, %u\n", m * r, m * (r + 1));
}

static int _vid_next_line_raster(vid_t *s, void *arg, int nlines, vid_line_t **lines)
{
	const char *seq;
	int x;
	int vy;
	int w;
	uint32_t rgb;
	int pal = 0;
	int fsc = 0;
	int16_t *lut_b = NULL;
	int16_t *lut_i = NULL;
	int16_t *lut_q = NULL;
	vid_line_t *l = lines[0];
	
	l->frame    = s->bframe;
	l->line     = s->bline;
	l->vbialloc = 0;
	
	/* Sequence codes: abcd
	 * 
	 * a: first sync
	 *    h = horizontal sync pulse
	 *    v = short vertical sync pulse
	 *    V = long vertical sync pulse
	 *    _ = no sync pulse
	 * 
	 * b: colour burst
	 *    0 = line always has a colour burst
	 *    _ = line never has a colour burst
	 *    1 = line has a colour burst on odd frames
	 *    2 = line has a colour burst on even frames
	 * 
	 * c: left content
	 *    _ = blanking
	 *    a = active video
	 * 
	 * d: right content
	 *    _ = blanking
	 *    a = active video
	 *    v = short vertical sync pulse
	 *    V = long vertical sync pulse
	 * 
	 **** I don't like this code, it's overly complicated for all it does.
	*/
	
	vy = -1;
	seq = "____";
	
	if(s->conf.type == VID_RASTER_625)
	{
		switch(l->line)
		{
		case 1:   seq = "V__V"; break;
		case 2:   seq = "V__V"; break;
		case 3:   seq = "V__v"; break;
		case 4:   seq = "v__v"; break;
		case 5:   seq = "v__v"; break;
		case 6:   seq = "h1__"; break;
		case 7:   seq = "h0__"; break;
		case 8:   seq = "h0__"; break;
		case 9:   seq = "h0__"; break;
		case 10:  seq = "h0__"; break;
		case 11:  seq = "h0__"; break;
		case 12:  seq = "h0__"; break;
		case 13:  seq = "h0__"; break;
		case 14:  seq = "h0__"; break;
		case 15:  seq = "h0__"; break;
		case 16:  seq = "h0__"; break;
		case 17:  seq = "h0__"; break;
		case 18:  seq = "h0__"; break;
		case 19:  seq = "h0__"; break;
		case 20:  seq = "h0__"; break;
		case 21:  seq = "h0__"; break;
		case 22:  seq = "h0__"; break;
		case 23:  seq = "h0_a"; break;
		
		case 310: seq = "h1aa"; break;
		case 311: seq = "v__v"; break;
		case 312: seq = "v__v"; break;
		case 313: seq = "v__V"; break;
		case 314: seq = "V__V"; break;
		case 315: seq = "V__V"; break;
		case 316: seq = "v__v"; break;
		case 317: seq = "v__v"; break;
		case 318: seq = "v___"; break;
		case 319: seq = "h2__"; break;
		case 320: seq = "h0__"; break;
		case 321: seq = "h0__"; break;
		case 322: seq = "h0__"; break;
		case 323: seq = "h0__"; break;
		case 324: seq = "h0__"; break;
		case 325: seq = "h0__"; break;
		case 326: seq = "h0__"; break;
		case 327: seq = "h0__"; break;
		case 328: seq = "h0__"; break;
		case 329: seq = "h0__"; break;
		case 330: seq = "h0__"; break;
		case 331: seq = "h0__"; break;
		case 332: seq = "h0__"; break;
		case 333: seq = "h0__"; break;
		case 334: seq = "h0__"; break;
		case 335: seq = "h0__"; break;
		
		case 622: seq = "h1aa"; break;
		case 623: seq = "h_av"; break;
		case 624: seq = "v__v"; break;
		case 625: seq = "v__v"; break;
		
		default:  seq = "h0aa"; break;
		}
		
		/* Calculate the active line number */
		vy = (l->line < 313 ? (l->line - 23) * 2 : (l->line - 336) * 2 + 1);
	}
	else if(s->conf.type == VID_RASTER_525)
	{
		switch(l->line)
		{
		case 1:   seq = "v__v"; break;
		case 2:   seq = "v__v"; break;
		case 3:   seq = "v__v"; break;
		case 4:   seq = "V__V"; break;
		case 5:   seq = "V__V"; break;
		case 6:   seq = "V__V"; break;
		case 7:   seq = "v__v"; break;
		case 8:   seq = "v__v"; break;
		case 9:   seq = "v__v"; break;
		case 10:  seq = "h0__"; break;
		case 11:  seq = "h0__"; break;
		case 12:  seq = "h0__"; break;
		case 13:  seq = "h0__"; break;
		case 14:  seq = "h0__"; break;
		case 15:  seq = "h0__"; break;
		case 16:  seq = "h0__"; break;
		case 17:  seq = "h0__"; break;
		case 18:  seq = "h0__"; break;
		case 19:  seq = "h0__"; break;
		case 20:  seq = "h0__"; break;
		
		case 263: seq = "h0av"; break;
		case 264: seq = "v__v"; break;
		case 265: seq = "v__v"; break;
		case 266: seq = "v__V"; break;
		case 267: seq = "V__V"; break;
		case 268: seq = "V__V"; break;
		case 269: seq = "V__v"; break;
		case 270: seq = "v__v"; break;
		case 271: seq = "v__v"; break;
		case 272: seq = "v___"; break;
		case 273: seq = "h0__"; break;
		case 274: seq = "h0__"; break;
		case 275: seq = "h0__"; break;
		case 276: seq = "h0__"; break;
		case 277: seq = "h0__"; break;
		case 278: seq = "h0__"; break;
		case 279: seq = "h0__"; break;
		case 280: seq = "h0__"; break;
		case 281: seq = "h0__"; break;
		case 282: seq = "h0__"; break;
		case 283: seq = "h0_a"; break;
		
		default:  seq = "h0aa"; break;
		}
		
		/* Calculate the active line number */
		
		/* There are 486 lines in this mode with some active video,
		 * but encoded files normally only have 480 of these. Here
		 * we use the line numbers suggested by SMPTE Recommended
		 * Practice RP-202. Lines 23-262 from the first field and
		 * 286-525 from the second. */
		
		vy = (l->line < 265 ? (l->line - 23) * 2 : (l->line - 286) * 2 + 1);
	}
	else if(s->conf.type == VID_RASTER_819)
	{
		switch(l->line)
		{
		case 817: seq = "h___"; break;
		case 818: seq = "h___"; break;
		case 819: seq = "h___"; break;
		case 1:   seq = "V___"; break;
		case 2:   seq = "h___"; break;
		case 3:   seq = "h___"; break;
		case 4:   seq = "h___"; break;
		case 5:   seq = "h___"; break;
		case 6:   seq = "h___"; break;
		case 7:   seq = "h___"; break;
		case 8:   seq = "h___"; break;
		case 9:   seq = "h___"; break;
		case 10:  seq = "h___"; break;
		case 11:  seq = "h___"; break;
		case 12:  seq = "h___"; break;
		case 13:  seq = "h___"; break;
		case 14:  seq = "h___"; break;
		case 15:  seq = "h___"; break;
		case 16:  seq = "h___"; break;
		case 17:  seq = "h___"; break;
		case 18:  seq = "h___"; break;
		case 19:  seq = "h___"; break;
		case 20:  seq = "h___"; break;
		case 21:  seq = "h___"; break;
		case 22:  seq = "h___"; break;
		case 23:  seq = "h___"; break;
		case 24:  seq = "h___"; break;
		case 25:  seq = "h___"; break;
		case 26:  seq = "h___"; break;
		case 27:  seq = "h___"; break;
		case 28:  seq = "h___"; break;
		case 29:  seq = "h___"; break;
		case 30:  seq = "h___"; break;
		case 31:  seq = "h___"; break;
		case 32:  seq = "h___"; break;
		case 33:  seq = "h___"; break;
		case 34:  seq = "h___"; break;
		case 35:  seq = "h___"; break;
		case 36:  seq = "h___"; break;
		case 37:  seq = "h___"; break;
		case 38:  seq = "h___"; break;
		
		case 406: seq = "h_a_"; break;
		case 407: seq = "h___"; break;
		case 408: seq = "h___"; break;
		case 409: seq = "h__V"; break;
		case 410: seq = "h___"; break;
		case 411: seq = "h___"; break;
		case 412: seq = "h___"; break;
		case 413: seq = "h___"; break;
		case 414: seq = "h___"; break;
		case 415: seq = "h___"; break;
		case 416: seq = "h___"; break;
		case 417: seq = "h___"; break;
		case 418: seq = "h___"; break;
		case 419: seq = "h___"; break;
		case 420: seq = "h___"; break;
		case 421: seq = "h___"; break;
		case 422: seq = "h___"; break;
		case 423: seq = "h___"; break;
		case 424: seq = "h___"; break;
		case 425: seq = "h___"; break;
		case 426: seq = "h___"; break;
		case 427: seq = "h___"; break;
		case 428: seq = "h___"; break;
		case 429: seq = "h___"; break;
		case 430: seq = "h___"; break;
		case 431: seq = "h___"; break;
		case 432: seq = "h___"; break;
		case 433: seq = "h___"; break;
		case 434: seq = "h___"; break;
		case 435: seq = "h___"; break;
		case 436: seq = "h___"; break;
		case 437: seq = "h___"; break;
		case 438: seq = "h___"; break;
		case 439: seq = "h___"; break;
		case 440: seq = "h___"; break;
		case 441: seq = "h___"; break;
		case 442: seq = "h___"; break;
		case 443: seq = "h___"; break;
		case 444: seq = "h___"; break;
		case 445: seq = "h___"; break;
		case 446: seq = "h___"; break;
		case 447: seq = "h__a"; break;
		
		default:  seq = "h_aa"; break;
		}
		
		/* Calculate the active line number */
		vy = (l->line < 406 ? (l->line - 48) * 2 : (l->line - 457) * 2 + 1);
	}
	else if(s->conf.type == VID_RASTER_405)
	{
		switch(l->line)
		{
		case 1:   seq = "V__V"; break;
		case 2:   seq = "V__V"; break;
		case 3:   seq = "V__V"; break;
		case 4:   seq = "V__V"; break;
		case 5:   seq = "h___"; break;
		case 6:   seq = "h___"; break;
		case 7:   seq = "h___"; break;
		case 8:   seq = "h___"; break;
		case 9:   seq = "h___"; break;
		case 10:  seq = "h___"; break;
		case 11:  seq = "h___"; break;
		case 12:  seq = "h___"; break;
		case 13:  seq = "h___"; break;
		case 14:  seq = "h___"; break;
		case 15:  seq = "h___"; break;
		
		case 203: seq = "h_aV"; break;
		case 204: seq = "V__V"; break;
		case 205: seq = "V__V"; break;
		case 206: seq = "V__V"; break;
		case 207: seq = "V___"; break;
		case 208: seq = "h___"; break;
		case 209: seq = "h___"; break;
		case 210: seq = "h___"; break;
		case 211: seq = "h___"; break;
		case 212: seq = "h___"; break;
		case 213: seq = "h___"; break;
		case 214: seq = "h___"; break;
		case 215: seq = "h___"; break;
		case 216: seq = "h___"; break;
		case 217: seq = "h___"; break;
		case 218: seq = "h__a"; break;
		
		default:  seq = "h_aa"; break;
		}
		
		/* Calculate the active line number */
		vy = (l->line < 210 ? (l->line - 16) * 2 : (l->line - 219) * 2 + 1);
	}
	else if(s->conf.type == VID_CBS_405)
	{
		switch(l->line)
		{
		case 1:   seq = "v__v"; break;
		case 2:   seq = "v__v"; break;
		case 3:   seq = "v__v"; break;
		case 4:   seq = "V__V"; break;
		case 5:   seq = "V__V"; break;
		case 6:   seq = "V__V"; break;
		case 7:   seq = "v__v"; break;
		case 8:   seq = "v__v"; break;
		case 9:   seq = "v__v"; break;
		case 10:  seq = "h___"; break;
		case 11:  seq = "h___"; break;
		case 12:  seq = "h___"; break;
		case 13:  seq = "h___"; break;
		case 14:  seq = "h___"; break;
		
		case 203: seq = "h_av"; break;
		case 204: seq = "v__v"; break;
		case 205: seq = "v__v"; break;
		case 206: seq = "v__V"; break;
		case 207: seq = "V__V"; break;
		case 208: seq = "V__V"; break;
		case 209: seq = "V__v"; break;
		case 210: seq = "v__v"; break;
		case 211: seq = "v__v"; break;
		case 212: seq = "v___"; break;
		case 213: seq = "h___"; break;
		case 214: seq = "h___"; break;
		case 215: seq = "h___"; break;
		case 216: seq = "h___"; break;
		case 217: seq = "h__a"; break;
		
		default:  seq = "h_aa"; break;
		}
		
		/* Calculate the active line number */
		vy = (l->line < 210 ? (l->line - 16) * 2 : (l->line - 219) * 2 + 1);
	}
	else if(s->conf.type == VID_APOLLO_320)
	{
		if(l->line <= 8) seq = "V__v";
		else seq = "h_aa";
		
		vy = l->line - 9;
		if(vy < 0 || vy >= s->conf.active_lines) vy = -1;
	}
	else if(s->conf.type == VID_BAIRD_240)
	{
		switch(l->line)
		{
		case 1:   seq = "V__V"; break;
		case 2:   seq = "V__V"; break;
		case 3:   seq = "V__V"; break;
		case 4:   seq = "V__V"; break;
		case 5:   seq = "V__V"; break;
		case 6:   seq = "V__V"; break;
		case 7:   seq = "V__V"; break;
		case 8:   seq = "V__V"; break;
		case 9:   seq = "V__V"; break;
		case 10:  seq = "V__V"; break;
		case 11:  seq = "V__V"; break;
		case 12:  seq = "V__V"; break;
		case 13:  seq = "h___"; break;
		case 14:  seq = "h___"; break;
		case 15:  seq = "h___"; break;
		case 16:  seq = "h___"; break;
		case 17:  seq = "h___"; break;
		case 18:  seq = "h___"; break;
		case 19:  seq = "h___"; break;
		case 20:  seq = "h___"; break;
		
		default:  seq = "h_aa"; break;
		}
		
		/* Calculate the active line number */
		vy = l->line - 20;
	}
	else if(s->conf.type == VID_BAIRD_30)
	{
		/* The original Baird 30 line standard has no sync pulses */
		seq = "__aa";
		vy = l->line - 1;
	}
	else if(s->conf.type == VID_NBTV_32)
	{
		switch(l->line)
		{
		case 1:  seq = "__aa"; break;
		default: seq = "h_aa"; break;
		}
		
		vy = l->line - 1;
	}
	
	if(vy < 0 || vy >= s->conf.active_lines) vy = -1;
	
	if(s->conf.colour_mode == VID_PAL ||
	   s->conf.colour_mode == VID_NTSC)
	{
		/* Does this line use colour? */
		pal  = seq[1] == '0';
		pal |= seq[1] == '1' && (l->frame & 1) == 1;
		pal |= seq[1] == '2' && (l->frame & 1) == 0;
		
		/* Calculate colour sub-carrier lookup-positions for the start of this line */
		vid_get_colour_subcarrier(s, l->frame, l->line, &lut_b, &lut_i, &lut_q);
	}
	if(s->conf.colour_mode == VID_APOLLO_FSC)
	{
		/* Apollo Field Sequential Colour */
		fsc = (l->frame * 2 + (l->line < 264 ? 0 : 1)) % 3;
		pal = 0;
	}
	else if(s->conf.colour_mode == VID_CBS_FSC)
	{
		/* CBS Field Sequential Colour */
		fsc = (l->frame * 2 + (l->line < 202 ? 0 : 1)) % 3;
		pal = 0;
	}
	
	/* Render the left side sync pulse */
	if(seq[0] == 'v') w = s->vsync_short_width;
	else if(seq[0] == 'V') w = s->vsync_long_width;
	else if(seq[0] == 'h') w = s->hsync_width;
	else w = 0;
	
	for(x = 0; x < w && x < s->half_width; x++)
	{
		l->output[x * 2] = s->sync_level;
	}
	
	/* Render left side of active video if required */
	if(seq[2] == 'a' && vy != -1)
	{
		for(; x < s->active_left; x++)
		{
			l->output[x * 2] = s->blanking_level;
		}
		
		for(; x < s->half_width; x++)
		{
			rgb = s->framebuffer != NULL ? s->framebuffer[vy * s->active_width + x - s->active_left] & 0xFFFFFF : 0x000000;
			
			if(s->conf.colour_mode == VID_APOLLO_FSC ||
			   s->conf.colour_mode == VID_CBS_FSC)
			{
				rgb  = (rgb >> (8 * fsc)) & 0xFF;
				rgb |= (rgb << 8) | (rgb << 16);
			}
			
			l->output[x * 2] = s->yiq_level_lookup[rgb].y;
			
			if(pal)
			{
				l->output[x * 2] += (s->yiq_level_lookup[rgb].i * lut_i[x]) >> 15;
				l->output[x * 2] += (s->yiq_level_lookup[rgb].q * lut_q[x]) >> 15;
			}
		}
	}
	else
	{
		for(; x < s->half_width; x++)
		{
			l->output[x * 2] = s->blanking_level;
		}
	}
	
	if(seq[3] == 'a' && vy != -1)
	{
		for(; x < s->active_left + s->active_width; x++)
		{
			rgb = s->framebuffer != NULL ? s->framebuffer[vy * s->active_width + x - s->active_left] & 0xFFFFFF : 0x000000;
			
			if(s->conf.colour_mode == VID_APOLLO_FSC ||
			   s->conf.colour_mode == VID_CBS_FSC)
			{
				rgb  = (rgb >> (8 * fsc)) & 0xFF;
				rgb |= (rgb << 8) | (rgb << 16);
			}
			
			l->output[x * 2] = s->yiq_level_lookup[rgb].y;
			
			if(pal)
			{
				l->output[x * 2] += (s->yiq_level_lookup[rgb].i * lut_i[x]) >> 15;
				l->output[x * 2] += (s->yiq_level_lookup[rgb].q * lut_q[x]) >> 15;
			}
		}
	}
	else
	{
		if(seq[3] == 'v') w = s->vsync_short_width;
		else if(seq[3] == 'V') w = s->vsync_long_width;
		else w = 0;
		
		for(; x < s->half_width + w && x < s->width; x++)
		{
			l->output[x * 2] = s->sync_level;
		}
	}
	
	/* Blank the remainder of the line */
	for(; x < s->width; x++)
	{
		l->output[x * 2] = s->blanking_level;
	}
	
	/* Render the colour burst */
	if(pal)
	{
		for(x = s->burst_left; x < s->burst_left + s->burst_width; x++)
		{
			l->output[x * 2] += (lut_b[x] * s->burst_win[x - s->burst_left]) >> 15;
		}
	}
	
	/* Render the FSC flag */
	if(s->conf.colour_mode == VID_APOLLO_FSC && fsc == 1 &&
	  (l->line == 18 || l->line == 281))
	{
		/* The Apollo colour standard transmits one colour per field
		 * (Blue, Red, Green), with the green field indicated by a flag
		 * on field line 18. The flag also indicates the temperature of
		 * the camera by its duration, varying between 5 and 45 μs. The
		 * duration is fixed to 20 μs in hacktv. */
		for(x = s->fsc_flag_left; x < s->fsc_flag_left + s->fsc_flag_width; x++)
		{
			l->output[x * 2] = s->fsc_flag_level;
		}
	}
	
	/* Render the CBS FSC flag */
	if(s->conf.colour_mode == VID_CBS_FSC && fsc == 2 &&
	  (l->line == 1 || l->line == 203))
	{
		w = (l->line == 1 ? s->fsc_flag_left : s->half_width + s->fsc_flag_left);
		for(x = 0; x < s->vsync_short_width; x++)
		{
			l->output[(w + x) * 2] = s->sync_level;
		}
	}
	
	/* Render the SECAM colour subcarrier */
	if(s->conf.colour_mode == VID_SECAM &&
	   (seq[2] == 'a' || seq[3] == 'a'))
	{
		x = s->active_left;
		w = x + s->active_width;
		
		if(seq[2] != 'a') x = s->half_width;
		if(seq[3] != 'a') w = s->half_width;
		
		x -= s->active_left - s->burst_left;
		
		for(; x < w; x++)
		{
			rgb = 0x000000;
			
			if(x >= s->active_left && x < s->active_left + s->active_width)
			{
				rgb = s->framebuffer != NULL ? s->framebuffer[vy * s->active_width + x - s->active_left] & 0xFFFFFF : 0x000000;
			}
			
			if(((l->frame * s->conf.lines) + l->line) & 1)
			{
				_fm_modulator_add(&s->fm_secam_cb, &l->output[x * 2], s->yiq_level_lookup[rgb].q);
			}
			else
			{
				_fm_modulator_add(&s->fm_secam_cr, &l->output[x * 2], s->yiq_level_lookup[rgb].i);
			}
		}
	}
	
	/* Clear the Q channel */
	for(x = 0; x < s->width; x++)
	{
		l->output[x * 2 + 1] = 0;
	}
	
	return(1);
}

static int _vid_filter_process(vid_t *s, void *arg, int nlines, vid_line_t **lines)
{
	_vid_filter_process_t *p = arg;
	vid_line_t *src = lines[nlines - 1];
	int x1, x2;
	
	x1 = p->offset;
	x2 = s->width - x1;
	
	fir_int16_process(&p->fir, &lines[0]->output[x1 * 2], &src->output[0], x2);
	fir_int16_process(&p->fir, &lines[1]->output[0], &src->output[x2 * 2], x1);
	
	return(1);
}

static void _vid_filter_free(vid_t *s, void *arg)
{
	_vid_filter_process_t *p = arg;
	
	fir_int16_free(&p->fir);
	free(p);
}

static int _vid_audio_process(vid_t *s, void *arg, int nlines, vid_line_t **lines)
{
	vid_line_t *l = lines[0];
	int16_t audio[2] = { 0, 0 };
	static int interp = 0;
	int x;
	
	for(x = 0; x < s->width; x++)
	{
		int16_t add[2] = { 0, 0 };
		
		/* TODO: Replace this with a real FIR filter... */
		interp += HACKTV_AUDIO_SAMPLE_RATE;
		if(interp >= s->sample_rate)
		{
			interp -= s->sample_rate;
			
			if(s->audiobuffer_samples == 0)
			{
				s->audiobuffer = _av_read_audio(s, &s->audiobuffer_samples);
				
				if(s->conf.systeraudio == 1)
				{
					ng_invert_audio(&s->ng, s->audiobuffer, s->audiobuffer_samples);
				}
			}
			
			if(s->audiobuffer)
			{
				/* Fetch next sample */
				audio[0] = s->audiobuffer[0];
				audio[1] = s->audiobuffer[1];
				s->audiobuffer += 2;
				s->audiobuffer_samples--;
			}
			else
			{
				/* No audio from the source */
				audio[0] = 0;
				audio[1] = 0;
			}
			
			if(s->conf.am_audio_level > 0 && s->conf.am_mono_carrier != 0)
			{
				s->am_mono.sample = (audio[0] + audio[1]) / 2;
			}
			
			if(s->conf.fm_mono_level > 0 && s->conf.fm_mono_carrier != 0)
			{
				s->fm_mono.sample = (audio[0] + audio[1]) / 2;
				if(s->fm_mono.limiter.width)
				{
					limiter_process(&s->fm_mono.limiter, &s->fm_mono.sample, &s->fm_mono.sample, &s->fm_mono.sample, 1, 1);
				}
			}
			
			if(s->conf.fm_left_level > 0 && s->conf.fm_left_carrier != 0)
			{
				s->fm_left.sample = audio[0];
				if(s->fm_left.limiter.width)
				{
					limiter_process(&s->fm_left.limiter, &s->fm_left.sample, &s->fm_left.sample, &s->fm_left.sample, 1, 1);
				}
			}
			
			if(s->conf.fm_right_level > 0 && s->conf.fm_right_carrier != 0)
			{
				s->fm_right.sample = audio[1];
				if(s->fm_right.limiter.width)
				{
					limiter_process(&s->fm_right.limiter, &s->fm_right.sample, &s->fm_right.sample, &s->fm_right.sample, 1, 1);
				}
			}
			
			if((s->conf.nicam_level > 0 && s->conf.nicam_carrier != 0) ||
			   s->conf.type == VID_MAC)
			{
				s->nicam_buf[s->nicam_buf_len++] = audio[0];
				s->nicam_buf[s->nicam_buf_len++] = audio[1];
				
				if(s->nicam_buf_len == NICAM_AUDIO_LEN * 2)
				{
					if(s->conf.nicam_level > 0 && s->conf.nicam_carrier != 0)
					{
						nicam_mod_input(&s->nicam, s->nicam_buf);
					}
					
					if(s->conf.type == VID_MAC)
					{
						mac_write_audio(s, s->nicam_buf);
					}
					
					s->nicam_buf_len = 0;
				}
			}
			
			if(s->conf.dance_level > 0 && s->conf.dance_carrier != 0)
			{
				s->dance_buf[s->dance_buf_len++] = audio[0];
				s->dance_buf[s->dance_buf_len++] = audio[1];
				
				if(s->dance_buf_len == DANCE_A_AUDIO_LEN * 2)
				{
					dance_mod_input(&s->dance, s->dance_buf);
					s->dance_buf_len = 0;
				}
			}
		}
		
		if(s->conf.fm_mono_level > 0 && s->conf.fm_mono_carrier != 0)
		{
			_fm_modulator_add(&s->fm_mono, add, s->fm_mono.sample);
		}
		
		if(s->conf.fm_left_level > 0 && s->conf.fm_left_carrier != 0)
		{
			_fm_modulator_add(&s->fm_left, add, s->fm_left.sample);
		}
		
		if(s->conf.fm_right_level > 0 && s->conf.fm_right_carrier != 0)
		{
			_fm_modulator_add(&s->fm_right, add, s->fm_right.sample);
		}
		
		if(s->conf.am_audio_level > 0 && s->conf.am_mono_carrier != 0)
		{
			_am_modulator_add(&s->am_mono, add, s->am_mono.sample);
		}
		
		l->output[x * 2 + 0] += add[0];
		l->output[x * 2 + 1] += add[1];
	}
	
	if(s->conf.nicam_level > 0 && s->conf.nicam_carrier != 0)
	{
		nicam_mod_output(&s->nicam, l->output, s->width);
	}
	
	if(s->conf.dance_level > 0 && s->conf.dance_carrier != 0)
	{
		dance_mod_output(&s->dance, l->output, s->width);
	}
	
	return(1);
}

static int _vid_fmmod_process(vid_t *s, void *arg, int nlines, vid_line_t **lines)
{
	vid_line_t *l = lines[0];
	int x;
	
	/* FM modulate the video and audio if requested */
	for(x = 0; x < s->width; x++)
	{
		_fm_modulator(&s->fm_video, &l->output[x * 2], l->output[x * 2]);
	}
	
	return(1);
}

static int _vid_offset_process(vid_t *s, void *arg, int nlines, vid_line_t **lines)
{
	vid_line_t *l = lines[0];
	int x;
	
	for(x = 0; x < s->width; x++)
	{
		cint16_t a, b;
		
		cint32_mul(&s->offset.phase, &s->offset.phase, &s->offset.delta);
		
		a.i = l->output[x * 2 + 0];
		a.q = l->output[x * 2 + 1];
		b.i = s->offset.phase.i >> 16;
		b.q = s->offset.phase.q >> 16;
		cint16_mul(&a, &a, &b);
		
		l->output[x * 2 + 0] = a.i;
		l->output[x * 2 + 1] = a.q;
		
		/* Correct the amplitude after INT16_MAX samples */
		if(--s->offset.counter == 0)
		{
			double ra = atan2(s->offset.phase.q, s->offset.phase.i);
			
			s->offset.phase.i = lround(cos(ra) * INT32_MAX);
			s->offset.phase.q = lround(sin(ra) * INT32_MAX);
			
			s->offset.counter = INT16_MAX;
		}
	}
	
	return(1);
}

static int _vid_passthru_process(vid_t *s, void *arg, int nlines, vid_line_t **lines)
{
	vid_line_t *l = lines[0];
	int x;
	
	if(feof(s->passthru))
	{
		return(1);
	}
	
	fread(s->passline, sizeof(int16_t) * 2, s->width, s->passthru);
	
	for(x = 0; x < s->width * 2; x++)
	{
		l->output[x] += s->passline[x];
	}
	
	return(1);
}

static int _add_lineprocess(vid_t *s, const char *name, int nlines, void *arg, vid_lineprocess_process_t pprocess, vid_lineprocess_free_t pfree)
{
	_lineprocess_t *p;
	
	p = realloc(s->processes, sizeof(_lineprocess_t) * (s->nprocesses + 1));
	if(!p)
	{
		return(VID_OUT_OF_MEMORY);
	}
	
	s->processes = p;
	p = &s->processes[s->nprocesses++];
	
	strncpy(p->name, name, 15);
	p->vid = s;
	p->nlines = nlines;
	p->arg = arg;
	p->process = pprocess;
	p->free = pfree;
	
	p->lines = calloc(sizeof(vid_line_t *), nlines);
	if(!p->lines)
	{
		return(VID_OUT_OF_MEMORY);
	}
	
	/* Update required line total (non-threaded version) */
	s->olines += nlines - 1;
	
	return(VID_OK);
}

static int _init_vfilter(vid_t *s)
{
	_vid_filter_process_t *p;
	int ntaps;
	
	p = calloc(1, sizeof(_vid_filter_process_t));
	if(!p)
	{
		return(VID_OUT_OF_MEMORY);
	}
	
	if(s->conf.modulation == VID_VSB)
	{
		int16_t *taps;
		
		ntaps = 51;
		
		taps = calloc(ntaps, sizeof(int16_t) * 2);
		if(!taps)
		{
			free(p);
			return(VID_OUT_OF_MEMORY);
		}
		
		fir_int16_complex_band_pass(taps, ntaps, s->sample_rate, -s->conf.vsb_lower_bw, s->conf.vsb_upper_bw, 750000, 1);
		fir_int16_scomplex_init(&p->fir, taps, ntaps);
		free(taps);
	}
	else if(s->conf.modulation == VID_FM)
	{
		const int16_t *taps;
		
		if(s->conf.type == VID_MAC)
		{
			if(s->sample_rate != 20250000)
			{
				fprintf(stderr, "Warning: The D/D2-MAC pre-emphasis filter is designed to run at 20.25 MHz.\n");
			}
			
			taps = fm_mac_taps;
			ntaps = sizeof(fm_mac_taps) / sizeof(int16_t);
		}
		else if(s->conf.lines == 525)
		{
			if(s->sample_rate != 18000000)
			{
				fprintf(stderr, "Warning: The 525-line FM video pre-emphasis filter is designed to run at 18 MHz.\n");
			}
			
			taps = fm_525_taps;
			ntaps = sizeof(fm_525_taps) / sizeof(int16_t);
		}
		else
		{
			if(s->sample_rate == 14000000)
			{
				taps = fm_625_14_taps;
				ntaps = sizeof(fm_625_14_taps) / sizeof(int16_t);
			}
			else if(s->sample_rate == 28000000)
			{
				taps = fm_625_28_taps;
				ntaps = sizeof(fm_625_28_taps) / sizeof(int16_t);
			}
			else
			{
				if(s->sample_rate != 20250000)
				{
					fprintf(stderr, "Warning: The 625-line FM video pre-emphasis filter is designed to run at 20.25 MHz.\n");
				}
				
				taps = fm_625_2025_taps;
				ntaps = sizeof(fm_625_2025_taps) / sizeof(int16_t);
			}
		}
		
		fir_int16_init(&p->fir, taps, ntaps);
	}
	else if(s->conf.modulation == VID_AM ||
	        s->conf.modulation == VID_NONE)
	{
		int16_t *taps;
		
		ntaps = 51;
		
		taps = calloc(ntaps, sizeof(int16_t));
		if(!taps)
		{
			free(p);
			return(VID_OUT_OF_MEMORY);
		}
		
		fir_int16_low_pass(taps, ntaps, s->sample_rate, s->conf.video_bw, 0.75e6, 1);
		fir_int16_init(&p->fir, taps, ntaps);
		free(taps);
	}
	
	if(p->fir.type == 0)
	{
		/* No filter has been created */
		free(p);
		return(VID_OK);
	}
	
	p->lines = 1 + p->fir.ntaps / 2 / s->width;
	p->offset = s->width - ((p->fir.ntaps / 2) % s->width);
	
	_add_lineprocess(s, "vfilter", 1 + p->lines, p, _vid_filter_process, _vid_filter_free);
	
	return(VID_OK);
}

int vid_init(vid_t *s, unsigned int sample_rate, const vid_config_t * const conf)
{
	int r, x;
	int64_t c;
	double d;
	double glut[0x100];
	double level, slevel;
	vid_line_t *l;
	
	/* Seed the system's PRNG, used by some of the video scramblers */
	srand(time(NULL));
	
	memset(s, 0, sizeof(vid_t));
	memcpy(&s->conf, conf, sizeof(vid_config_t));
	
	/* Calculate the number of samples per line */
	_test_sample_rate(&s->conf, sample_rate);
	
	s->width = round((double) sample_rate / ((double) s->conf.frame_rate_num / s->conf.frame_rate_den) / s->conf.lines);
	s->half_width = round((double) sample_rate / ((double) s->conf.frame_rate_num / s->conf.frame_rate_den) / s->conf.lines / 2);
	
	s->sample_rate = sample_rate;
	
	/* Calculate the active video width and offset */
	s->active_left = round(s->sample_rate * s->conf.active_left);
	s->active_width = ceil(s->sample_rate * s->conf.active_width);
	if(s->active_width > s->width) s->active_width = s->width;
	
	s->hsync_width       = round(s->sample_rate * s->conf.hsync_width);
	s->vsync_short_width = round(s->sample_rate * s->conf.vsync_short_width);
	s->vsync_long_width  = round(s->sample_rate * s->conf.vsync_long_width);
	
	/* Calculate signal levels */
	/* slevel is the the sub-carrier level. When FM modulating
	 * this is always 1.0, otherwise it equals the overall level */
	slevel = s->conf.modulation == VID_FM ? 1.0 : s->conf.level;
	
	level = s->conf.video_level * slevel;
	
	/* Calculate 16-bit blank and sync levels */
	s->white_level    = round(s->conf.white_level    * level * INT16_MAX);
	s->black_level    = round(s->conf.black_level    * level * INT16_MAX);
	s->blanking_level = round(s->conf.blanking_level * level * INT16_MAX);
	s->sync_level     = round(s->conf.sync_level     * level * INT16_MAX);
	
	/* Allocate memory for YUV lookup tables */
	s->yiq_level_lookup = malloc(0x1000000 * sizeof(_yiq16_t));
	if(s->yiq_level_lookup == NULL)
	{
		vid_free(s);
		return(VID_OUT_OF_MEMORY);
	}
	
	/* Generate the gamma lookup table. LUTception */
	if(s->conf.gamma <= 0)
	{
		s->conf.gamma = 1.0;
	}
	
	for(c = 0; c < 0x100; c++)
	{
		glut[c] = pow((double) c / 255, 1 / s->conf.gamma);
	}
	
	/* Generate the RGB > signal level lookup tables */
	for(c = 0x000000; c <= 0xFFFFFF; c++)
	{
		double r, g, b;
		double y, u, v;
		double i, q;
		
		/* Calculate RGB 0..1 values */
		r = glut[(c & 0xFF0000) >> 16];
		g = glut[(c & 0x00FF00) >> 8];
		b = glut[(c & 0x0000FF) >> 0];
		
		/* Calculate Y, Cb and Cr values */
		y = r * s->conf.rw_co
		  + g * s->conf.gw_co
		  + b * s->conf.bw_co;
		u = (b - y);
		v = (r - y);
		
		i = s->conf.iv_co * v + s->conf.iu_co * u;
		q = s->conf.qv_co * v + s->conf.qu_co * u;
		
		/* Adjust values to correct signal level */
		y  = s->conf.black_level + (y * (s->conf.white_level - s->conf.black_level));
		i *= s->conf.white_level - s->conf.black_level;
		q *= s->conf.white_level - s->conf.black_level;
		
		/* Convert to INT16 range and store in tables */
		s->yiq_level_lookup[c].y = round(_dlimit(y * level, -1, 1) * INT16_MAX);
		s->yiq_level_lookup[c].i = round(_dlimit(i * level, -1, 1) * INT16_MAX);
		s->yiq_level_lookup[c].q = round(_dlimit(q * level, -1, 1) * INT16_MAX);
	}
	
	if(s->conf.colour_lookup_lines > 0)
	{
		/* Generate the colour subcarrier lookup table */
		/* This carrier is in phase with the U (B-Y) component */
		s->colour_lookup_width = s->width * s->conf.colour_lookup_lines;
		d = 2.0 * M_PI * s->conf.colour_carrier / s->sample_rate;
		
		s->colour_lookup = malloc((s->colour_lookup_width + s->width) * sizeof(int16_t));
		if(!s->colour_lookup)
		{
			vid_free(s);
			return(VID_OUT_OF_MEMORY);
		}
		
		for(c = 0; c < s->colour_lookup_width; c++)
		{
			s->colour_lookup[c] = round(-sin(d * c) * INT16_MAX);
		}
		
		/* To make overflow easier to handle, we repeat the first line at the end */
		memcpy(&s->colour_lookup[s->colour_lookup_width], s->colour_lookup, s->width * sizeof(int16_t));
	}
	
	if(s->conf.burst_level > 0)
	{
		/* Generate the colour burst envelope */
		s->burst_left  = round(s->sample_rate * (s->conf.burst_left - s->conf.burst_rise / 2));
		s->burst_win   = _burstwin(
			s->sample_rate,
			s->conf.burst_width,
			s->conf.burst_rise,
			s->conf.burst_level * (s->conf.white_level - s->conf.blanking_level) / 2 * level,
			&s->burst_width
		);
		if(!s->burst_win)
		{
			vid_free(s);
			return(VID_OUT_OF_MEMORY);
		}
	}
	
	s->fsc_flag_left  = round(s->sample_rate * s->conf.fsc_flag_left);
	s->fsc_flag_width = round(s->sample_rate * s->conf.fsc_flag_width);
	s->fsc_flag_level = round(s->conf.fsc_flag_level * (s->conf.white_level - s->conf.blanking_level) * level * INT16_MAX);
	
	if(s->conf.colour_mode == VID_SECAM)
	{
		double secam_level = s->conf.burst_level * (s->conf.white_level - s->conf.blanking_level) * level;
		
		r = _init_fm_modulator(&s->fm_secam_cr, s->sample_rate, 4250000, 230000, secam_level);
		if(r != VID_OK)
		{
			vid_free(s);
			return(r);
		}
		
		r = _init_fm_modulator(&s->fm_secam_cb, s->sample_rate, 4406260, 280000, secam_level);
		if(r != VID_OK)
		{
			vid_free(s);
			return(r);
		}
	}
	
	/* Set the next line/frame counter */
	/* NOTE: TV line and frame numbers start at 1 rather than 0 */
	s->bline  = 1;
	s->bframe = 1;
	
	s->framebuffer = NULL;
	s->olines = 1;
	s->audio = 0;
	
	/* Initalise D/D2-MAC state */
	if(s->conf.type == VID_MAC)
	{
		r = mac_init(s);
		
		if(r != VID_OK)
		{
			return(r);
		}
		
		_add_lineprocess(s, "macraster", 3, NULL, mac_next_line, NULL);
	}
	else
	{
		_add_lineprocess(s, "raster", 1, NULL, _vid_next_line_raster, NULL);
	}
	
	/* Initalise VITS inserter */
	if(s->conf.vits)
	{
		if((r = vits_init(&s->vits, s->sample_rate, s->width, s->conf.lines, s->white_level - s->blanking_level)) != VID_OK)
		{
			vid_free(s);
			return(r);
		}
		
		_add_lineprocess(s, "vits", 1, &s->vits, vits_render, NULL);
	}
	
	/* Initalise the WSS system */
	if(s->conf.wss)
	{
		if((r = wss_init(&s->wss, s, s->conf.wss)) != VID_OK)
		{
			vid_free(s);
			return(r);
		}
		
		_add_lineprocess(s, "wss", 1, &s->wss, wss_render, NULL);
	}
	
	/* Initialise videocrypt I/II encoder */
	if(s->conf.videocrypt || s->conf.videocrypt2)
	{
		if((r = vc_init(&s->vc, s, s->conf.videocrypt, s->conf.videocrypt2)) != VID_OK)
		{
			vid_free(s);
			return(r);
		}
		
		_add_lineprocess(s, "videocrypt", 2, &s->vc, vc_render_line, NULL);
	}
	
	/* Initialise videocrypt S encoder */
	if(s->conf.videocrypts)
	{
		if((r = vcs_init(&s->vcs, s, s->conf.videocrypts)) != VID_OK)
		{
			vid_free(s);
			return(r);
		}
		
		_add_lineprocess(s, "videocrypts", VCS_DELAY_LINES, &s->vcs, vcs_render_line, NULL);
	}
	
	/* Initalise syster encoder */
	if(s->conf.syster || s->conf.systercnr)
	{
		if((r = ng_init(&s->ng, s)) != VID_OK)
		{
			vid_free(s);
			return(r);
		}
		
		_add_lineprocess(s, "syster", NG_DELAY_LINES, &s->ng, ng_render_line, NULL);
	}

	/* Initalise D11 encoder */
	if(s->conf.d11)
	{
		if((r = d11_init(&s->ng, s, s->conf.d11)) != VID_OK)
		{
			vid_free(s);
			return(r);
		}
		
		_add_lineprocess(s, "discret11", 2, &s->ng, d11_render_line, NULL);
	}
	
	/* Initalise ACP renderer */
	if(s->conf.acp)
	{
		if((r = acp_init(&s->acp, s)) != VID_OK)
		{
			vid_free(s);
			return(r);
		}
		
		_add_lineprocess(s, "acp", 1, &s->acp, acp_render_line, NULL);
	}
	
	/* Initalise the teletext system */
	if(s->conf.teletext || s->conf.txsubtitles)
	{
		if((r = tt_init(&s->tt, s, s->conf.teletext ? s->conf.teletext : "subtitles")) != VID_OK)
		{
			vid_free(s);
			return(r);
		}
		
		/* Start the teletext renderer thread for non-MAC modes */
		if(s->conf.type != VID_MAC)
		{
			_add_lineprocess(s, "teletext", 1, &s->tt, tt_render_line, NULL);
		}
	}
	
	if(s->conf.vfilter)
	{
		_init_vfilter(s);
	}
	
	/* FM audio */
	if(s->conf.fm_mono_level > 0 && s->conf.fm_mono_carrier != 0)
	{
		r = _init_fm_modulator(&s->fm_mono, s->sample_rate, s->conf.fm_mono_carrier, s->conf.fm_mono_deviation, s->conf.fm_mono_level * slevel);
		if(r != VID_OK)
		{
			vid_free(s);
			return(r);
		}
		
		if(s->conf.fm_mono_preemph && s->conf.fmaudiotest)
		{
			const int32_t *taps = NULL;
			int ntaps = 0;
			
			switch(s->conf.fm_mono_preemph)
			{
			case VID_50US:
				taps = fm_audio_50us_taps;
				ntaps = sizeof(fm_audio_50us_taps) / sizeof(int32_t);
				break;
			case VID_75US:
				taps = fm_audio_75us_taps;
				ntaps = sizeof(fm_audio_75us_taps) / sizeof(int32_t);
				break;
			}
			
			r = limiter_init(&s->fm_mono.limiter, INT16_MAX, 21, taps, fm_audio_flat_taps, ntaps);
			if(r != 0)
			{
				vid_free(s);
				return(VID_OUT_OF_MEMORY);
			}
		}
		
		s->audio = 1;
	}
	
	if(s->conf.fm_left_level > 0 && s->conf.fm_left_carrier != 0)
	{
		r = _init_fm_modulator(&s->fm_left, s->sample_rate, s->conf.fm_left_carrier, s->conf.fm_left_deviation, s->conf.fm_left_level * slevel);
		if(r != VID_OK)
		{
			vid_free(s);
			return(r);
		}
		
		if(s->conf.fm_left_preemph && s->conf.fmaudiotest)
		{
			const int32_t *taps = NULL;
			int ntaps = 0;
			
			switch(s->conf.fm_left_preemph)
			{
			case VID_50US:
				taps = fm_audio_50us_taps;
				ntaps = sizeof(fm_audio_50us_taps) / sizeof(int32_t);
				break;
			case VID_75US:
				taps = fm_audio_75us_taps;
				ntaps = sizeof(fm_audio_75us_taps) / sizeof(int32_t);
				break;
			}
			
			r = limiter_init(&s->fm_left.limiter, INT16_MAX, 21, taps, fm_audio_flat_taps, ntaps);
			if(r != 0)
			{
				vid_free(s);
				return(VID_OUT_OF_MEMORY);
			}
		}
		
		s->audio = 1;
	}
	
	if(s->conf.fm_right_level > 0 && s->conf.fm_right_carrier != 0)
	{
		r = _init_fm_modulator(&s->fm_right, s->sample_rate, s->conf.fm_right_carrier, s->conf.fm_right_deviation, s->conf.fm_right_level * slevel);
		if(r != VID_OK)
		{
			vid_free(s);
			return(r);
		}
		
		if(s->conf.fm_right_preemph && s->conf.fmaudiotest)
		{
			const int32_t *taps = NULL;
			int ntaps = 0;
			
			switch(s->conf.fm_right_preemph)
			{
			case VID_50US:
				taps = fm_audio_50us_taps;
				ntaps = sizeof(fm_audio_50us_taps) / sizeof(int32_t);
				break;
			case VID_75US:
				taps = fm_audio_75us_taps;
				ntaps = sizeof(fm_audio_75us_taps) / sizeof(int32_t);
				break;
			}
			
			r = limiter_init(&s->fm_right.limiter, INT16_MAX, 21, taps, fm_audio_flat_taps, ntaps);
			if(r != 0)
			{
				vid_free(s);
				return(VID_OUT_OF_MEMORY);
			}
		}
		
		s->audio = 1;
	}
	
	/* NICAM audio */
	if(s->conf.nicam_level > 0 && s->conf.nicam_carrier != 0)
	{
		r = nicam_mod_init(&s->nicam, NICAM_MODE_STEREO, 0, s->sample_rate, s->conf.nicam_carrier, s->conf.nicam_beta, s->conf.nicam_level * slevel);
		
		if(r != 0)
		{
			vid_free(s);
			return(VID_OUT_OF_MEMORY);
		}
		
		s->nicam_buf_len = 0;
		s->audio = 1;
	}
	
	/* DANCE audio */
	if(s->conf.dance_level > 0 && s->conf.dance_carrier != 0)
	{
		r = dance_mod_init(&s->dance, DANCE_MODE_A, s->sample_rate, s->conf.dance_carrier, s->conf.dance_beta, s->conf.dance_level * slevel);
		
		if(r != 0)
		{
			vid_free(s);
			return(VID_OUT_OF_MEMORY);
		}
		
		s->dance_buf_len = 0;
		s->audio = 1;
	}
	
	/* AM audio */
	if(s->conf.am_audio_level > 0 && s->conf.am_mono_carrier != 0)
	{
		r = _init_am_modulator(&s->am_mono, s->sample_rate, s->conf.am_mono_carrier, s->conf.am_audio_level * slevel);
		if(r != VID_OK)
		{
			vid_free(s);
			return(r);
		}
		
		s->audio = 1;
	}
	
	/* Add the audio process */
	if(s->audio == 1)
	{
		_add_lineprocess(s, "audio", 1, NULL, _vid_audio_process, NULL);
	}
	
	/* FM video */
	if(s->conf.modulation == VID_FM)
	{
		r = _init_fm_modulator(&s->fm_video, s->sample_rate, 0, s->conf.fm_deviation, s->conf.fm_level * s->conf.level);
		if(r != VID_OK)
		{
			vid_free(s);
			return(r);
		}
		
		_add_lineprocess(s, "fmmod", 1, NULL, _vid_fmmod_process, NULL);
	}
	
	if(s->conf.offset != 0)
	{
		double d;
		
		s->offset.counter = INT16_MAX;
		s->offset.phase.i = INT16_MAX;
		s->offset.phase.q = 0;
		
		d = 2.0 * M_PI / s->sample_rate * s->conf.offset;
		s->offset.delta.i = lround(cos(d) * INT32_MAX);
		s->offset.delta.q = lround(sin(d) * INT32_MAX);
		
		_add_lineprocess(s, "offset", 1, NULL, _vid_offset_process, NULL);
	}
	
	if(s->conf.passthru)
	{
		/* Open the passthru source */
		if(strcmp(s->conf.passthru, "-") == 0)
		{
			s->passthru = stdin;
		}
		else
		{
			s->passthru = fopen(s->conf.passthru, "rb");
		}
		
		if(!s->passthru)
		{
			perror(s->conf.passthru);
			vid_free(s);
			return(VID_ERROR);
		}
		
		/* Allocate memory for the temporary passthru buffer */
		s->passline = calloc(sizeof(int16_t) * 2, s->width);
		if(!s->passline)
		{
			vid_free(s);
			return(VID_OUT_OF_MEMORY);
		}
		
		_add_lineprocess(s, "passthru", 1, NULL, _vid_passthru_process, NULL);
	}
	
	/* The final process is only for output */
	_add_lineprocess(s, "output", 1, NULL, NULL, NULL);
	s->output_process = &s->processes[s->nprocesses - 1];
	
	/* Output line buffer(s) */
	s->oline = calloc(sizeof(vid_line_t), s->olines);
	if(!s->oline)
	{
		vid_free(s);
		return(VID_OUT_OF_MEMORY);
	}
	
	for(r = 0; r < s->olines; r++)
	{
		s->oline[r].output = malloc(sizeof(int16_t) * 2 * s->width);
		if(!s->oline[r].output)
		{
			vid_free(s);
			return(VID_OUT_OF_MEMORY);
		}
		
		/* Blank the lines */
		for(x = 0; x < s->width; x++)
		{
			s->oline[r].output[x * 2] = s->blanking_level;
		}
		
		s->oline[r].frame = 1;
		s->oline[r].line = 0;
		s->oline[r].vbialloc = 0;
		s->oline[r].next = &s->oline[(r + 1) % s->olines];
	}
	
	/* Setup lineprocess output windows (non-threaded version) */
	l = &s->oline[s->olines - 1];
	
	for(r = 0; r < s->nprocesses; r++)
	{
		_lineprocess_t *p = &s->processes[r];
		
		l -= p->nlines - 1;
		
		for(x = 0; x < p->nlines; x++)
		{
			p->lines[x] = &l[x];
		}
	}
	
	return(VID_OK);
}

void vid_free(vid_t *s)
{
	int i;
	
	/* Close the AV source */
	vid_av_close(s);
	
	for(i = 0; i < s->nprocesses; i++)
	{
		if(s->processes[i].free)
		{
			s->processes[i].free(s, s->processes[i].arg);
		}
		
		free(s->processes[i].lines);
	}
	free(s->processes);
	
	if(s->conf.passthru)
	{
		fclose(s->passthru);
		free(s->passline);
	}
	
	if(s->conf.teletext)
	{
		tt_free(&s->tt);
	}
	
	if(s->conf.vits)
	{
		vits_free(&s->vits);
	}
	
	if(s->conf.acp)
	{
		acp_free(&s->acp);
	}
	
	if(s->conf.syster || s->conf.d11 || s->conf.systercnr)
	{
		ng_free(&s->ng);
	}
	
	if(s->conf.videocrypt || s->conf.videocrypt2)
	{
		vc_free(&s->vc);
	}
	
	if(s->conf.videocrypts)
	{
		vcs_free(&s->vcs);
	}
	
	if(s->conf.wss)
	{
		wss_free(&s->wss);
	}
	
	if(s->conf.type == VID_MAC)
	{
		mac_free(s);
	}
	
	/* Free allocated memory */
	free(s->yiq_level_lookup);
	free(s->colour_lookup);
	_free_fm_modulator(&s->fm_secam_cr);
	_free_fm_modulator(&s->fm_secam_cb);
	_free_fm_modulator(&s->fm_video);
	_free_fm_modulator(&s->fm_mono);
	_free_fm_modulator(&s->fm_left);
	_free_fm_modulator(&s->fm_right);
	limiter_free(&s->fm_mono.limiter);
	limiter_free(&s->fm_left.limiter);
	limiter_free(&s->fm_right.limiter);
	dance_mod_free(&s->dance);
	nicam_mod_free(&s->nicam);
	_free_am_modulator(&s->am_mono);
	
	if(s->oline)
	{
		for(i = 0; i < s->olines; i++)
		{
			free(s->oline[i].output);
		}
		free(s->oline);
	}
	
	free(s->burst_win);
	
	memset(s, 0, sizeof(vid_t));
}

void vid_info(vid_t *s)
{
	fprintf(stderr, "Video: %dx%d %.2f fps (full frame %dx%d)\n",
		s->active_width, s->conf.active_lines,
		(double) s->conf.frame_rate_num / s->conf.frame_rate_den,
		s->width, s->conf.lines
	);
	
	fprintf(stderr, "Sample rate: %d\n", s->sample_rate);
}

size_t vid_get_framebuffer_length(vid_t *s)
{
	return(sizeof(uint32_t) * s->active_width * s->conf.active_lines);
}

static vid_line_t *_vid_next_line(vid_t *s, size_t *samples)
{
	vid_line_t *l = s->output_process->lines[0];
	int i, j;
	
	/* Load the next frame */
	if(s->bline == 1 || (s->conf.interlace && s->bline == s->conf.hline))
	{
		/* Have we reached the end of the video? */
		if(_av_eof(s))
		{
			return(NULL);
		}
		s->framebuffer = _av_read_video(s, &s->ratio);
	}
	
	for(i = 0; i < s->nprocesses; i++)
	{
		_lineprocess_t *p = &s->processes[i];
		
		if(p->process)
		{
			p->process(p->vid, p->arg, p->nlines, p->lines);
		}
		
		for(j = 0; j < p->nlines; j++)
		{
			p->lines[j] = p->lines[j]->next;
		}
	}
	
	/* Advance the next line/frame counter */
	if(s->bline++ == s->conf.lines)
	{
		s->bline = 1;
		s->bframe++;
	}
	
	/* Return a pointer to the output buffer */
	if(samples)
	{
		*samples = s->width;
	}
	
	return(l);
}

int16_t *vid_next_line(vid_t *s, size_t *samples)
{
	vid_line_t *l;
	
	/* Drop any delay lines introduced by scramblers / filters */
	do
	{
		l = _vid_next_line(s, samples);
		if(l == NULL) return(NULL);
	}
	while(l->line < 1);
	
	s->frame = l->frame;
	s->line  = l->line;
	
	return(l->output);
}

