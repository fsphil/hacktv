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

#ifndef _VBIDATA_H
#define _VBIDATA_H

#define VBIDATA_FILTER_RC (0)

#define VBIDATA_LSB_FIRST (0)
#define VBIDATA_MSB_FIRST (1)

extern int16_t *vbidata_init(unsigned int swidth, unsigned int dwidth, int16_t level, int filter, double beta);
extern void     vbidata_render_nrz(const int16_t *lut, const uint8_t *src, int offset, size_t length, int order, int16_t *dst, size_t step);

#endif

