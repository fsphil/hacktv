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

#include <ft2build.h>
#include FT_FREETYPE_H
#include "hacktv.h"
#include "font.h"
#include "fonts.h"

static FT_Library _freetype = NULL;

int font_init(vid_t *s, int size, float ratio)
{	
	int r;
	int x_res;
	
	av_font_t *font;
	
	font = calloc(1, sizeof(av_font_t));
	if(!font)
	{
		fprintf(stderr, "Font memory allocation error.\n");
		return(HACKTV_OUT_OF_MEMORY);
	}
	
	/* Normalise ratio */
	ratio = ratio >= 14.0/9.0 ? 16.0/9.0 : 4.0/3.0;
	
	font->font_size = size;
	font->video_width = s->active_width;
	font->video_height = s->conf.active_lines;
	font->video_ratio = s->conf.pillarbox || s->conf.letterbox ? 4.0 / 3.0 : ratio;
	
	/* Hack to deal with different sampling rates */
	x_res = 96.0 * ((float) font->video_width / font->video_height / font->video_ratio);
	
	/* Initialise the freetype library */
	r = FT_Init_FreeType(&_freetype);
	if(r)
	{
		fprintf(stderr, "There was an error initialising the freetype library.\n");
		return(HACKTV_ERROR);
	}
	
	// r = FT_New_Face(_freetype, fontfile, 0, &font->fontface);
	r = FT_New_Memory_Face( _freetype,
                            _font_evolventa,    /* first byte in memory */
                            sizeof(_font_evolventa),      /* size in bytes        */
                            0,         /* face_index           */
                            &font->fontface );
	if(r == FT_Err_Unknown_File_Format)
	{
		fprintf(stderr, "Unknown font file format.");
		return(HACKTV_ERROR);
	}
	else if(r)
	{
		fprintf(stderr, "Error loading font.");
		return(HACKTV_ERROR);
	}
	
	r = FT_Set_Char_Size(font->fontface, 0, (FT_F26Dot6) 32 * size, x_res, 96);
	if(r)
	{
		fprintf(stderr, "Error setting font size %d.", 32);
		return(HACKTV_ERROR);
	}
	
	/* Callback */
	s->av_font = font;
	
	return(HACKTV_OK);
}

static uint32_t _make_transparent(uint32_t a, uint32_t b, float t)
{
	int vr, vg, vb;
	
	vr = ((b >> 16) & 0xFF) * t + ((a >> 16) & 0xFF) * (1 - t);
	vg = ((b >> 8)  & 0xFF) * t + ((a >> 8)  & 0xFF) * (1 - t);
	vb = ((b >> 0)  & 0xFF) * t + ((a >> 0)  & 0xFF) * (1 - t);
	
	return (vr << 16 | vg << 8 | vb << 0);	
}

static int draw_box(av_font_t *font, int x_start, int y_start, int x_end, int y_end, uint32_t colour, float transparency)
{
	int i, j;
	uint32_t *dp;
	
	for(i = x_start; i < x_end; i++)
	{
		for(j = y_start; j < y_end; j++)
		{
			dp = &font->video[j * font->video_width + i];
			*dp = _make_transparent(*dp, colour, transparency);
		}
	}
	
	return(0);
}

int display_bitmap_subtitle(av_font_t *font, uint32_t *vid, int w, int h, uint32_t *bitmap)
{
	font->video = vid;
		
	uint32_t c, *dp;
	int i, j, x_start, y_start, x, y;
	
	x_start = (font->video_width / 2) - (w / 2);
	y_start = (font->video_height) * 0.8;
	
	for (y = 0, i = y_start; y < h; y++, i++) 
	{
		for(x = 0, j = x_start; x < w; x++, j++)
		{
			dp = &font->video[i * font->video_width + j];
			c = bitmap[y * w + x];
			*dp = c > 0 ? c : *dp;
		}
	}
	
	return (0);
}

