/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2020 Philip Heron <phil@sanslogic.co.uk>                    */
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

#ifndef _VITS_H
#define _VITS_H

#include <stdint.h>
#include "video.h"

typedef struct {
	int width;
	int lines;
	int16_t *line[4];
} vits_t;

extern int vits_init(vits_t *s, unsigned int sample_rate, int width, int lines, int level);
extern void vits_free(vits_t *s);

extern int vits_render(vid_t *s, void *arg, int nlines, vid_line_t **lines);

#endif

