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


#ifndef FONT_H_
#define FONT_H_

#include <ft2build.h>
#include FT_FREETYPE_H
#include "video.h"

typedef struct {
	uint32_t *video;
	vid_t *vs;
	FT_Face fontface;
} av_font_t;


extern int font_init(vid_t *s, int size, float ratio);
extern void print_text(av_font_t *av, uint32_t *vid, int32_t y, char *fmt);
extern int display_bitmap_subtitle(av_font_t *av, uint32_t *vid, int w, int h, uint32_t *bitmap_data);
#endif
