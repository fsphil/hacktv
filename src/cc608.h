/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2025 Philip Heron <phil@sanslogic.co.uk>                    */
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

#ifndef _CC608_H
#define _CC608_H

#include <stdint.h>
#include "video.h"

typedef struct {
	
	/* Config */
	int lines[2];
	
	/* Clock run-in signal */
	int cri_x;
	int cri_len;
	int16_t *cri;
	
	/* VBI renderer lookup */
	vbidata_lut_t *lut;
	
	/* Active and next message */
	const uint8_t *msg;
	const uint8_t *nmsg;
	
} cc608_t;

extern int cc608_init(cc608_t *s, vid_t *vid);
extern void cc608_free(cc608_t *s);

extern int cc608_render(vid_t *s, void *arg, int nlines, vid_line_t **lines);

#endif

