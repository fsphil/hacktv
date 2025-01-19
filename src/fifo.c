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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "fifo.h"

int fifo_init(fifo_t *fifo, size_t count, size_t length)
{
	int i;
	
	/* There must be at least 3 blocks of 1 byte */
	if(count < 3) return(-1);
	if(length < 1) return(-1);
	
	fifo->count = count;
	fifo->blocks = calloc(sizeof(fifo_block_t), count);
	if(!fifo->blocks)
	{
		return(-1);
	}
	
	fifo->blocks->data = calloc(length, count);
	if(!fifo->blocks->data)
	{
		free(fifo->blocks);
		return(-1);
	}
	
	for(i = 0; i < count; i++)
	{
		pthread_mutex_init(&fifo->blocks[i].mutex, NULL);
		pthread_cond_init(&fifo->blocks[i].cond, NULL);
		fifo->blocks[i].readers = 0;
		fifo->blocks[i].writing = 1;
		fifo->blocks[i].data = (uint8_t *) fifo->blocks->data + (length * i);
		fifo->blocks[i].length = length;
		fifo->blocks[i].prev = &fifo->blocks[(i + count - 1) % count];
		fifo->blocks[i].next = &fifo->blocks[(i + 1) % count];
	}
	
	/* The writer starts on the first block */
	fifo->block = fifo->blocks;
	fifo->block->writing = 1;
	fifo->offset = 0;
	
	return(0);
}

void fifo_reader_init(fifo_reader_t *reader, fifo_t *fifo, int prefill)
{
	/* Readers start on the last (empty) block, waiting for the writer */
	reader->block = fifo->block->prev;
	reader->block->readers++;
	reader->offset = reader->block->length;
	reader->eof = 0;
	reader->prefill = NULL;
	
	if(prefill != 0)
	{
		/* The prefill block cannot be either of the last two blocks */
		if(prefill < 0 || prefill > fifo->count - 2) prefill = fifo->count - 2;
		
		reader->prefill = &fifo->blocks[prefill - 1];
	}
}

void fifo_reader_close(fifo_reader_t *reader)
{
	fifo_block_t *block = reader->block;
	
	if(reader->block != NULL && reader->eof == 0)
	{
		pthread_mutex_lock(&block->mutex);
		block->readers--;
		pthread_cond_signal(&block->cond);
		pthread_mutex_unlock(&block->mutex);
		
		reader->block = NULL;
		reader->eof = 1;
	}
}

void fifo_close(fifo_t *fifo)
{
	fifo_block_t *block = fifo->block;
	
	if(block == NULL) return;
	
	block->length = fifo->offset;
	
	if(block->length > 0)
	{
		fifo_block_t *next = block->next;
		
		pthread_mutex_lock(&next->mutex);
		
		/* Wait for the next block to be read */
		while(next->readers > 0)
		{
			pthread_cond_wait(&next->cond, &next->mutex);
		}
		
		next->writing = 0;
		next->length = 0;
		
		pthread_mutex_unlock(&next->mutex);
	}
	
	/* Mark current block as ready */
	pthread_mutex_lock(&block->mutex);
	block->writing = 0;
	pthread_cond_signal(&block->cond);
	pthread_mutex_unlock(&block->mutex);
	
	fifo->block = (block->length == 0 ? block : block->next);
	fifo->offset = 0;
}

void fifo_free(fifo_t *fifo)
{
	fifo_block_t *block;
	
	if(fifo->block == NULL) return;
	
	/* Send out EOF signal */
	fifo_close(fifo);
	
	block = fifo->block->next;
	
	/* TODO: Wait for all readers to end */
	while(block->length > 0)
	{
		pthread_mutex_lock(&block->mutex);
		
		while(block->readers > 0)
		{
			pthread_cond_wait(&block->cond, &block->mutex);
		}
		
		block->writing = 0;
		block->length = 0;
		
		pthread_mutex_unlock(&block->mutex);
		
		block = block->next;
	}
	
	/* Tear down the FIFO */
	for(int i = 0; i < fifo->count; i++)
	{
		pthread_cond_destroy(&fifo->blocks[i].cond);
		pthread_mutex_destroy(&fifo->blocks[i].mutex);
	}
	
	free(fifo->blocks->data);
	free(fifo->blocks);
	
	fifo->block = NULL;
}

