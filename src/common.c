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

#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "common.h"

int64_t gcd(int64_t a, int64_t b)
{
	int64_t c;
	
	while((c = a % b))
	{
		a = b;
		b = c;
	}
	
	return(b);
}

rational_t rational_mul(rational_t a, rational_t b)
{
	int64_t c, d, e;
	c = (int64_t) a.num * b.num;
	d = (int64_t) a.den * b.den;
	e = gcd(c, d);
	return((rational_t) { c / e, d / e });
}

rational_t rational_div(rational_t a, rational_t b)
{
	int64_t c, d, e;
	c = (int64_t) a.num * b.den;
	d = (int64_t) a.den * b.num;
	e = gcd(c, d);
	return((rational_t) { c / e, d / e });
}

int rational_cmp(rational_t a, rational_t b)
{
	int64_t c = (int64_t) a.num * b.den - (int64_t) a.den * b.num;
	return(c < 0 ? -1 : (c > 0 ? 1 : 0));
}

cint16_t *sin_cint16(unsigned int length, unsigned int cycles, double level)
{
	cint16_t *lut;
	unsigned int i;
	double d;
	
	lut = malloc(length * sizeof(cint16_t));
	if(!lut)
	{
		return(NULL);
	}
	
	d = 2.0 * M_PI / length * cycles;
	for(i = 0; i < length; i++)
	{
		lut[i].i = round(cos(d * i) * level * INT16_MAX);
		lut[i].q = round(sin(d * i) * level * INT16_MAX);
	}
	
	return(lut);
}

double rc_window(double t, double left, double width, double rise)
{
	double r;
	
	t -= left + width / 2;
	t = fabs(t) - (width - rise) / 2;
	
	if(t <= 0)
	{
		r = 1.0;
	}
	else if(t < rise)
	{
		r = 0.5 + cos(t / rise * M_PI) / 2;
	}
	else
	{
		r = 0.0;
	}
	
	return(r);
}

