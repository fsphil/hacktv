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
#include <unistd.h> /* For _POSIX_BARRIERS */

/* These factors where calculated with: f = M_PI / 2.0 / asin(0.9 - 0.1); */
#define RT1090 1.6939549523182869 /* Factor to convert 10-90% rise time to 0-100% */
#define RT2080 2.4410157268268087 /* Factor to convert 20-80% rise time to 0-100% */

/* As above but for integrated raised cosine edges */
#define IRT1090 2.0738786 /* 10-90% to 0-100% */
#define IRT2080 3.0546756 /* 10-90% to 0-100% */

typedef struct {
	int64_t num;
	int64_t den;
} r64_t;

typedef struct {
	int16_t i;
	int16_t q;
} cint16_t;

typedef struct {
	int32_t i;
	int32_t q;
} cint32_t;

extern int64_t gcd(int64_t a, int64_t b);
extern r64_t r64_mul(r64_t a, r64_t b);
extern r64_t r64_div(r64_t a, r64_t b);
extern int r64_cmp(r64_t a, r64_t b);
extern r64_t r64_nearest(r64_t ref, r64_t a, r64_t b);
extern r64_t r64_parse_decimal(const char *str, const char **endptr);
extern r64_t r64_parse(const char *str, const char **endptr);
extern cint16_t *sin_cint16(unsigned int length, unsigned int cycles, double level);
extern double rc_window(double t, double left, double width, double rise);
extern double rrc(double x, double b, double t);

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

#if !defined(_POSIX_BARRIERS) || _POSIX_BARRIERS <= 0

/* For systems that don't include a native POSIX Barriers implementation
 * Looking at you Apple....
*/

#include <pthread.h>

typedef struct {
	
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int count;
	int pending;
	int cycle;
	
} pthread_barrier_t;

#define PTHREAD_BARRIER_SERIAL_THREAD -1

extern int pthread_barrier_destroy(pthread_barrier_t *barrier);
extern int pthread_barrier_init(pthread_barrier_t *restrict barrier, void *attr, unsigned count);
extern int pthread_barrier_wait(pthread_barrier_t *barrier);

#endif

#endif

