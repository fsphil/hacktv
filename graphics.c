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

/*
 * Inspiration from  http://zarb.org/~gc/html/libpng.html
 *
 * Modified by Yoshimasa Niwa to support all possible colour types.
 */
 
#include <stdlib.h>
#include <stdio.h>
#include "video.h"
#include "hacktv.h"

void _overlay_image_logo(uint32_t *framebuffer, image_t *l, int vid_width, int vid_height, int pos)
{
	int i, j, x, y, r, g, b, vi;
	float t;
	uint32_t c;
	int x_start;
	int y_start;

	/* Set logo position */
	if(pos == LOGO_POS_TR)
	{
		x_start = ((float) vid_width * 0.9) - ((float) l->vlogo_width * 0.8);
		y_start = (float) (vid_height) * 0.08;
	}
	
	/* Set logo position */
	if(pos == LOGO_POS_CENTRE)
	{
		x_start = ((float) vid_width * 0.5) - ((float) l->vlogo_width * 0.5);
		y_start = (float) (vid_height) * 0.15;
	}
	
	/* Overlay logo */
	for (y = 0, i = y_start; y < l->vlogo_height; y++, i++) 
	{
		for(x = 0, j = x_start; x < l->vlogo_width; x++, j++)
		{
			/* Get pixel */
			c = l->logo[x + ((l->vlogo_height - y - 1) * l->vlogo_width)];
			
			/* Calculate transparency level */
			t = 1.0 - (float) (c >> 24) / 0xFF;
			
			/* Set logo position */
			vi = i * vid_width + j;
			
			/* Overlay image on video with transparency  */
			r = ((framebuffer[vi] >> 16) & 0xFF) * t + ((c >> 16) & 0xFF) * (1 - t);
			g = ((framebuffer[vi] >> 8)  & 0xFF) * t + ((c >> 8)  & 0xFF) * (1 - t);
			b = ((framebuffer[vi] >> 0)  & 0xFF) * t + ((c >> 0)  & 0xFF) * (1 - t);
			
			framebuffer[vi] = (r << 16 | g << 8 | b << 0);
		}
	}
}

int read_png_file(image_t *image) 
{
	int y;
	FILE *fp = fopen(image->filename, "rb");
	if (!fp)
	{
		fprintf(stderr,"Warning: File %s could not be opened for reading\n", image->filename);
		return (HACKTV_ERROR);
	}
	
	png_byte color_type;
	png_byte bit_depth;
	
	/* Validate the file to ensure that it is, in fact, a PNG file */
	png_const_bytep buf[8];
	fread(buf, 1, 8, fp);
	if(png_sig_cmp((png_const_bytep) buf, (png_size_t) 0, 8))
	{
		fprintf(stderr,"Warning: %s is not a PNG file.\n", image->filename);
		return(HACKTV_ERROR);
	}
	
	/* Define png structure */
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	
	/* Allocate/initialize the memory for image information. */
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL)
	{
		fclose(fp);
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		fprintf(stderr,"Warning: Error allocating memory for file %s.\n", image->filename);
		return (HACKTV_OUT_OF_MEMORY);
	}
	
	if(setjmp(png_jmpbuf(png_ptr)))
	{
		/* Free all of the memory associated with the png_ptr and info_ptr */
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		fclose(fp);
		
		fprintf(stderr,"Warning: Error reading file %s.\n", image->filename);
		return (HACKTV_ERROR);
	};
	
	/* Set up input stream */
	png_init_io(png_ptr, fp);
	
	/* Skip signature bytes */
	png_set_sig_bytes(png_ptr, 8);
	
	/* Read image details */
	png_read_info(png_ptr, info_ptr);
	
	image->width = png_get_image_width(png_ptr, info_ptr);
	image->height = png_get_image_height(png_ptr, info_ptr);
	color_type = png_get_color_type(png_ptr, info_ptr);
	bit_depth = png_get_bit_depth(png_ptr, info_ptr);
	
	/* Convert to 8-bit RBGA, if required */
	if(bit_depth == 16)
	{
		png_set_strip_16(png_ptr);
	}
	
	if(color_type == PNG_COLOR_TYPE_PALETTE)
	{
		png_set_palette_to_rgb(png_ptr);
	}
	
	if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
	{
		png_set_expand_gray_1_2_4_to_8(png_ptr);
	}
	
	if(png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
	{
		png_set_tRNS_to_alpha(png_ptr);
	}
	
	/* Fill alpha channel with 0xFF (white) if it's missing */
	if(color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE)
	{
		png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
	}
	
	if(color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
	{
		png_set_gray_to_rgb(png_ptr);
	}
	
	/* Update PNG info with any changes from above */
	png_read_update_info(png_ptr, info_ptr);
	
	/* Allocate memory to image */
	image->row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * image->height);
	
	for(y = 0; y < image->height; y++)
	{
		image->row_pointers[y] = (png_byte*) malloc(png_get_rowbytes(png_ptr, info_ptr));
	}
	
	/* Load image into memory */
	png_read_image(png_ptr, image->row_pointers);
	
	/* Close file */
	fclose(fp);
	
	/* Free memory */
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	
	return (HACKTV_OK);
}