size_t fifo_read(fifo_reader_t *reader, void **ptr, size_t length, int wait)
{
	fifo_block_t *block = reader->block;
	
	if(block == NULL || reader->eof)
	{
		/* End of line */
		return(-1);
	}
	
	if(reader->prefill)
	{
		pthread_mutex_lock(&reader->prefill->mutex);
		
		if(wait)
		{
			/* Wait until the next block is written to */
			while(reader->prefill->writing == 1 && reader->prefill->length != 0)
			{
				pthread_cond_wait(&reader->prefill->cond, &reader->prefill->mutex);
			}
		}
		else if(reader->prefill->writing == 1 && reader->prefill->length != 0)
		{
			/* Non-blocking */
			pthread_mutex_unlock(&reader->prefill->mutex);
			return(0);
		}
		
		pthread_mutex_unlock(&reader->prefill->mutex);
		
		reader->prefill = NULL;
	}
	
	if(reader->offset == block->length)
	{
		fifo_block_t *next = block->next;
		
		pthread_mutex_lock(&next->mutex);
		
		if(wait)
		{
			/* Wait until the next block is written to */
			while(next->writing == 1 && next->length != 0)
			{
				pthread_cond_wait(&next->cond, &next->mutex);
			}
		}
		else if(next->writing == 1 && next->length != 0)
		{
			/* Non-blocking */
			pthread_mutex_unlock(&next->mutex);
			return(0);
		}
		
		if(next->length == 0)
		{
			/* End of stream */
			reader->eof = 1;
		}
		else
		{
			next->readers++;
		}
		
		pthread_mutex_unlock(&next->mutex);
		
		pthread_mutex_lock(&block->mutex);
		block->readers--;
		pthread_cond_signal(&block->cond);
		pthread_mutex_unlock(&block->mutex);
		
		/* Move to the next block */
		reader->block = block = next;
		reader->offset = 0;
		
		if(reader->eof)
		{
			return(-1);
		}
	}
	
	/* Limit reads to the current block */
	if(length > block->length - reader->offset)
	{
		length = block->length - reader->offset;
	}
	
	*ptr = (uint8_t *) block->data + reader->offset;
	reader->offset += length;
	
	return(length);
}

size_t fifo_write_ptr(fifo_t *fifo, void **ptr, int wait)
{
	fifo_block_t *block = fifo->block;
	
	if(block == NULL || block->length == 0) return(-1);
	
	if(fifo->offset == block->length)
	{
		fifo_block_t *next = block->next;
		
		pthread_mutex_lock(&next->mutex);
		
		if(wait)
		{
			/* Wait for the next block to be read */
			while(next->readers > 0)
			{
				pthread_cond_wait(&next->cond, &next->mutex);
			}
		}
		else if(next->readers > 0)
		{
			pthread_mutex_unlock(&next->mutex);
			return(0);
		}
		
		next->writing = 1;
		
		pthread_mutex_unlock(&next->mutex);
		
		/* Mark current block as ready */
		pthread_mutex_lock(&block->mutex);
		block->writing = 0;
		pthread_cond_signal(&block->cond);
		pthread_mutex_unlock(&block->mutex);
		
		fifo->block = block = next;
		fifo->offset = 0;
	}
	
	*ptr = (uint8_t *) block->data + fifo->offset;
	
	return(block->length - fifo->offset);
}

void fifo_write(fifo_t *fifo, size_t length)
{
	fifo->offset += length;
}

