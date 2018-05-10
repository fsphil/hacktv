/* Nagravision Syster encoder for hacktv                                 */
/*=======================================================================*/
/* Copyright 2018 Alex L. James                                          */
/* Copyright 2018 Philip Heron <phil@sanslogic.co.uk>                    */
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

#ifndef _SYSTER_H
#define _SYSTER_H

#include <stdint.h>
#include "video.h"

#define NG_FIELD_1_START   23
#define NG_FIELD_2_START   336
#define NG_LINES_PER_FIELD 287

/* NG_DELAY_LINES needs to be long enough for the scrambler to access any
 * line in the next field from at least the last 32 lines of the current.
 * This is a safe amount and can probably be reduced. */

#define NG_DELAY_LINES (625 + NG_FIELD_1_START + NG_LINES_PER_FIELD - (NG_FIELD_2_START + NG_LINES_PER_FIELD - 32))

typedef struct {
	
	vid_t *vid;
	
	/* PRNG seed values */
	int s; /* 0, ..., 127 */
	int r; /* 0, ..., 255 */
	
	/* The line order for the next field (0-287) */
	int order[NG_LINES_PER_FIELD];
	
	/* Delay line */
	int16_t *delay;
	int16_t *delay_line[NG_DELAY_LINES];
	
} ng_t;

extern int ng_init(ng_t *s, vid_t *vs);
extern void ng_free(ng_t *s);
extern void ng_render_line(ng_t *s);

#endif

