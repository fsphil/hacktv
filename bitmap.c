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

#include "bitmap.h"
#include <stdio.h>
#include <stdint.h>

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
