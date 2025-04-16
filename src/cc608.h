/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2025 Philip Heron <phil@sanslogic.co.uk>                    */
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

#ifndef _CC608_H
#define _CC608_H

#include <stdint.h>
#include "video.h"



/* CC608 FIFO functions */

typedef struct {
	
	uint8_t *fifo;
	int size;
	int len;
	int ptr_in;
	int ptr_out;
	
	pthread_mutex_t mutex;
	
} cc608_fifo_t;

/* cc608_fifo_init(): Returns 0 if successful, -1 if out of memory */

extern int cc608_fifo_init(cc608_fifo_t *fifo);

/* cc608_fifo_free(): Frees FIFO memory */

extern void cc608_fifo_free(cc608_fifo_t *fifo);

/* cc608_fifo_write(): Write up to "len" bytes from "data" into the FIFO.
 *                     Returns the number of bytes written, always an even
 *                     number */

extern int cc608_fifo_write(cc608_fifo_t *fifo, uint8_t *data, int len);

/* cc608_fifo_read(): Read up to "len" bytes from the FIFO into "data".
 *                    Returns the number of bytes read, always an even number */

extern int cc608_fifo_read(cc608_fifo_t *fifo, uint8_t *data, int len);



/* CC608 render functions */

typedef struct {
	
	/* Config */
	int lines[2];
	
	/* Clock run-in signal */
	int cri_x;
	int cri_len;
	int16_t *cri;
	
	/* VBI renderer lookup */
	vbidata_lut_t *lut;
	
	/* FIFO */
	cc608_fifo_t ccfifo;
	
} cc608_t;

extern int cc608_init(cc608_t *s, vid_t *vid);
extern void cc608_free(cc608_t *s);

extern int cc608_render(vid_t *s, void *arg, int nlines, vid_line_t **lines);



#endif

