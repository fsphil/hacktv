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

#include <png.h>
#include "video.h"

#define IMG_POS_CENTRE 0
#define IMG_POS_TL 1
#define IMG_POS_TR 2
#define IMG_POS_BL 3
#define IMG_POS_BR 4
#define IMG_POS_FULL 5

#define IMG_TEST 0
#define IMG_LOGO 1

#define MAX_PNG_SIZE 128

typedef struct {
	char *name;
	int width;
	int height;
	int img_width;
	int img_height;
	uint32_t *logo;
	png_bytep *row_pointers;
} image_t;

typedef struct {
	const uint8_t data[MAX_PNG_SIZE * 1024];
} png_t;

typedef struct {
	const char *name;
	const png_t *png;
	size_t size;
} pngs_t;

typedef struct {
	size_t size;
	const uint8_t *data;
	const uint8_t *cursor;
} png_mem_t;

extern int read_png_file(image_t *image);
extern void overlay_image(uint32_t *framebuffer, image_t *l, int vid_width, int vid_height, int pos);
extern int load_png(image_t *image, int width, int height, char *filename, float scale, float ratio, int type);
extern void resize_bitmap(uint32_t *input, uint32_t *output, int old_width, int old_height, int new_width, int new_height);
#endif
