/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2023 Philip Heron <phil@sanslogic.co.uk>                    */
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

#ifndef _RF_H
#define _RF_H

/* Return codes */
#define RF_OK             0
#define RF_ERROR         -1
#define RF_OUT_OF_MEMORY -2

/* Signal types */
#define RF_INT16_COMPLEX 0
#define RF_INT16_REAL    1

/* File output types */
#define RF_UINT8  0
#define RF_INT8   1
#define RF_UINT16 2
#define RF_INT16  3
#define RF_INT32  4
#define RF_FLOAT  5 /* 32-bit float */

/* RF output function prototypes */
typedef int (*rf_write_t)(void *ctx, int16_t *iq_data, size_t samples);
typedef int (*rf_close_t)(void *ctx);

typedef struct {
	
	void *ctx;
	rf_write_t write;
	rf_close_t close;
	
} rf_t;

extern int rf_write(rf_t *s, int16_t *iq_data, size_t samples);
extern int rf_close(rf_t *s);

#include "rf_file.h"

#ifdef HAVE_HACKRF
#include "rf_hackrf.h"
#endif

#ifdef HAVE_SOAPYSDR
#include "rf_soapysdr.h"
#endif

#ifdef HAVE_FL2K
#include "rf_fl2k.h"
#endif

#endif

