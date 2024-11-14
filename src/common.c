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
#include <ctype.h>
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

const rational_t _normalise(int64_t *num, int64_t *den)
{
	int64_t e;
	
	if(*den == 0)
	{
		*num = 0;
		return((rational_t) { });
	}
	
	if(*den < 0)
	{
		*num = -*num;
		*den = -*den;
	}
	
	e = gcd(*num, *den);
	
	return((rational_t) { *num /= e, *den /= e });
}

rational_t rational_mul(rational_t a, rational_t b)
{
	int64_t c, d;
	c = (int64_t) a.num * b.num;
	d = (int64_t) a.den * b.den;
	return(_normalise(&c, &d));
}

rational_t rational_div(rational_t a, rational_t b)
{
	int64_t c, d;
	c = (int64_t) a.num * b.den;
	d = (int64_t) a.den * b.num;
	return(_normalise(&c, &d));
}

int rational_cmp(rational_t a, rational_t b)
{
	int64_t c = (int64_t) a.num * b.den - (int64_t) a.den * b.num;
	return(c < 0 ? -1 : (c > 0 ? 1 : 0));
}

rational_t rational_nearest(rational_t ref, rational_t a, rational_t b)
{
	/* Return "a" or "b" depending on which is nearest "ref", or "a" if equal */
	rational_t h = { a.num * b.den + a.den * b.num, a.den * b.den * 2 };
	return(rational_cmp(ref, h) <= 0 ? a : b);
}

rational_t rational_parse_decimal(const char *str, const char **endptr)
{
	/* Parse decimal number with exponent */
	const char *s = str;
	int64_t e, num, den = 1;
	
	if(endptr != NULL) *endptr = str;
	
	/* Skip any leading spaces */
	while(isspace(*s)) s++;
	
	/* Test for the sign */
	if(*s == '+' || *s == '-')
	{
		if(*s == '-') den = -1;
		s++;
	}
	
	/* Test for no number */
	if((s[0] != '.' && !isdigit(s[0])) ||
	   (s[0] == '.' && !isdigit(s[1])))
	{
		return((rational_t) { });
	}
	
	/* Read first number/integer part */
	for(num = 0; isdigit(*s); s++)
	{
		num = num * 10 + *s - '0';
	}
	
	/* Read the fractional part */
	if(*s == '.')
	{
		for(s++; isdigit(*s); s++)
		{
			num = num * 10 + *s - '0';
			den *= 10;
		}
		
		(void) _normalise(&num, &den);
	}
	
	/* Read the exponent part */
	if(*s == 'e' || *s == 'E')
	{
		int neg = 0;
		
		s++;
		
		/* Test for the sign */
		if(*s == '+' || *s == '-')
		{
			if(*s == '-') neg = 1;
			s++;
		}
		
		if(!isdigit(*s)) return((rational_t) { });
		
		for(e = 0; isdigit(*s); s++)
		{
			e = e * 10 + *s - '0';
		}
		
		if(neg) { for(; e > 0; e--) den *= 10; }
		else    { for(; e > 0; e--) num *= 10; }
	}
	
	/* Test for empty string or invalid trailing characters */
	if(*s == '.' || *s == '+' || *s == '-' ||
	   *s == 'e' || *s == 'E' || s == str)
	{
		return((rational_t) { });
	}
	
	/* Looks good, return the result */
	if(endptr != NULL) *endptr = s;
	return(_normalise(&num, &den));
}

rational_t rational_parse(const char *str, const char **endptr)
{
	/* Parse decimal number with exponent,
	 * individually or as a ratio pair x:y or x/y */
	const char *s;
	rational_t a, b;
	
	if(endptr != NULL) *endptr = str;
	
	/* Parse the first part */
	a = rational_parse_decimal(str, &s);
	if(a.den == 0) return(a);
	
	if(*s == ':' || *s == '/')
	{
		/* Don't allow spaces after the divider */
		s++;
		if(*s == ' ') return((rational_t) { });
		
		/* Parse the second part */
		b = rational_parse_decimal(s, &str);
		if(b.num == 0 || b.den == 0)
		{
			return((rational_t) { });
		}
		
		/* Test for too many dividers */
		s = str;
		if(*s == ':' || *s == '/')
		{
			return((rational_t) { });
		}
		
		/* Apply divider */
		a = rational_div(a, b);
	}
	
	/* Looks good, return the result */
	if(endptr != NULL) *endptr = s;
	return(a);
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

