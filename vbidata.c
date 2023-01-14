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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "vbidata.h"

static double _sinc(double x)
{
	return(sin(M_PI * x) / (M_PI * x));
}

static double _raised_cosine(double x, double b, double t)
{
	if(x == 0) return(1.0);
        return(_sinc(x / t) * (cos(M_PI * b * x / t) / (1.0 - (4.0 * b * b * x * x / (t * t)))));
}

static double _win(double t, double left, double width, double rise, double amplitude)
{
	double r, a;
	
	width -= rise;
	t -= left - rise / 2;
	
	if(t <= 0)
	{
		r = 0.0;
	}
	else if(t < rise)
	{
		a = t / rise * M_PI / 2;
		r = pow(sin(a), 2);
	}
	else if(t < rise + width)
	{
		r = 1.0;
	}
	else if(t < rise + width + rise)
	{
		a = (t - width) / rise * M_PI / 2;
		r = pow(sin(a), 2);
	}
	else
	{
		r = 0.0;
	}
	
	return(r * amplitude);
}

static size_t _vbidata_init(int16_t *lut, unsigned int swidth, unsigned int dwidth, int level, int filter, double beta)
{
	size_t l;
	int b, x, lb;
	
	l = 0;
	
	for(lb = b = 0; b < swidth; b++)
	{
		double tt = (1.0 / swidth) * (0.5 + b);
		
		for(x = 0; x < dwidth; x++)
		{
			double tv = (1.0 / dwidth) * (0.5 + x);
			double tr = (tv - tt) * swidth;
			double h = _raised_cosine(tr, beta, 1);
			int16_t w = (int16_t) round(h * level);
			
			if(w != 0)
			{
				if(lb != b)
				{
					if(lut)
					{
						*(lut++) = b;
						*(lut++) = 0;
					}
					
					l += sizeof(int16_t) * 2;
					
					lb = b;
				}
				
				if(lut)
				{
					*(lut++) = x;
					*(lut++) = w;
				}
				
				l += sizeof(int16_t) * 2;
			}
		}
		
		l += sizeof(int16_t) * 2;
	}
	
	if(lut)
	{
		*(lut++) = 0;
		*(lut++) = 0;
	}
	
	l += sizeof(int16_t) * 2;
	
	return(l);
}

int16_t *vbidata_init(unsigned int swidth, unsigned int dwidth, int level, int filter, double beta)
{
	size_t l;
	int16_t *s;
	
	/* Calculate the length of the lookup-table and allocate memory */
	l = _vbidata_init(NULL, swidth, dwidth, level, filter, beta);
	
	s = malloc(l);
	if(!s)
	{
		return(NULL);
	}
	
	/* Generate the lookup-table and return */
	_vbidata_init(s, swidth, dwidth, level, filter, beta);
	
	return(s);
}

static size_t _vbidata_init_step(int16_t *lut, unsigned int swidth, unsigned int dwidth, int level, double rise)
{
	size_t l;
	int b, x, lb;
	
	l = 0;
	
	for(lb = b = 0; b < swidth; b++)
	{
		for(x = 0; x < dwidth; x++)
		{
			double h = _win((double) x, (double) dwidth / swidth * b, (double) dwidth / swidth, rise, level);
			int w = (int16_t) round(h);
			
			if(w != 0)
			{
				if(lb != b)
				{
					if(lut)
					{
						*(lut++) = b;
						*(lut++) = 0;
					}
					
					l += sizeof(int16_t) * 2;
					
					lb = b;
				}
				
				if(lut)
				{
					*(lut++) = x;
					*(lut++) = w;
				}
				
				l += sizeof(int16_t) * 2;
			}
		}
		
		//l += sizeof(int16_t) * 2;
	}
	
	if(lut)
	{
		*(lut++) = 0;
		*(lut++) = 0;
	}
	
	l += sizeof(int16_t) * 2;
	
	return(l);
}

int16_t *vbidata_init_step(unsigned int swidth, unsigned int dwidth, int level, double rise)
{
	size_t l;
	int16_t *s;
	
	/* Calculate the length of the lookup-table and allocate memory */
	l = _vbidata_init_step(NULL, swidth, dwidth, level, rise);
	
	s = malloc(l);
	if(!s)
	{
		return(NULL);
	}
	
	/* Generate the lookup-table and return */
	_vbidata_init_step(s, swidth, dwidth, level, rise);
	
	return(s);
}

void vbidata_render_nrz(const int16_t *lut, const uint8_t *src, int offset, size_t length, int order, int16_t *dst, size_t step)
{
	int b = offset;
	int x = 0;
	int bit;
	
	/* LUT format:
	 * 
	 * [x][v] = [x offset][value]
	 * [x][0] = [x offset][end of bit]
	 * [0][0] = End of LUT
	*/
	
	bit = (b < 0 || b >= length ? 0 : (src[b >> 3] >> (order == VBIDATA_LSB_FIRST ? (b & 7) : 7 - (b & 7))) & 1);
	
	for(; !(lut[0] == 0 && lut[1] == 0); lut += 2)
	{
		if(lut[1] == 0)
		{
			b = lut[0] + offset;
			bit = (b < 0 || b >= length ? 0 : (src[b >> 3] >> (order == VBIDATA_LSB_FIRST ? (b & 7) : 7 - (b & 7))) & 1);
		}
		else if(bit)
		{
			x = lut[0];
			dst[x * step] += lut[1];
		}
	}
}

