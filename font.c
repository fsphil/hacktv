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

static FT_Library _freetype = NULL;

int font_init(vid_t *s, int size, float ratio)
{	
	int r;
	int x_res;
	
	char *fontfile = "verdana.ttf";
	
	av_font_t *av;
	
	av = calloc(1, sizeof(av_font_t));
	if(!av)
	{
		return(HACKTV_OUT_OF_MEMORY);
	}
	
	av->vs = s;
	
	/* Hack to deal with different sampling rates */
	x_res = 96.0 * ((float) av->vs->active_width / av->vs->conf.active_lines / ratio);
		
	/* Initialise the freetype library */
	r = FT_Init_FreeType(&_freetype);
	if(r)
	{
		fprintf(stderr, "There was an error initialising the freetype library.\n");
		return(HACKTV_ERROR);
	}
	
	r = FT_New_Face(_freetype, fontfile, 0, &av->fontface);
	if(r == FT_Err_Unknown_File_Format)
	{
		fprintf(stderr, "%s: Unknown font file format.", fontfile);
		return(HACKTV_ERROR);
	}
	else if(r)
	{
		fprintf(stderr, "%s: Error loading font.", fontfile);
		return(HACKTV_ERROR);
	}
	
	r = FT_Set_Char_Size(av->fontface, 0, (FT_F26Dot6) 32 * size, x_res, 96);
	if(r)
	{
		fprintf(stderr, "Error setting font size %d.", 32);
		return(HACKTV_ERROR);
	}
	
	/* Callback */
	s->av_font = av;
	
	return(HACKTV_OK);
}

static uint32_t make_transparent(uint32_t a, uint32_t b, float t)
{
	int vr, vg, vb;
	
	vr = ((b >> 16) & 0xFF) * t + ((a >> 16) & 0xFF) * (1 - t);
	vg = ((b >> 8)  & 0xFF) * t + ((a >> 8)  & 0xFF) * (1 - t);
	vb = ((b >> 0)  & 0xFF) * t + ((a >> 0)  & 0xFF) * (1 - t);
	
	return (vr << 16 | vg << 8 | vb << 0);	
}

static int draw_box(av_font_t *av, int x_start, int y_start, int x_end, int y_end, uint32_t colour)
{
	int i, j;
	uint32_t *dp;
	
	for(i = x_start; i < x_end; i++)
	{
		for(j = y_start; j < y_end; j++)
		{
			dp = &av->video[j * av->vs->active_width + i];
			*dp = make_transparent(*dp, colour, 0.50);
		}
	}
	
	return(0);
}

int display_bitmap_subtitle(av_font_t *av, uint32_t *vid, int w, int h, uint32_t *bitmap)
{
	av->video = vid;
		
	uint32_t c;
	uint32_t *dp;
	int x_start, y_start, x, y, i, j;
	
	x_start = (av->vs->active_width / 2) - (w / 2);
	y_start = (float) (av->vs->conf.active_lines) * 0.8;
	
	for (y = 0, i = y_start; y < h; y++, i++) 
	{
		for(x = 0, j = x_start; x < w; x++, j++)
		{
			dp = &av->video[i * av->vs->active_width + j];
			c = bitmap[y * w + x];
			*dp = c > 0 ? c : *dp;
		}
	}
	
	return (0);
}

static int _printchar(av_font_t *av, FT_Bitmap *bitmap, FT_Int x, FT_Int y, uint32_t colour)
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
			if(i >= av->vs->active_width || j >= av->vs->conf.active_lines) continue;
			
			dp = &av->video[j * av->vs->active_width + i];
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

