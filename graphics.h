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

#ifndef GRAPHICS_H_
#define GRAPHICS_H_

#include <stdint.h>
#include <stdlib.h>
#include <png.h>
#include "video.h"

#define LOGO_POS_CENTRE 0
#define LOGO_POS_TL 1
#define LOGO_POS_TR 2
#define LOGO_POS_BL 3
#define LOGO_POS_BR 4

typedef struct {
	char *filename;
	int width;
	int height;
	float vratio;
	int vlogo_width;
	int vlogo_height;
	uint32_t *logo;
	png_bytep *row_pointers;
} image_t;

extern int read_png_file(image_t *image);
extern void _overlay_image_logo(uint32_t *framebuffer, image_t *l, int vid_width, int vid_height, int pos);
extern int _load_png(image_t *image, int width, int height, char *filename, float scale, float ratio);
extern void resize_bitmap(uint32_t *input, uint32_t *output, int old_width, int old_height, int new_width, int new_height);
#endif
