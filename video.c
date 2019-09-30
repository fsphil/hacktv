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
#include "hacktv.h"

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
	.fm_audio_level = 0.22, /* FM audio carrier power level */
	.nicam_level    = 0.07 / 2, /* NICAM audio carrier power level */
	
	.type           = VID_RASTER_625,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 625,
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
	.burst_left     = 0.00000560, /* |-->| 5.6 ±0.1µs */
	.burst_level    = 3.0 / 7.0, /* 3 / 7 of white - blanking level */
	.colour_carrier = 4433618.75,
	.colour_lookup_lines = 625 * 4, /* The carrier repeats after 4 frames */
	
	.gamma          = 1.2, /* 2.8 in spec? too bright */
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	.iu_co          = 0.000,
	.iv_co          = 0.877,
	.qu_co          = 0.493,
	.qv_co          = 0.000,
	
	.fm_mono_carrier    = 6000000 - 400, /* Hz */
	.fm_audio_preemph   = 0.000050, /* Seconds */
	.fm_audio_deviation = 50000, /* +/- Hz */
	
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
	.fm_audio_level = 0.22, /* FM audio carrier power level */
	.nicam_level    = 0.07 / 2, /* NICAM audio carrier power level */
	
	.type           = VID_RASTER_625,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 625,
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
	.burst_left     = 0.00000560, /* |-->| 5.6 ±0.1µs */
	.burst_level    = 3.0 / 7.0, /* 3 / 7 of white - blanking level */
	.colour_carrier = 4433618.75,
	.colour_lookup_lines = 625 * 4, /* The carrier repeats after 4 frames */
	
	.gamma          = 1.2, /* 2.8 in spec? too bright */
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	.iu_co          = 0.000,
	.iv_co          = 0.877,
	.qu_co          = 0.493,
	.qv_co          = 0.000,
	
	.fm_mono_carrier    = 5500000, /* Hz */
	.fm_audio_preemph   = 0.000050, /* Seconds */
	.fm_audio_deviation = 50000, /* +/- Hz */
	
	.nicam_carrier  = 5850000, /* Hz */
	.nicam_beta     = 0.4,
};

const vid_config_t vid_config_pal_fm = {
	
	/* PAL FM (satellite) */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.modulation     = VID_FM,
	.fm_level       = 1.0,
	.fm_deviation   = 6000000, /* kHz */
	
	.level          = 0.8, /* Overall signal level */
	
	.video_level    = 1.00, /* Power level of video */
	.fm_audio_level = 0.05, /* FM audio carrier power level */
	
	.type           = VID_RASTER_625,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 625,
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
	.burst_left     = 0.00000560, /* |-->| 5.6 ±0.1µs */
	.burst_level    = 3.0 / 7.0, /* 3 / 7 of white - blanking level */
	.colour_carrier = 4433618.75,
	.colour_lookup_lines = 625 * 4, /* The carrier repeats after 4 frames */
	
	.gamma          = 1.4,
	
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	.iu_co          = 0.000,
	.iv_co          = 0.877,
	.qu_co          = 0.493,
	.qv_co          = 0.000,
	
	.fm_mono_carrier    = 6500000, /* Hz */
	//.fm_left_carrier    = 7200000, /* Hz */
	//.fm_right_carrier   = 7020000, /* Hz */
	.fm_audio_preemph   = 0.000050, /* Seconds */
	.fm_audio_deviation = 85000, /* +/- Hz */
};

const vid_config_t vid_config_pal = {
	
	/* Composite PAL */
	.output_type    = HACKTV_INT16_REAL,
	
	.level          = 1.0, /* Overall signal level */
	.video_level    = 1.0, /* Power level of video */
	
	.type           = VID_RASTER_625,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 625,
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
	.burst_left     = 0.00000560, /* |-->| 5.6 ±0.1µs */
	.burst_level    = 3.0 / 7.0, /* 3 / 7 of white - blanking level */
	.colour_carrier = 4433618.75,
	.colour_lookup_lines = 625 * 4, /* The carrier repeats after 4 frames */
	
	.gamma          = 1.4, /* 2.8 in spec? too bright */
	
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
	
	/* TODO: Find out what the usual video/audio power ratio is */
	.video_level    = 0.80, /* Power level of video */
	.am_audio_level = 0.20, /* FM audio carrier power level */
	
	.type           = VID_RASTER_625,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 625,
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
	.burst_left     = 0.00000560, /* |-->| 5.6 ±0.1µs */
	
	.gamma          = 1.2,
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	.iu_co          = 1.000,
	.iv_co          = 0.000,
	.qu_co          = 0.000,
	.qv_co          = -1.000,
	
	.am_mono_carrier = 6500000, /* Hz */
};