static int _printchar(av_font_t *font, FT_Bitmap *bitmap, FT_Int x, FT_Int y, uint32_t colour)
{
	FT_Int i, j, p, q;
	FT_Int x_max = x + bitmap->width;
	FT_Int y_max = y + bitmap->rows;
	
	for(i = x, p = 0; i < x_max; i++, p++)
	{
		for(j = y, q = 0; j < y_max; j++, q++)
		{
			uint32_t *dp;
			uint8_t r, g, b;
			int c;
			
			if(i < 0 || j < 0) continue;
			if(i >= font->video_width || j >= font->video_height) continue;
			
			dp = &font->video[j * font->video_width + i];
			c = bitmap->buffer[q * bitmap->width + p];
			
			r = (((*dp >> 16) & 0xFF) * (255 - c) + ((colour >> 16) & 0xFF) * c) / 256;
			g = (((*dp >>  8) & 0xFF) * (255 - c) + ((colour >>  8) & 0xFF) * c) / 256;
			b = (((*dp >>  0) & 0xFF) * (255 - c) + ((colour >>  0) & 0xFF) * c) / 256;
			
			*dp = (r << 16) | (g << 8) | (b << 0);
		}
	}
	
	return(0);
}

const uint32_t _utf8_to_utf32(char *str, char **next)
{
	const uint8_t *c;
	uint32_t u, m;
	uint8_t b;
	
	c = (const uint8_t *) str;
	if(next) *next = str + 1;
	
	if(*c < 0x80) return(*c);
	else if((*c & 0xE0) == 0xC0) { u = *c & 0x1F; b = 1; m = 0x00000080; }
	else if((*c & 0xF0) == 0xE0) { u = *c & 0x0F; b = 2; m = 0x00000800; }
	else if((*c & 0xF8) == 0xF0) { u = *c & 0x07; b = 3; m = 0x00010000; }
	else if((*c & 0xFC) == 0xF8) { u = *c & 0x03; b = 4; m = 0x00200000; }
	else if((*c & 0xFE) == 0xFC) { u = *c & 0x01; b = 5; m = 0x04000000; }
	else return(0xFFFD);
	
	while(b--)
	{
		if((*(++c) & 0xC0) != 0x80) return(0xFFFD);
		
		u <<= 6;
		u |= *c & 0x3F;
		
		if(next) (*next)++;
	}
	
	if(u < m) return(0xFFFD);
	
	return(u);
}

int _printf(av_font_t *font, int32_t x, int32_t y, uint32_t colour, char *fmt)
{
	FT_GlyphSlot slot;
	FT_F26Dot6 pen_x, pen_y;
	FT_Bool use_kerning;
	FT_UInt previous;
	int i;
	char *s;
	uint32_t u;
	
	if(!_freetype || !font->fontface)
	{
		fprintf(stderr, "Freetype library not initialised or no font set.\n");
		return(HACKTV_ERROR);
	}
	
	/* Todo: Process formatted text */
	
	slot = font->fontface->glyph;
	pen_x = x << 6;
	pen_y = y << 6;
	
	use_kerning = FT_HAS_KERNING(font->fontface);
	previous = 0;
	
	s = fmt;
	while((u = _utf8_to_utf32(s, &s)))
	{
		/* Ignore CR in Windows files */
		if(u != '\r')
		{
			FT_UInt glyph_index;
			glyph_index = FT_Get_Char_Index(font->fontface, u);
			
			if(use_kerning && previous && glyph_index)
			{
				FT_Vector delta;
				FT_Get_Kerning(font->fontface, previous, glyph_index, ft_kerning_default, &delta);
				pen_x += delta.x;
			}
			
			i = FT_Load_Glyph(font->fontface, glyph_index, FT_LOAD_RENDER);
			if(i) continue;
			
			_printchar(font, &slot->bitmap,
				(pen_x >> 6) + slot->bitmap_left,
				(pen_y >> 6) - slot->bitmap_top,
				colour);
			
			pen_x += slot->advance.x;
			pen_y += slot->advance.y;
			
			previous = glyph_index;
		}
	}
	
	return(0);
}

