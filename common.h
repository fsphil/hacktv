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

#ifndef _COMMON_H
#define _COMMON_H

#include <stdint.h>

typedef struct {
	int16_t i;
	int16_t q;
} cint16_t;

typedef struct {
	int32_t i;
	int32_t q;
} cint32_t;

static inline void cint16_mul(cint16_t *r, const cint16_t *a, const cint16_t *b)
{
	int32_t i, q;
	
	i = (int32_t) a->i * (int32_t) b->i - (int32_t) a->q * (int32_t) b->q;
	q = (int32_t) a->i * (int32_t) b->q + (int32_t) a->q * (int32_t) b->i;
	
	r->i = i >> 15;
	r->q = q >> 15;
}

static inline void cint16_mula(cint16_t *r, const cint16_t *a, const cint16_t *b)
{
	int32_t i, q;
	
	i = (int32_t) a->i * (int32_t) b->i - (int32_t) a->q * (int32_t) b->q;
	q = (int32_t) a->i * (int32_t) b->q + (int32_t) a->q * (int32_t) b->i;
	
	r->i += i >> 15;
	r->q += q >> 15;
}

static inline void cint32_mul(cint32_t *r, const cint32_t *a, const cint32_t *b)
{
	int64_t i, q;
	
	i = (int64_t) a->i * (int64_t) b->i - (int64_t) a->q * (int64_t) b->q;
	q = (int64_t) a->i * (int64_t) b->q + (int64_t) a->q * (int64_t) b->i;
	
	r->i = i >> 31;
	r->q = q >> 31;
}

static inline void cint32_mula(cint32_t *r, const cint32_t *a, const cint32_t *b)
{
	int64_t i, q;
	
	i = (int64_t) a->i * (int64_t) b->i - (int64_t) a->q * (int64_t) b->q;
	q = (int64_t) a->i * (int64_t) b->q + (int64_t) a->q * (int64_t) b->i;
	
	r->i += i >> 31;
	r->q += q >> 31;
}

extern int gcd(int a, int b);

extern cint16_t *sin_cint16(unsigned int length, unsigned int cycles, double level);

#endif

