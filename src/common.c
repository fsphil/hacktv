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

static r64_t _normalise(int64_t *num, int64_t *den)
{
	int64_t e;
	
	if(*den == 0)
	{
		*num = 0;
		return((r64_t) { });
	}
	
	if(*den < 0)
	{
		*num = -*num;
		*den = -*den;
	}
	
	e = gcd(*num, *den);
	
	return((r64_t) { *num /= e, *den /= e });
}

r64_t r64_mul(r64_t a, r64_t b)
{
	int64_t c, d;
	c = (int64_t) a.num * b.num;
	d = (int64_t) a.den * b.den;
	return(_normalise(&c, &d));
}

r64_t r64_div(r64_t a, r64_t b)
{
	int64_t c, d;
	c = (int64_t) a.num * b.den;
	d = (int64_t) a.den * b.num;
	return(_normalise(&c, &d));
}

int r64_cmp(r64_t a, r64_t b)
{
	int64_t c = (int64_t) a.num * b.den - (int64_t) a.den * b.num;
	return(c < 0 ? -1 : (c > 0 ? 1 : 0));
}

r64_t r64_nearest(r64_t ref, r64_t a, r64_t b)
{
	/* Return "a" or "b" depending on which is nearest "ref", or "a" if equal */
	r64_t h = { a.num * b.den + a.den * b.num, a.den * b.den * 2 };
	return(r64_cmp(ref, h) <= 0 ? a : b);
}

r64_t r64_parse_decimal(const char *str, const char **endptr)
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
		return((r64_t) { });
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
		
		if(!isdigit(*s)) return((r64_t) { });
		
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
		return((r64_t) { });
	}
	
	/* Looks good, return the result */
	if(endptr != NULL) *endptr = s;
	return(_normalise(&num, &den));
}

r64_t r64_parse(const char *str, const char **endptr)
{
	/* Parse decimal number with exponent,
	 * individually or as a ratio pair x:y or x/y */
	const char *s;
	r64_t a, b;
	
	if(endptr != NULL) *endptr = str;
	
	/* Parse the first part */
	a = r64_parse_decimal(str, &s);
	if(a.den == 0) return(a);
	
	if(*s == ':' || *s == '/')
	{
		/* Don't allow spaces after the divider */
		s++;
		if(*s == ' ') return((r64_t) { });
		
		/* Parse the second part */
		b = r64_parse_decimal(s, &str);
		if(b.num == 0 || b.den == 0)
		{
			return((r64_t) { });
		}
		
		/* Test for too many dividers */
		s = str;
		if(*s == ':' || *s == '/')
		{
			return((r64_t) { });
		}
		
		/* Apply divider */
		a = r64_div(a, b);
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
		/* Raised cosine edge */
		//r = 0.5 + cos(t / rise * M_PI) / 2;
		
		/* Integrated raised cosine edge */
		t = 1.0 - t / rise * 2;
		r = 0.5 * (1.0 + t + sin(M_PI * t) / M_PI);
	}
	else
	{
		r = 0.0;
	}
	
	return(r);
}

double rrc(double x, double b, double t)
{
	double r;
	
	/* Based on the Wikipedia page, https://en.wikipedia.org/w/index.php?title=Root-raised-cosine_filter&oldid=787851747 */
	
	if(x == 0)
	{
		r = (1.0 / t) * (1.0 + b * (4.0 / M_PI - 1));
	}
	else if(fabs(x) == t / (4.0 * b))
	{
		r = b / (t * sqrt(2.0)) * ((1.0 + 2.0 / M_PI) * sin(M_PI / (4.0 * b)) + (1.0 - 2.0 / M_PI) * cos(M_PI / (4.0 * b)));
	}
	else
	{
		double t1 = (4.0 * b * (x / t));
		double t2 = (sin(M_PI * (x / t) * (1.0 - b)) + 4.0 * b * (x / t) * cos(M_PI * (x / t) * (1.0 + b)));
		double t3 = (M_PI * (x / t) * (1.0 - t1 * t1));
		
		r = (1.0 / t) * (t2 / t3);
	}
	
	return(r);
}

