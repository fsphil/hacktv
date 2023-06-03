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
#include "common.h"

static double _sinc(double x)
{
	return(sin(M_PI * x) / (M_PI * x));
}

static double _raised_cosine(double x, double b, double t)
{
	if(x == 0) return(1.0);
        return(_sinc(x / t) * (cos(M_PI * b * x / t) / (1.0 - (4.0 * b * b * x * x / (t * t)))));
}

void vbidata_update(vbidata_lut_t *lut, int render, int offset, int value)
{
	if(value != 0)
	{
		if(lut->length == 0)
		{
			lut->offset = offset;
		}
		
		for(; lut->length < (offset - lut->offset); lut->length++)
		{
			if(render)
			{
				lut->value[lut->length] = 0;
			}
		}
		
		if(render)
		{
			lut->value[lut->length] = value;
		}
		
		lut->length++;
	}
}

static int _vbidata_init(vbidata_lut_t *lut, unsigned int nsymbols, unsigned int dwidth, int level, int filter, double bwidth, double beta, double offset)
{
	int l;
	int b, x;
	vbidata_lut_t lc;
	vbidata_lut_t *lptr = (lut ? lut : &lc);
	
	l = 0;
	
	for(b = 0; b < nsymbols; b++)
	{
		double t = -bwidth * b - offset;
		
		lptr->offset = lptr->length = 0;
		
		for(x = 0; x < dwidth; x++)
		{
			double h = _raised_cosine((t + x) / bwidth, beta, 1) * level;
			vbidata_update(lptr, lut ? 1 : 0, x, round(h));
		}
		
		l += 2 + lptr->length;
		
		if(lut)
		{
			lptr = (vbidata_lut_t *) &lptr->value[lptr->length];
		}
	}
	
	/* End of LUT marker */
	if(lut)
	{
		lptr->length = -1;
	}
	
	l++;
	
	return(l * sizeof(int16_t));
}

vbidata_lut_t *vbidata_init(unsigned int nsymbols, unsigned int dwidth, int level, int filter, double bwidth, double beta, double offset)
{
	int l;
	vbidata_lut_t *lut;
	
	/* Calculate the length of the lookup-table and allocate memory */
	l = _vbidata_init(NULL, nsymbols, dwidth, level, filter, bwidth, beta, offset);
	
	lut = malloc(l);
	if(!lut)
	{
		return(NULL);
	}
	
	/* Generate the lookup-table and return */
	_vbidata_init(lut, nsymbols, dwidth, level, filter, bwidth, beta, offset);
	
	return(lut);
}

static int _vbidata_init_step(vbidata_lut_t *lut, unsigned int nsymbols, unsigned int dwidth, int level, double width, double rise, double offset)
{
	int l;
	int b, x;
	vbidata_lut_t lc;
	vbidata_lut_t *lptr = (lut ? lut : &lc);
	
	l = 0;
	
	for(b = 0; b < nsymbols; b++)
	{
		lptr->offset = lptr->length = 0;
		
		for(x = 0; x < dwidth; x++)
		{
			double h = rc_window((double) x - offset, width * b, width, rise) * level;
			vbidata_update(lptr, lut ? 1 : 0, x, round(h));
		}
		
		l += 2 + lptr->length;
		
		if(lut)
		{
			lptr = (vbidata_lut_t *) &lptr->value[lptr->length];
		}
	}
	
	/* End of LUT marker */
	if(lut)
	{
		lptr->length = -1;
	}
	
	l++;
	
	return(l * sizeof(int16_t));
}

vbidata_lut_t *vbidata_init_step(unsigned int nsymbols, unsigned int dwidth, int level, double width, double rise, double offset)
{
	int l;
	vbidata_lut_t *lut;
	
	/* Calculate the length of the lookup-table and allocate memory */
	l = _vbidata_init_step(NULL, nsymbols, dwidth, level, width, rise, offset);
	lut = malloc(l);
	if(!lut)
	{
		return(NULL);
	}
	
	/* Generate the lookup-table and return */
	_vbidata_init_step(lut, nsymbols, dwidth, level, width, rise, offset);
	
	return(lut);
}

void vbidata_render(const vbidata_lut_t *lut, const uint8_t *src, int offset, int length, int order, vid_line_t *line)
{
	int b = -offset;
	int x;
	int bit;
	
	/* LUT format:
	 * 
	 * [l][x][[v]...] = [length][x offset][[value]...]
	 * [-1]           = End of LUT
	*/
	
	for(; b < length && lut->length != -1; b++, lut = (vbidata_lut_t *) &lut->value[lut->length])
	{
		bit = (b < 0 ? 0 : (src[b >> 3] >> (order == VBIDATA_LSB_FIRST ? (b & 7) : 7 - (b & 7))) & 1);
		
		if(bit)
		{
			for(x = 0; x < lut->length; x++)
			{
				line->output[(lut->offset + x) * 2] += lut->value[x];
			}
		}
	}
}

