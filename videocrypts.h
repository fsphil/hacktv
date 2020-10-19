/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2019 Philip Heron <phil@sanslogic.co.uk>                    */
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

#ifndef _VIDEOCRYPTS_H
#define _VIDEOCRYPTS_H

#include <stdint.h>
#include "video.h"

#define VCS_SAMPLE_RATE         17734475
#define VCS_WIDTH               1135
#define VCS_VBI_LEFT            211
#define VCS_VBI_FIELD_1_START   24
#define VCS_VBI_FIELD_2_START   336
#define VCS_VBI_LINES_PER_FIELD 4
#define VCS_VBI_LINES_PER_FRAME (VCS_VBI_LINES_PER_FIELD * 2)
#define VCS_VBI_SAMPLES_PER_BIT 22
#define VCS_VBI_BITS_PER_LINE   40
#define VCS_VBI_BYTES_PER_LINE  (VCS_VBI_BITS_PER_LINE / 8)
#define VCS_PACKET_LENGTH       32

/* This is not correct - just a placeholder */
#define VCS_PRBS_CW_FA (((uint64_t) 1 << 60) - 1)

/* VCS_DELAY_LINES needs to be long enough for the scrambler to access any
 * line in the next block, which may be in the next field... */

#define VCS_DELAY_LINES 125

typedef struct {
	uint8_t mode;
	uint8_t channel;
	uint64_t codeword;
	uint8_t messages[8][32];
} _vcs_block_t;

typedef struct {
	
	uint8_t counter;
	
	/* VCS blocks */
	const _vcs_block_t *blocks;
	size_t block_num;
	size_t block_len;
	uint8_t message[32];
	uint8_t vbi[VCS_VBI_BYTES_PER_LINE * VCS_VBI_LINES_PER_FRAME];
	
	int block[47];
	
	int video_scale[VCS_WIDTH];
	
} vcs_t;

extern int vcs_init(vcs_t *s, vid_t *vs, const char *mode);
extern void vcs_free(vcs_t *s);
extern int vcs_render_line(vid_t *s, void *arg, int nlines, vid_line_t **lines);

#endif