const vid_config_t vid_config_secam = {
	
	/* Composite SECAM */
	.output_type    = HACKTV_INT16_REAL,
	
	.level          = 1.0, /* Overall signal level */
	.video_level    = 1.0, /* Power level of video */
	
	.type           = VID_RASTER_625,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 625,
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
	.burst_left     = 0.00000560, /* |-->| 5.6 ±0.1µs */
	
	.gamma          = 1.2,
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	.iu_co          = 1.000,
	.iv_co          = 0.000,
	.qu_co          = 0.000,
	.qv_co          = -1.000,
};

const vid_config_t vid_config_ntsc_m = {
	
	/* System M (NTSC) */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.modulation     = VID_VSB,
	.vsb_upper_bw   = 4200000, /* Hz */
	.vsb_lower_bw   =  750000, /* Hz */
	
	.level          = 1.0, /* Overall signal level */
	
	.video_level    = 0.83, /* Power level of video */
	.fm_audio_level = 0.17, /* FM audio carrier power level */
	
	.type           = VID_RASTER_525,
	.frame_rate_num = 30000,
	.frame_rate_den = 1001,
	.lines          = 525,
	.active_lines   = 480,
	.active_width   = 0.00005290, /* 52.90µs */
	.active_left    = 0.00000920, /* |-->| 9.20µs */
	
	.hsync_width       = 0.00000470, /*  4.70 ±1.00µs */
	.vsync_short_width = 0.00000230, /*  2.30 ±0.10µs */
	.vsync_long_width  = 0.00002710, /* 27.10 µs */
	
	.white_level    = 0.2000,
	.black_level    = 0.7280,
	.blanking_level = 0.7712,
	.sync_level     = 1.0000,
	
	.colour_mode    = VID_NTSC,
	.burst_width    = 0.00000250, /* 2.5 ±0.28µs */
	.burst_left     = 0.00000530, /* |-->| 5.3 ±0.1µs */
	.burst_level    = 4.0 / 10.0, /* 4/10 of white - blanking level */
	.colour_carrier = 5000000.0 * 63 / 88,
	.colour_lookup_lines = 2, /* The carrier repeats after 2 lines */
	
	.gamma          = 1.2,
	
	.rw_co          =  0.299, /* R weight */
	.gw_co          =  0.587, /* G weight */
	.bw_co          =  0.114, /* B weight */
	.iu_co          = -0.177,
	.iv_co          =  0.768,
	.qu_co          =  0.626,
	.qv_co          = -0.082,
	
	.fm_mono_carrier    = 4500000, /* Hz */
	.fm_audio_preemph   = 0.000075, /* Seconds */
	.fm_audio_deviation = 25000, /* +/- Hz */
};

const vid_config_t vid_config_ntsc = {
	
	/* Composite NTSC */
	.output_type    = HACKTV_INT16_REAL,
	
	.level          = 1.0, /* Overall signal level */
	.video_level    = 1.0, /* Power level of video */
	
	.type           = VID_RASTER_525,
	.frame_rate_num = 30000,
	.frame_rate_den = 1001,
	.lines          = 525,
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
	
	.colour_mode    = VID_NTSC,
	.burst_width    = 0.00000250, /* 2.5 ±0.28µs */
	.burst_left     = 0.00000530, /* |-->| 5.3 ±0.1µs */
	.burst_level    = 4.0 / 10.0, /* 4/10 of white - blanking level */
	.colour_carrier = 5000000.0 * 63 / 88,
	.colour_lookup_lines = 2, /* The carrier repeats after 2 lines */
	
	.gamma          =  1.2,
	.rw_co          =  0.299, /* R weight */
	.gw_co          =  0.587, /* G weight */
	.bw_co          =  0.114, /* B weight */
	.iu_co          = -0.177,
	.iv_co          =  0.768,
	.qu_co          =  0.626,
	.qv_co          = -0.082,
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
	.active_lines   = 720, /* Normally 738 */
	.active_width   = 0.00003944, /* 39.44µs */
	.active_left    = 0.00000890, /* |-->| 8.9µs */
	
	.hsync_width      = 0.00000250, /* 2.50 ±0.10µs */
	.vsync_long_width = 0.00002000, /* 20.0 ±1.00µs */
	
	.white_level    = 1.00,
	.black_level    = 0.35,
	.blanking_level = 0.30,
	.sync_level     = 0.00,
	
	.gamma          = 1.2,
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	
	/* AM modulated */
	.am_mono_carrier   = 11500000, /* Hz */
	.am_mono_bandwidth =    10000, /* Hz */
};