int _load_png(image_t *image, int width, int height, char *filename, float scale, float ratio)
{
	image->filename = filename;
	image->vratio = ratio;
	if(read_png_file(image) == 0)
	{
		image->vlogo_width = image->width * scale / (image->vratio) / ((float) height / (float) width) * 1.02;
		image->vlogo_height = image->height * scale;
		int i, j, k;
		uint32_t *logo;
		
		logo = malloc(image->width * image->height * sizeof(uint32_t));
		image->logo = malloc(image->vlogo_width * image->vlogo_height * sizeof(uint32_t));
		
		for(i = image->height - 1, k = 0; i > -1 ; i--)
		{	
			png_bytep row = image->row_pointers[i];
			for(j = 0; j < image->width; j++, k++)
			{
				png_bytep px = &(row[j * 4]);
				logo[k] = px[3] << 24 | px[0] << 16 | px[1] << 8 | px[2] << 0;
			}
		}
		
		resize_bitmap(logo, image->logo, image->width, image->height, image->vlogo_width, image->vlogo_height);
		return(HACKTV_OK);
	}
	
	return(HACKTV_ERROR);
}


/* Inspiration from http://tech-algorithm.com/articles/bilinear-image-scaling/ */

void resize_bitmap(uint32_t *input, uint32_t *output, int old_width, int old_height, int new_width, int new_height) 
{
	int a, b, c, d, x, y, index;
	float x_ratio = ((float)(old_width - 1)) / new_width;
	float y_ratio = ((float)(old_height - 1)) / new_height;
	float x_diff, y_diff, blue, red, green, alpha;
	int offset = 0 ;
	
	for (int i = 0; i < new_height; i++) 
	{
		for (int j = 0; j < new_width; j++) 
		{
			x = (int)(x_ratio * j);
			y = (int)(y_ratio * i);
			x_diff = (x_ratio * j) - x;
			y_diff = (y_ratio * i) - y;
			index = (y * old_width + x);
			a = input[index];
			b = input[index + 1];
			c = input[index + old_width];
			d = input[index + old_width + 1];
			
			/* Blue */
			blue = (a & 0xFF) * ( 1- x_diff) * (1 - y_diff) + (b & 0xFF) * (x_diff) * (1 - y_diff) +
				   (c & 0xFF) * (y_diff) * (1 - x_diff)   + (d & 0xFF) * (x_diff * y_diff);
			
			/* Green */
			green = ((a >> 8) & 0xFF) * (1-x_diff) * (1 - y_diff) + ((b >> 8) & 0xFF) * (x_diff) * (1 - y_diff) +
					((c >> 8) & 0xFF) * (y_diff) * (1 - x_diff)   + ((d >> 8) & 0xFF) * (x_diff * y_diff);
			
			/* Reg */
			red = ((a >> 16) & 0xFF) * (1 - x_diff) * (1 - y_diff) + ((b >> 16) & 0xFF) * (x_diff) * (1-y_diff) +
				  ((c >> 16) & 0xFF) * (y_diff) * (1 - x_diff)   + ((d >> 16) & 0xFF) * (x_diff * y_diff);
			
			// /* Alpha */
			alpha = ((a >> 24) & 0xFF) * (1-x_diff) * (1 - y_diff) + ((b >> 24) & 0xFF) * (x_diff) * (1 - y_diff) +
					((c >> 24) & 0xFF) * (y_diff) * (1 - x_diff)   + ((d >> 24) & 0xFF) * (x_diff * y_diff);
			
			output[offset++] = 
					((((int) alpha) << 24) & 0xFF000000) |
					((((int) red)   << 16) & 0xFF0000)   |
					((((int) green) << 8)  & 0xFF00)     |
					((((int) blue)  << 0)  & 0xFF);
		}
	}
}

// void process_png_file(image_t *image) {
// 	for(int y = 0; y < image->height; y++) 
// 	{
// 		png_bytep row = image->row_pointers[y];
// 		for(int x = 0; x < image->width; x++)
// 		{
// 			png_bytep px = &(row[x * 4]);
// 			// Do something awesome for each pixel here...
// 			printf("%4d, %4d = RGBA(%3d, %3d, %3d, %3d)\n", x, y, px[0], px[1], px[2], px[3]);
// 			//printf("%s", px[0] ? "*" : " ");
// 		}
// 		// printf("\n");
// 	}
// }