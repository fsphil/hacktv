/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
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

#include "video.h"

#ifndef _VBIDATA_H
#define _VBIDATA_H

#define VBIDATA_FILTER_RC (0)

#define VBIDATA_LSB_FIRST (0)
#define VBIDATA_MSB_FIRST (1)

typedef struct {
	int16_t length;
	int16_t offset;
	int16_t value[];
} vbidata_lut_t;

extern void vbidata_update(vbidata_lut_t *lut, int render, int offset, int value);
extern int vbidata_update_step(vbidata_lut_t *lut, double offset, double width, double rise, int level);
extern vbidata_lut_t *vbidata_init(unsigned int nsymbols, unsigned int dwidth, int level, int filter, double bwidth, double beta, double offset);
extern vbidata_lut_t *vbidata_init_step(unsigned int nsymbols, unsigned int dwidth, int level, double width, double rise, double offset);
extern void vbidata_render(const vbidata_lut_t *lut, const uint8_t *src, int offset, int length, int order, vid_line_t *line);

#endif