const vid_config_t vid_config_819 = {
	
	/* 819 line video, French variant */
	.output_type    = HACKTV_INT16_REAL,
	
	.level          = 1.0, /* Overall signal level */
	.video_level    = 1.0, /* Power level of video */
	
	.type           = VID_RASTER_819,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 819,
	.active_lines   = 720, /* Normally 738 */
	.active_width   = 0.00003944, /* 39.44µs */
	.active_left    = 0.00000890, /* |-->| 8.9µs */
	
	.hsync_width      = 0.00000250, /* 2.50 ±0.10µs */
	.vsync_long_width = 0.00002000, /* 20.0 ±1.00µs */
	
	.white_level    =  0.70,
	.black_level    =  0.05,
	.blanking_level =  0.00,
	.sync_level     = -0.30,
	
	.gamma          = 1.2,
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
	.active_lines   = 376,
	.active_width   = 0.00008030, /* 80.3µs */
	.active_left    = 0.00001680, /* |-->| 16.8µs */
	
	.hsync_width       = 0.00000900, /* 9.00 ±1.00µs */
	.vsync_long_width  = 0.00004000, /* 40.0 ±2.00µs */
	
	.white_level    = 1.00,
	.black_level    = 0.30,
	.blanking_level = 0.30,
	.sync_level     = 0.00,
	
	.gamma          = 1.2,
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
	
	.type           = VID_RASTER_405,
	.frame_rate_num = 25,
	.frame_rate_den = 1,
	.lines          = 405,
	.active_lines   = 376,
	.active_width   = 0.00008030, /* 80.3µs */
	.active_left    = 0.00001680, /* |-->| 16.8µs */
	
	.hsync_width       = 0.00000900, /* 9.00 ±1.00µs */
	.vsync_long_width  = 0.00004000, /* 40.0 ±2.00µs */
	
	.white_level    =  0.70,
	.black_level    =  0.00,
	.blanking_level =  0.00,
	.sync_level     = -0.30,
	
	.gamma          = 1.2,
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
	
	.gamma          = 1.2,
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
	
	.gamma          = 1.2,
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
	
	.gamma          = 1.2,
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
	
	.gamma          = 1.2,
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
};

const vid_config_t vid_config_apollo_colour_fm = {
	
	/* Unified S-Band, Apollo Colour Lunar Television */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.level          = 1.000, /* Overall signal level */
	.video_level    = 1.000, /* Power level of video */
	.fm_audio_level = 0.150, /* Power level of audio */
	
	.modulation     = VID_FM,
	.fm_level       = 1.0,
	.fm_deviation   = 1000000, /* Hz */
	
	.type           = VID_RASTER_525,
	.frame_rate_num = 30000,
	.frame_rate_den = 1001,
	.lines          = 525,
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
	
	.gamma          =  1.0,
	.rw_co          =  0.299, /* R weight */
	.gw_co          =  0.587, /* G weight */
	.bw_co          =  0.114, /* B weight */
	
	/* The audio carrier overlaps the video signal and
	 * requires the video to either be low pass filtered
	 * to 750kHz (Apollo 10 to 14) or cancelled out
	 * in post-processing (Apollo 15-17). */
	
	.fm_mono_carrier    = 1250000, /* Hz */
	.fm_audio_deviation = 25000, /* +/- Hz */
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
	
	.gamma          =  1.0,
	.rw_co          =  0.299, /* R weight */
	.gw_co          =  0.587, /* G weight */
	.bw_co          =  0.114, /* B weight */
};

const vid_config_t vid_config_apollo_mono_fm = {
	
	/* Unified S-Band, Apollo Lunar Television 10 fps video (Mode 1) */
	.output_type    = HACKTV_INT16_COMPLEX,
	
	.level          = 1.000, /* Overall signal level */
	.video_level    = 1.000, /* Power level of video */
	.fm_audio_level = 0.150, /* Power level of audio */
	
	.modulation     = VID_FM,
	.fm_level       = 1.0,
	.fm_deviation   = 1000000, /* Hz */
	
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
	.gamma          = 1.0,
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
	
	.fm_mono_carrier    = 1250000, /* Hz */
	.fm_audio_deviation = 25000, /* +/- Hz */
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
	.gamma          = 1.0,
	.rw_co          = 0.299, /* R weight */
	.gw_co          = 0.587, /* G weight */
	.bw_co          = 0.114, /* B weight */
};

const vid_configs_t vid_configs[] = {
	{ "i",             &vid_config_pal_i            },
	{ "b",             &vid_config_pal_bg           },
	{ "g",             &vid_config_pal_bg           },
	{ "pal-fm",        &vid_config_pal_fm           },
	{ "pal",           &vid_config_pal              },
	{ "l",             &vid_config_secam_l          },
	{ "secam",         &vid_config_secam            },
	{ "m",             &vid_config_ntsc_m           },
	{ "ntsc",          &vid_config_ntsc             },
	{ "e",             &vid_config_819_e            },
	{ "819",           &vid_config_819              },
	{ "a",             &vid_config_405_a            },
	{ "405",           &vid_config_405              },
	{ "240-am",        &vid_config_baird_240_am     },
	{ "240",           &vid_config_baird_240        },
	{ "30-am",         &vid_config_baird_30_am      },
	{ "30",            &vid_config_baird_30         },
	{ "apollo-fsc-fm", &vid_config_apollo_colour_fm },
	{ "apollo-fsc",    &vid_config_apollo_colour    },
	{ "apollo-fm",     &vid_config_apollo_mono_fm   },
	{ "apollo",        &vid_config_apollo_mono      },
	{ NULL,            NULL },
};

