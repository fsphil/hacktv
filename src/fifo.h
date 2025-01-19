/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2024 Philip Heron <phil@sanslogic.co.uk>                    */
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

#ifndef _FIFO_H
#define _FIFO_H

/* Single writer / multi reader FIFO */

typedef struct _fifo_block_t {
	
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	
	int readers;
	int writing;
	
	void *data;
	size_t length;
	
	struct _fifo_block_t *prev, *next;
	
} fifo_block_t;

typedef struct {
	
	size_t count;
	fifo_block_t *blocks;
	
	fifo_block_t *block;
	size_t offset;
	
} fifo_t;

typedef struct {
	
	fifo_block_t *block;
	size_t offset;
	
	int eof;
	fifo_block_t *prefill;
	
} fifo_reader_t;

/* Initalise and allocate memory for a FIFO.
 *
 * fifo: Pointer to uninitalised FIFO
 * count: Number of blocks (min: 3)
 * length: Length of each block in bytes (min: 1)
 *
 * Returns 0 on success, or -1 if out of memory
*/
extern int fifo_init(fifo_t *fifo, size_t count, size_t length);

/* Mark the FIFO as closed. Readers can continue
 * reading any remaining data.
 *
 * fifo: Pointer to initalised FIFO
*/
extern void fifo_close(fifo_t *fifo);

/* Free a FIFO. Waits until all readers have
 * finished and releases memory.
 *
 * fifo: Pointer to initalised FIFO
*/
extern void fifo_free(fifo_t *fifo);

/* Request a pointer into the FIFO for writing.
 *
 * fifo: Pointer to initalised FIFO
 * ptr: Pointer to where the pointer is stored
 * wait: Set to 1 to wait on a free block
 *
 * Returns Number of bytes available in buffer,
 * 0 if no blocks are free and wait == 0, or
 * -1 if the FIFO is closed.
 *
 * ptr is only valid if the return is > 0
*/
extern size_t fifo_write_ptr(fifo_t *fifo, void **ptr, int wait);

/* Submit data written to the pointer returned
 * by fifo_write_ptr().
 *
 * fifo: Pointer to initalised FIFO
 * length: Number of bytes written
 *
 * length must not be greater than the
 * value returned by fifo_write_ptr().
*/
extern void fifo_write(fifo_t *fifo, size_t length);

/* Initalise a FIFO reader.
 *
 * reader: Pointer to an uninitalised FIFO reader
 * fifo: Pointer to an initalised FIFO
 * prefill: Number of blocks that must be written to
 *          before reading begins (max: num. blocks - 2),
 *          or -1 to automatically use the max value
 *
 * This must be called in the same thread that
 * called fifo_init(), and before any writes
 * have been made.
*/
extern void fifo_reader_init(fifo_reader_t *reader, fifo_t *fifo, int prefill);

/* Close a FIFO reader.
 *
 * reader: Pointer to an initalised FIFO reader
*/
extern void fifo_reader_close(fifo_reader_t *reader);

/* Read data from the FIFO
 *
 * reader: Pointer to an initalised FIFO reader
 * ptr: Pointer to where the pointer is stored
 * length: Maximum number of bytes to return
 * wait: Set to 1 to wait on data being available
 *
 * Returns number of bytes avaliable at ptr,
 * 0 if wait == 0 and no data was ready, or
 * -1 if the FIFO reader is closed.
*/
extern size_t fifo_read(fifo_reader_t *reader, void **ptr, size_t length, int wait);

#endif