int _printf(av_font_t *av, int32_t x, int32_t y, uint32_t colour, char *fmt)
{
	FT_GlyphSlot slot;
	FT_F26Dot6 pen_x, pen_y;
	FT_Bool use_kerning;
	FT_UInt previous;
	int i;
	char *s;
	uint32_t u;
	
	if(!_freetype || !av->fontface)
	{
		fprintf(stderr, "Freetype library not initialised or no font set.\n");
		return(HACKTV_ERROR);
	}
	
	/* Todo: Process formatted text */
	
	slot = av->fontface->glyph;
	pen_x = x << 6;
	pen_y = y << 6;
	
	use_kerning = FT_HAS_KERNING(av->fontface);
	previous = 0;
	
	s = fmt;
	while((u = _utf8_to_utf32(s, &s)))
	{
		/* Needs to be done properly */
		/* Check for LF */
		if(u == '\n')
		{
			pen_y = ((pen_y >> 6) + 32) << 6;
			pen_x = x << 6;
		}
		/* Ignore CR in Windows files */
		else if(u == '\r')
		{
			continue;
		}
		else
		{
			FT_UInt glyph_index;
			glyph_index = FT_Get_Char_Index(av->fontface, u);
			
			if(use_kerning && previous && glyph_index)
			{
				FT_Vector delta;
				FT_Get_Kerning(av->fontface, previous, glyph_index, ft_kerning_default, &delta);
				pen_x += delta.x;
			}
			
			i = FT_Load_Glyph(av->fontface, glyph_index, FT_LOAD_RENDER);
			if(i) continue;
			
		
			_printchar(av, &slot->bitmap,
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

int32_t get_line_width(av_font_t *av, char *fmt)
{
	FT_GlyphSlot slot;
	FT_F26Dot6 pen_x;
	FT_Bool use_kerning;
	FT_UInt previous;
	int i;
	int32_t x = 0;
	char *s;
	uint32_t u;
	
	if(!_freetype || !av->fontface)
	{
		fprintf(stderr, "Freetype library not initialised or no font set.\n");
		return(HACKTV_ERROR);
	}
	
	slot = av->fontface->glyph;
	pen_x = x << 6;
	
	use_kerning = FT_HAS_KERNING(av->fontface);
	previous = 0;

	s = fmt;
		
	while((u = _utf8_to_utf32(s, &s)))
	{
		/* Ignore CR in Windows files */
		if(u != '\r')
		{
			FT_UInt glyph_index;
			glyph_index = FT_Get_Char_Index(av->fontface, u);
			
			if(use_kerning && previous && glyph_index)
			{
				FT_Vector delta;
				FT_Get_Kerning(av->fontface, previous, glyph_index, ft_kerning_default, &delta);
				pen_x += delta.x;
			}
			
			i = FT_Load_Glyph(av->fontface, glyph_index, FT_LOAD_RENDER);
			if(i) continue;
			
			pen_x += slot->advance.x;
			previous = glyph_index;
		}
	}
	
	return(pen_x >> 6);
}

void print_line(av_font_t *av, int32_t w, int32_t y, char *fmt)
{
		int32_t x = (float) av->vs->active_width / 2 - w / 2;
		
		int x_box_start = x - 10;
		int x_box_end = x_box_start + w + 20;
		
		int y_box_start = y - 24;
		int y_box_end = y_box_start + 32;
		
		draw_box(av, x_box_start, y_box_start, x_box_end, y_box_end, 0x3A3A3A);

		_printf(av, x + 2, y + 2, 0x000000, fmt);
		_printf(av, x, y, 0xFFFFFF, fmt);
	
}

void print_text(av_font_t *av, uint32_t *vid, int32_t y, char *fmt)
{
	if(strcmp(fmt, "") != 0) 
	{
		int i, p;
		int spacing = 32;
		
		av->video = vid;
		
		int lines = 1;
		char sub_text[128];
		
		for(int a = 0; a < 128; a++) sub_text[a] = '\0';
		
		y = (float) y/100.00 * (float) av->vs->conf.active_lines;
		
		/* Calculate y position */
		for(i = 0; i < strlen(fmt); i++)
		{
			/* Check for new line */
			if(fmt[i] == '\n') 
			{
				/* Move starting y position 1 line up */
				y -=spacing;
				lines++;
			}
		}
		
		/* Calculate y position */
		for(i = p = 0; i < strlen(fmt); i++)
		{
			if(fmt[i] == '\n') 
			{
				print_line(av, get_line_width(av, sub_text), y, sub_text);
				
				/* Move starting y position */
				y += spacing;
				lines--;
				for(int a = 0; a < 128; a++) sub_text[a] = '\0';
				p = 0;
			}
			else
			{
				sub_text[p] = fmt[i];
				p++;
			}
		}
		
		print_line(av, get_line_width(av, sub_text), y + ((lines - 1) * spacing), sub_text);
	}
	
}