static int16_t *_colour_subcarrier_phase(vid_t *s, int phase)
{
	int frame = (s->frame - 1) & 3;
	int line = s->line - 1;
	int p;
	
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

/* FM modulator */
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

int vid_init(vid_t *s, unsigned int sample_rate, const vid_config_t * const conf)
{
	int r, x;
	int64_t c;
	double d;
	double glut[0x100];
	double level, slevel;
	
	memset(s, 0, sizeof(vid_t));
	memcpy(&s->conf, conf, sizeof(vid_config_t));
		
	/* Calculate the number of samples per line */
	s->width = round((double) sample_rate / ((double) s->conf.frame_rate_num / s->conf.frame_rate_den) / s->conf.lines);
	s->half_width = round((double) sample_rate / ((double) s->conf.frame_rate_num / s->conf.frame_rate_den) / s->conf.lines / 2);
	
	/* Calculate the "actual" sample rate we use. This is calculated
	 * to give us an exact number of samples per line */
	//s->sample_rate = s->width * s->conf.lines * s->conf.frame_rate;
	// This won't work with NTSC
	
	//if(s->sample_rate != sample_rate)
	//{
	//	fprintf(stderr, "Sample rate error %0.2f%%\n", (double) s->sample_rate / sample_rate * 100);
	//}
	
	s->sample_rate = sample_rate;
	
	/* Calculate the active video width and offset */
	s->active_width = ceil(s->sample_rate * s->conf.active_width);
	if(s->active_width > s->width) s->active_width = s->width;
	
	s->active_left  = round(s->sample_rate * s->conf.active_left);
	
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
	s->y_level_lookup = malloc(0x1000000 * sizeof(int16_t));
	s->i_level_lookup = malloc(0x1000000 * sizeof(int16_t));
	s->q_level_lookup = malloc(0x1000000 * sizeof(int16_t));
	
	if(s->y_level_lookup == NULL ||
	   s->i_level_lookup == NULL ||
	   s->q_level_lookup == NULL)
	{
		vid_free(s);
		return(VID_OUT_OF_MEMORY);
	}
	
	/* Generate the gamma lookup table. LUTception */
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
		s->y_level_lookup[c] = round(y * level * INT16_MAX);
		s->i_level_lookup[c] = round(i * level * INT16_MAX);
		s->q_level_lookup[c] = round(q * level * INT16_MAX);
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
	
	s->burst_left  = round(s->sample_rate * s->conf.burst_left);
	s->burst_width = round(s->sample_rate * s->conf.burst_width);
	s->burst_level = round(s->conf.burst_level * (s->conf.white_level - s->conf.blanking_level) / 2 * level * INT16_MAX);
	
	s->fsc_flag_left  = round(s->sample_rate * s->conf.fsc_flag_left);
	s->fsc_flag_width = round(s->sample_rate * s->conf.fsc_flag_width);
	s->fsc_flag_level = round(s->conf.fsc_flag_level * (s->conf.white_level - s->conf.blanking_level) * level * INT16_MAX);
	
	if(s->conf.colour_mode == VID_SECAM)
	{
		int secam_level = round((s->conf.white_level - s->conf.blanking_level) * 0.200 * s->conf.video_level * level * INT16_MAX);
		
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
	s->bline  = s->line = 1;
	s->bframe = s->frame = 1;
	
	s->framebuffer = NULL;
	
	s->olines = 1;
	
	/* Audio */
	s->audio = 0;
	
	/* FM audio */
	if(s->conf.fm_audio_level > 0 && s->conf.fm_mono_carrier != 0)
	{
		r = _init_fm_modulator(&s->fm_mono, s->sample_rate, s->conf.fm_mono_carrier, s->conf.fm_audio_deviation, s->conf.fm_audio_level * slevel);
		if(r != VID_OK)
		{
			vid_free(s);
			return(r);
		}
		
		s->audio = 1;
	}
	
	if(s->conf.fm_audio_level > 0 && s->conf.fm_left_carrier != 0)
	{
		r = _init_fm_modulator(&s->fm_left, s->sample_rate, s->conf.fm_left_carrier, s->conf.fm_audio_deviation, s->conf.fm_audio_level * slevel);
		if(r != VID_OK)
		{
			vid_free(s);
			return(r);
		}
		
		s->audio = 1;
	}
	
	if(s->conf.fm_audio_level > 0 && s->conf.fm_right_carrier != 0)
	{
		r = _init_fm_modulator(&s->fm_right, s->sample_rate, s->conf.fm_right_carrier, s->conf.fm_audio_deviation, s->conf.fm_audio_level * slevel);
		if(r != VID_OK)
		{
			vid_free(s);
			return(r);
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
	
	/* FM video */
	if(s->conf.modulation == VID_FM)
	{
		r = _init_fm_modulator(&s->fm_video, s->sample_rate, 0, s->conf.fm_deviation, s->conf.fm_level * s->conf.level);
		if(r != VID_OK)
		{
			vid_free(s);
			return(r);
		}
	}
	
	/* Initalise the teletext system */
	if(s->conf.teletext && (r = tt_init(&s->tt, s, s->conf.teletext)) != VID_OK)
	{
		vid_free(s);
		return(r);
	}
	
	/* Initalise the WSS system */
	if(s->conf.wss && (r = wss_init(&s->wss, s, s->conf.wss)) != VID_OK)
	{
		vid_free(s);
		return(r);
	}
	
	/* Initialise videocrypt encoder */
	if((s->conf.videocrypt || s->conf.videocrypt2) && 
	(r = vc_init(&s->vc, s, s->conf.videocrypt, s->conf.videocrypt2, s->conf.key)) != VID_OK)
	{
		vid_free(s);
		return(r);
	}
	
	/* Initialise videocrypt S encoder */
	if(s->conf.videocrypts && (r = vcs_init(&s->vcs, s, s->conf.videocrypts)) != VID_OK)
	{
		vid_free(s);
		return(r);
	}
	
	/* Initalise syster encoder */
	if(s->conf.syster && (r = ng_init(&s->ng, s)) != VID_OK)
	{
		vid_free(s);
		return(r);
	}
	
	/* Initalise D11 encoder */
 if(s->conf.d11 && (r = d11_init(&s->ng, s)) != VID_OK)
 {
	 vid_free(s);
	 return(r);
 }
 
	/* Initalise ACP renderer */
	if(s->conf.acp && (r = acp_init(&s->acp, s)) != VID_OK)
	{
		vid_free(s);
		return(r);
	}
	
	/* Output line buffer(s) */
	s->oline = calloc(sizeof(int16_t *), s->olines);
	if(!s->oline)
	{
		vid_free(s);
		return(VID_OUT_OF_MEMORY);
	}
	
	for(r = 0; r < s->olines; r++)
	{
		s->oline[r] = malloc(sizeof(int16_t) * 2 * s->width);
		if(!s->oline[r])
		{
			vid_free(s);
			return(VID_OUT_OF_MEMORY);
		}
		
		/* Blank the lines */
		for(x = 0; x < s->width; x++)
		{
			s->oline[r][x * 2] = s->blanking_level;
		}
	}
	
	return(VID_OK);
}

void vid_free(vid_t *s)
{
	int i;
	
	/* Close the AV source */
	vid_av_close(s);
	
	if(s->conf.acp)
	{
		acp_free(&s->acp);
	}
	
	if(s->conf.syster || s->conf.d11)
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
	
	if(s->conf.teletext)
	{
		tt_free(&s->tt);
	}
	
	if(s->video_filter_taps)
	{
		fir_int16_complex_free(&s->video_filter);
		free(s->video_filter_taps);
	}
	
	/* Free allocated memory */
	if(s->y_level_lookup != NULL) free(s->y_level_lookup);
	if(s->i_level_lookup != NULL) free(s->i_level_lookup);
	if(s->q_level_lookup != NULL) free(s->q_level_lookup);
	if(s->colour_lookup != NULL) free(s->colour_lookup);
	_free_fm_modulator(&s->fm_secam_cr);
	_free_fm_modulator(&s->fm_secam_cb);
	_free_fm_modulator(&s->fm_video);
	_free_fm_modulator(&s->fm_mono);
	_free_fm_modulator(&s->fm_left);
	_free_fm_modulator(&s->fm_right);
	nicam_mod_free(&s->nicam);
	_free_am_modulator(&s->am_mono);
	
	if(s->oline)
	{
		for(i = 0; i < s->olines; i++)
		{
			free(s->oline[i]);
		}
		free(s->oline);
	}
	
	memset(s, 0, sizeof(vid_t));
}

void vid_info(vid_t *s)
{
	fprintf(stderr, "Video: %dx%d %.2f fps (full frame %dx%d)\n",
		s->active_width, s->conf.active_lines, (double) s->conf.frame_rate_num / s->conf.frame_rate_den,
		s->width, s->conf.lines
	);
	
	fprintf(stderr, "Sample rate: %d\n", s->sample_rate);
}

int vid_init_filter(vid_t *s)
{
	int taps = 51; /* Magic number :( */
	
	if(s->conf.modulation == VID_VSB)
	{
		/* Prepare the video filter */
		s->video_filter_taps = calloc(taps, sizeof(int16_t) * 2);
		if(!s->video_filter_taps)
		{
			return(VID_OUT_OF_MEMORY);
		}
		
		fir_int16_complex_band_pass(s->video_filter_taps, taps, s->sample_rate, -s->conf.vsb_lower_bw, s->conf.vsb_upper_bw, 750000, 1);
		fir_int16_complex_init(&s->video_filter, s->video_filter_taps, taps, 1, 1);
	}
	else if(s->conf.modulation == VID_FM)
	{
		/* Test taps for a CCIR-405 video pre-emphasis filter at 16MHz */
		const int16_t fm_taps[47] = {
			-1, -1, -1, -1, -2, -3, -5, -8, -13, -18, -28, -41, -62, -91, -140, -207, -322, -490, -777, -1209, -1962, -3110, -5215,
			/*38620,*/ 32767, -5215, -3110, -1962, -1209, -777, -490, -322, -207, -140, -91, -62, -41, -28, -18, -13, -8, -5, -3, -2, -1, -1, -1, -1
		};
		
		s->video_filter_taps = calloc(47, sizeof(int16_t));
		if(!s->video_filter_taps)
		{
			return(VID_OUT_OF_MEMORY);
		}
		
		memcpy(s->video_filter_taps, fm_taps, 47 * sizeof(int16_t));
		
		fir_int16_init(&s->video_filter, s->video_filter_taps, 47, 1, 1);
	}
	
	return(VID_OK);
}

size_t vid_get_framebuffer_length(vid_t *s)
{
	return(sizeof(uint32_t) * s->active_width * s->conf.active_lines);
}

int16_t *vid_adj_delay(vid_t *s, int lines)
{
	s->odelay -= lines;
	s->output = s->oline[s->odelay];
	
	s->line -= lines;
	while(s->line < 1)
	{
		s->line += s->conf.lines;
		s->frame--;
	}
	
	return(s->output);
}

static int16_t *_vid_next_line(vid_t *s, size_t *samples)
{
	const char *seq;
	int x;
	int vy;
	int w;
	uint32_t rgb;
	int pal;
	int odd;
	int fsc = 0;
	int16_t *lut_b;
	int16_t *lut_i;
	int16_t *lut_q;
	
	s->odelay = s->olines - 1;
	s->output = s->oline[s->odelay];
	
	s->frame = s->bframe;
	s->line = s->bline;
	
	/* Load the next frame */
	if(s->line == 1)
	{
		/* Have we reached the end of the video? */
		if(_av_eof(s))
		{
			return(NULL);
		}
		
		s->framebuffer = _av_read_video(s, &s->ratio);
	}
	
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
		switch(s->line)
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
		
		case 622: seq = "h2aa"; break;
		case 623: seq = "h_av"; break;
		case 624: seq = "v__v"; break;
		case 625: seq = "v__v"; break;
		
		default:  seq = "h0aa"; break;
		}
		
		/* Calculate the active line number */
		vy = (s->line < 313 ? (s->line - 23) * 2 : (s->line - 336) * 2 + 1);
	}
	else if(s->conf.type == VID_RASTER_525)
	{
		switch(s->line)
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
		
		vy = (s->line < 265 ? (s->line - 23) * 2 : (s->line - 286) * 2 + 1);
	}
	else if(s->conf.type == VID_RASTER_819)
	{
		switch(s->line)
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
		vy = (s->line < 406 ? (s->line - 48) * 2 : (s->line - 457) * 2 + 1);
	}
	else if(s->conf.type == VID_RASTER_405)
	{
		switch(s->line)
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
		vy = (s->line < 210 ? (s->line - 16) * 2 : (s->line - 219) * 2 + 1);
	}
	else if(s->conf.type == VID_APOLLO_320)
	{
		if(s->line <= 8) seq = "V__v";
		else seq = "h_aa";
		
		vy = s->line - 9;
		if(vy < 0 || vy >= s->conf.active_lines) vy = -1;
	}
	else if(s->conf.type == VID_BAIRD_240)
	{
		switch(s->line)
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
		vy = s->line - 20;
	}
	else if(s->conf.type == VID_BAIRD_30)
	{
		/* The original Baird 30 line standard has no sync pulses */
		seq = "__aa";
		vy = s->line - 1;
	}
	
	if(vy < 0 || vy >= s->conf.active_lines) vy = -1;
	
	/* Does this line use colour? */
	pal  = seq[1] == '0';
	pal |= seq[1] == '1' && (s->frame & 1) == 1;
	pal |= seq[1] == '2' && (s->frame & 1) == 0;
	
	/* odd == 1 if this is an odd line, otherwise odd == 0 */
	odd = (s->frame + s->line + 1) & 1;
	
	/* Calculate colour sub-carrier lookup-positions for the start of this line */
	if(s->conf.colour_mode == VID_PAL)
	{
		/* PAL */
		lut_b = _colour_subcarrier_phase(s, odd ? -135 : 135);
		lut_i = _colour_subcarrier_phase(s, odd ? -90 : 90);
		lut_q = _colour_subcarrier_phase(s, 0);
	}
	else if(s->conf.colour_mode == VID_NTSC)
	{
		/* NTSC */
		lut_b = _colour_subcarrier_phase(s, 180);
		lut_i = _colour_subcarrier_phase(s, 90);
		lut_q = _colour_subcarrier_phase(s, 0);
	}
	else if(s->conf.colour_mode == VID_APOLLO_FSC)
	{
		/* Apollo Field Sequential Colour */
		fsc = (s->frame * 2 + (s->line < 264 ? 0 : 1)) % 3;
		lut_b = NULL;
		lut_i = NULL;
		lut_q = NULL;
		pal = 0;
	}
	else
	{
		/* No colour */
		lut_b = NULL;
		lut_i = NULL;
		lut_q = NULL;
		pal = 0;
	}
	
	/* Render the left side sync pulse */
	if(seq[0] == 'v') w = s->vsync_short_width;
	else if(seq[0] == 'V') w = s->vsync_long_width;
	else if(seq[0] == 'h') w = s->hsync_width;
	else w = 0;
	
	for(x = 0; x < w && x < s->half_width; x++)
	{
		s->output[x * 2] = s->sync_level;
	}
	
	/* Render left side of active video if required */
	if(seq[2] == 'a' && vy != -1)
	{
		for(; x < s->active_left; x++)
		{
			s->output[x * 2] = s->blanking_level;
		}
		
		for(; x < s->half_width; x++)
		{
			rgb = s->framebuffer != NULL ? s->framebuffer[vy * s->active_width + x - s->active_left] & 0xFFFFFF : 0x000000;
			
			if(s->conf.colour_mode == VID_APOLLO_FSC)
			{
				rgb  = (rgb >> (8 * fsc)) & 0xFF;
				rgb |= (rgb << 8) | (rgb << 16);
			}
			
			s->output[x * 2] = s->y_level_lookup[rgb];
			
			if(pal)
			{
				s->output[x * 2] += (s->i_level_lookup[rgb] * lut_i[x]) >> 15;
				s->output[x * 2] += (s->q_level_lookup[rgb] * lut_q[x]) >> 15;
			}
		}
	}
	else
	{
		for(; x < s->half_width; x++)
		{
			s->output[x * 2] = s->blanking_level;
		}
	}
	
	if(seq[3] == 'a' && vy != -1)
	{
		for(; x < s->active_left + s->active_width; x++)
		{
			rgb = s->framebuffer != NULL ? s->framebuffer[vy * s->active_width + x - s->active_left] & 0xFFFFFF : 0x000000;
			
			if(s->conf.colour_mode == VID_APOLLO_FSC)
			{
				rgb  = (rgb >> (8 * fsc)) & 0xFF;
				rgb |= (rgb << 8) | (rgb << 16);
			}
			
			s->output[x * 2] = s->y_level_lookup[rgb];
			
			if(pal)
			{
				s->output[x * 2] += (s->i_level_lookup[rgb] * lut_i[x]) >> 15;
				s->output[x * 2] += (s->q_level_lookup[rgb] * lut_q[x]) >> 15;
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
			s->output[x * 2] = s->sync_level;
		}
	}
	
	/* Blank the remainder of the line */
	for(; x < s->width; x++)
	{
		s->output[x * 2] = s->blanking_level;
	}
	
	/* Render the colour burst */
	if(pal)
	{
		for(x = s->burst_left; x < s->burst_left + s->burst_width; x++)
		{
			s->output[x * 2] += (lut_b[x] * s->burst_level) >> 15;
		}
	}
	
	/* Render the FSC flag */
	if(s->conf.colour_mode == VID_APOLLO_FSC && fsc == 1 &&
	  (s->line == 18 || s->line == 281))
	{
		/* The Apollo colour standard transmits one colour per field
		 * (Blue, Red, Green), with the green field indicated by a flag
		 * on field line 18. The flag also indicates the temperature of
		 * the camera by its duration, varying between 5 and 45 μs. The
		 * duration is fixed to 20 μs in hacktv. */
		for(x = s->fsc_flag_left; x < s->fsc_flag_left + s->fsc_flag_width; x++)
		{
			s->output[x * 2] = s->fsc_flag_level;
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
		
		x -= s->burst_left;
		
		for(; x < w; x++)
		{
			rgb = 0x000000;
			
			if(x >= s->active_left && x < s->active_left + s->active_width)
			{
				rgb = s->framebuffer != NULL ? s->framebuffer[vy * s->active_width + x - s->active_left] & 0xFFFFFF : 0x000000;
			}
			
			if(((s->frame * s->conf.lines) + s->line) & 1)
			{
				_fm_modulator_add(&s->fm_secam_cr, &s->output[x * 2], s->i_level_lookup[rgb]);
			}
			else
			{
				_fm_modulator_add(&s->fm_secam_cb, &s->output[x * 2], s->q_level_lookup[rgb]);
			}
		}
	}
	
	/* Teletext, if enabled */
	if(s->conf.teletext)
	{
		tt_render_line(&s->tt);
	}
	
	/* WSS, if enabled */
	if(s->conf.wss)
	{
		wss_render_line(&s->wss);
	}
	
	/* Videocrypt scrambling, if enabled */
	if(s->conf.videocrypt || s->conf.videocrypt2)
	{
		vc_render_line(&s->vc, s->conf.videocrypt, s->conf.videocrypt2, s->conf.key);
	}
	
	/* Videocrypt S scrambling, if enabled */
	if(s->conf.videocrypts)
	{
		vcs_render_line(&s->vcs);
	}
	
	/* Syster scrambling, if enabled */
	if(s->conf.syster == 1)
	{
		ng_render_line(&s->ng);
	}
	
	/* D11 scrambling, if enabled */
	if(s->conf.d11 == 1)
	{
		d11_render_line(&s->ng);
	}
	
	if(s->conf.acp == 1)
	{
		acp_render_line(&s->acp);
	}
	
	/* Clear the Q channel */
	for(x = 0; x < s->width; x++)
	{
		s->output[x * 2 + 1] = 0;
	}
	
	/* Apply video filter if enabled */
	if(s->video_filter_taps)
	{
		if(s->conf.modulation == VID_VSB)
		{
			fir_int16_complex_process(&s->video_filter, s->output, 1, s->output, s->width, 1);
		}
		else if(s->conf.modulation == VID_FM)
		{
			fir_int16_process(&s->video_filter, s->output, 2, s->output, s->width, 2);
		}
	}
	
	/* Generate the FM audio subcarrier(s) */
	if(s->conf.fm_audio_level > 0 || s->conf.am_audio_level > 0)
	{
		static int16_t audio[2] = { 0, 0 };
		static int interp = 0;
		
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
				
				if(s->conf.nicam_level > 0 && s->conf.nicam_carrier != 0)
				{
					s->nicam_buf[s->nicam_buf_len++] = audio[0];
					s->nicam_buf[s->nicam_buf_len++] = audio[1];
					
					if(s->nicam_buf_len == NICAM_AUDIO_LEN * 2)
					{
						nicam_mod_input(&s->nicam, s->nicam_buf);
						s->nicam_buf_len = 0;
					}
				}
			}
			
			if(s->conf.fm_audio_level > 0 && s->conf.fm_mono_carrier != 0)
			{
				_fm_modulator_add(&s->fm_mono, add, (audio[0] + audio[1]) / 2);
			}
			
			if(s->conf.fm_audio_level > 0 && s->conf.fm_left_carrier != 0)
			{
				_fm_modulator_add(&s->fm_left, add, audio[0]);
			}
			
			if(s->conf.fm_audio_level > 0 && s->conf.fm_right_carrier != 0)
			{
				_fm_modulator_add(&s->fm_right, add, audio[1]);
			}
			
			if(s->conf.am_audio_level > 0 && s->conf.am_mono_carrier != 0)
			{
				_am_modulator_add(&s->am_mono, add, (audio[0] + audio[1]) / 2);
			}
			
			s->output[x * 2 + 0] += add[0];
			s->output[x * 2 + 1] += add[1];
		}
	}
	
	if(s->conf.nicam_level > 0 && s->conf.nicam_carrier != 0)
	{
		nicam_mod_output(&s->nicam, s->output, s->width);
	}
	
	/* FM modulate the video and audio if requested */
	if(s->conf.modulation == VID_FM)
	{
		for(x = 0; x < s->width; x++)
		{
			_fm_modulator(&s->fm_video, &s->output[x * 2], s->output[x * 2]);
		}
	}
	
	/* Advance the next line/frame counter */
	if(s->bline++ == s->conf.lines)
	{
		s->bline = 1;
		s->bframe++;
	}
	
	/* Return a pointer to the line buffer */
	*samples = s->width;
	
	/* Rotate the output lines */
	s->output = s->oline[0];
	for(x = 1; x < s->olines; x++)
	{
		s->oline[x - 1] = s->oline[x];
	}
	s->oline[x - 1] = s->output;
	
	return(s->output);
}

int16_t *vid_next_line(vid_t *s, size_t *samples)
{
	int16_t *output;
	
	/* Drop any delay lines introduced by scramblers / filters */
	do
	{
		output = _vid_next_line(s, samples);
	}
	while(s->frame < 1);
	
	return(output);
}