static int _get_line_size(av_font_t *font, char *fmt, int *line_width, int *line_height)
{
	FT_GlyphSlot slot;
	FT_F26Dot6 pen_x;
	FT_Bool use_kerning;
	FT_UInt previous;
	int i;
	int32_t x = 0;
	char *s;
	uint32_t u;
	
	*line_width = 0;
	*line_height = 0;
	
	if(!_freetype || !font->fontface)
	{
		fprintf(stderr, "Freetype library not initialised or no font set.\n");
		return(HACKTV_ERROR);
	}
	
	slot = font->fontface->glyph;
	pen_x = x << 6;
	
	use_kerning = FT_HAS_KERNING(font->fontface);
	previous = 0;

	s = fmt;
		
	while((u = _utf8_to_utf32(s, &s)))
	{
		/* Ignore CR in Windows files */
		if(u != '\r')
		{
			FT_UInt glyph_index;
			glyph_index = FT_Get_Char_Index(font->fontface, u);
			
			if(use_kerning && previous && glyph_index)
			{
				FT_Vector delta;
				FT_Get_Kerning(font->fontface, previous, glyph_index, ft_kerning_default, &delta);
				pen_x += delta.x;
			}
			
			i = FT_Load_Glyph(font->fontface, glyph_index, FT_LOAD_RENDER);
			if(i) continue;
			
			pen_x += slot->advance.x;
			previous = glyph_index;
			
			*line_height = slot->metrics.height >> 6 > *line_height ? slot->metrics.height >> 6 : *line_height;
		}
	}
	
	*line_width = pen_x >> 6;
	return(HACKTV_OK);
}

static void _print_line(av_font_t *font, int line_width, int line_height, int pos_x, int pos_y, char *fmt, int shadow, int box, uint32_t colour, float transparency)
{
		if(box)
		{
			int x_box_start = pos_x - 5;
			int x_box_end = x_box_start + line_width + 10;
			
			int y_box_start = pos_y - (line_height * 1.15);
			int y_box_end = y_box_start + (line_height * 1.425);
			draw_box(font, x_box_start, y_box_start, x_box_end, y_box_end, colour, transparency);
		}
		if(shadow) _printf(font, pos_x + 2, pos_y + 2, 0x000000, fmt);
		_printf(font, pos_x, pos_y, 0xFFFFFF, fmt);
}

void print_subtitle(av_font_t *font, uint32_t *vid, char *fmt)
{
	if(strcmp(fmt, "") != 0) 
	{
		int i, p, x, y;
		int spacing = 32;
		
		font->video = vid;
		
		int lines = 1;
		char text[128];
		
		int line_width;
		int line_height;
		
		for(int a = 0; a < 128; a++) text[a] = '\0';
		y = 90.0 / 100.00 * font->video_height;
		
		/* Calculate y position */
		for(i = 0; i < strlen(fmt); i++)
		{
			/* Check for new line */
			if(fmt[i] == '\n') 
			{
				/* Move starting y position 1 line up for every line break */
				y -=spacing;
				lines++;
			}
		}
		
		/* Print multiple lines, if needed */
		for(i = p = 0; i < strlen(fmt); i++)
		{
			if(fmt[i] == '\n') 
			{
				_get_line_size(font, text, &line_width, &line_height);
				
				/* Centre line on screen */
				x = font->video_width / 2 - line_width / 2;
				
				_print_line(font, line_width, line_height, x, y, text, 1, 1, 0x3A3A3A, 0.5);
				
				/* Move starting y position */
				y += spacing;
				lines--;
				for(int a = 0; a < 128; a++) text[a] = '\0';
				p = 0;
			}
			else
			{
				text[p++] = fmt[i];
			}
		}
		
		_get_line_size(font, text, &line_width, &line_height);
		
		/* Centre line on screen */
		x = font->video_width / 2 -  line_width / 2;
		
		_print_line(font, line_width, line_height, x, y + ((lines - 1) * spacing), text, 1, 1, 0x3A3A3A, 0.5);
	}
}

void print_generic_text(av_font_t *font, uint32_t *vid, char *fmt, float pos_x, float pos_y, int shadow, int box, int colour, int transparency)
{
	if(strcmp(fmt, "") != 0)
	{
		int line_width;
		int line_height;
		
		font->video = vid;
		
		_get_line_size(font, fmt, &line_width, &line_height);
		
		/* Centre if 50% */
		pos_x = font->video_width * (pos_x / 100.00) - (pos_x != 50 ? 0 : line_width * (pos_x / 100.00));
		pos_y = pos_y / 100.00 * font->video_height;
		_print_line(font, line_width, line_height, (int) pos_x, (int) pos_y, fmt, shadow, box, colour, transparency);
	}
}
