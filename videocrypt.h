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

#ifndef _VIDEOCRYPT_H
#define _VIDEOCRYPT_H

#include <stdint.h>
#include "video.h"

#define VC_SAMPLE_RATE         14000000
#define VC_WIDTH               (VC_SAMPLE_RATE / 25 / 625)
#define VC_VBI_LEFT            120
#define VC_VBI_FIELD_1_START   12
#define VC_VBI_FIELD_2_START   325
#define VC_VBI_LINES_PER_FIELD 4
#define VC_VBI_LINES_PER_FRAME (VC_VBI_LINES_PER_FIELD * 2)
#define VC_VBI_SAMPLES_PER_BIT 18
#define VC_VBI_BITS_PER_LINE   40
#define VC_VBI_BYTES_PER_LINE  (VC_VBI_BITS_PER_LINE / 8)
#define VC_PACKET_LENGTH       32

#define VC_LEFT                120
#define VC_RIGHT               (VC_LEFT + 710)
#define VC_OVERLAP             15
#define VC_FIELD_1_START       24
#define VC_FIELD_2_START       336
#define VC_LINES_PER_FIELD     287
#define VC_LINES_PER_FRAME     (VC_LINES_PER_FIELD * 2)

typedef struct {
	
	vid_t *vid;
	
	uint8_t counter;
	uint8_t mode;
	size_t command;
	uint8_t vbi[VC_VBI_BYTES_PER_LINE * VC_VBI_LINES_PER_FRAME];
	
	int16_t *delay;
	int video_scale[VC_WIDTH];
	
} vc_t;

extern int vc_init(vc_t *s, vid_t *vs);
extern void vc_free(vc_t *s);
extern void vc_render_line(vc_t *s);

#endif

