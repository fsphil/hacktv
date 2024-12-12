/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2017 Philip Heron <phil@sanslogic.co.uk>                    */
/* Copyright 2024 Matthew Millman                                        */
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
#include <stdlib.h>
#include <string.h>
#include <libhackrf/hackrf.h>
#include <pthread.h>
#include <unistd.h>
#include "rf.h"

/* Value from host/libhackrf/src/hackrf.c */
#define TRANSFER_BUFFER_SIZE 262144

/* HackDAC */
#define HACKDAC_FIRMWARE_SUFFIX "hackdac"
#define HACKDAC_MODE_RF          0
#define HACKDAC_MODE_BASEBAND    (1 << 7)
#define HACKDAC_AUDIO_MODE_SHIFT 1
#define HACKDAC_AUDIO_MODE_MASK  (0x3 << HACKDAC_AUDIO_MODE_SHIFT)
#define HACKDAC_AUDIO_MODE(x)    ((x) << HACKDAC_AUDIO_MODE_SHIFT)
#define HACKDAC_NO_AUDIO         0 // No audio. Don't send any audio data to HackRF by any means.
#define HACKDAC_SYNC_AUDIO       1 // Audio interleaved with video data
#define HACKDAC_ASYNC_AUDIO      2 // Audio transferred separately through USB_AUDIO_OUT_EP

#define HACKDAC_USB_AUDIO_BUFFER_SIZE 512 // (bytes), 128 L+R 16-bit samples
#define HACKDAC_SYNC_MAGIC_1          0x87654321
#define HACKDAC_SYNC_MAGIC_2          0x12345678
#define HACKRF_AHB_BUFFER_SIZE        16384 // Not HackDAC specific but not exposed in hackrf.h either
#define HACKDAC_PHASE_SIZE            (HACKRF_AHB_BUFFER_SIZE + HACKDAC_USB_AUDIO_BUFFER_SIZE)

/* hackrf_set_hw_sync_mode is 'borrowed' to configure hackdac for now */
#define hackrf_set_hackdac_mode(dev, value) hackrf_set_hw_sync_mode(dev, value)

typedef enum {
	BUFFER_EMPTY,
	BUFFER_PREFILL,
	BUFFER_FILL,
	BUFFER_READY,
} buffer_status_t;

typedef struct {
	
	/* Buffers are locked while reading/writing */
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	buffer_status_t status;
	
	/* Pointer to the start of the buffer */
	int8_t *data;
	
	/* Offset to start of data */
	size_t start;
	
	/* Length of data ready */
	size_t length;
	
} buffer_t;

typedef struct {
	
	buffer_t *buffers;
	
	int length;
	int count;
	int prefill;
	int in;
	int out;
	
} buffers_t;

typedef struct {
	
	/* HackRF device */
	hackrf_device *d;
	
	/* HackDAC device */
	int hackdac_firmware_version; /* 0 if not present */
	int hackdac_sync_frame_sent;
	int hackdac_frame_phase;
	int hackdac_frame_padding;
	
	/* Buffers */
	buffers_t buffers;
	
} hackrf_t;

static int _buffer_init(buffers_t *buffers, size_t count, size_t length)
{
	int i;
	
	buffers->count = count;
	buffers->length = length;
	buffers->buffers = calloc(count, sizeof(buffer_t));
	
	for(i = 0; i < count; i++)
	{
		pthread_mutex_init(&buffers->buffers[i].mutex, NULL);
		pthread_cond_init(&buffers->buffers[i].cond, NULL);
		buffers->buffers[i].data = malloc(length);
		buffers->buffers[i].start = buffers->length;
		buffers->buffers[i].length = buffers->length;
		buffers->buffers[i].status = BUFFER_EMPTY;
	}
	
	buffers->prefill = 1;
	buffers->in = 0;
	buffers->out = 0;
	
	return(0);
}

static int _buffer_free(buffers_t *buffers)
{
	int i;
	
	for(i = 0; i < buffers->count; i++)
	{
		free(buffers->buffers[i].data);
		pthread_cond_destroy(&buffers->buffers[i].cond);
		pthread_mutex_destroy(&buffers->buffers[i].mutex);
	}
	
	free(buffers->buffers);
	memset(buffers, 0, sizeof(buffers_t));
	
	return(0);
}

static int _buffer_read(buffers_t *buffers, int8_t *dst, size_t length)
{
	buffer_t *buf = &buffers->buffers[buffers->out];
	
	if(buf->start == buffers->length)
	{
		buffer_status_t r;
		
		/* Check if we can read this block */
		pthread_mutex_lock(&buf->mutex);
		r = buf->status;
		pthread_mutex_unlock(&buf->mutex);
		
		if(r != BUFFER_READY)
		{
			/* This buffer is not ready - display warning if not in prefill stage */
			if(r != BUFFER_PREFILL)
			{
				fprintf(stderr, "U");
			}
			
			return(0);
		}
		
		buf->start = 0;
	}
	
	if(length > buffers->length - buf->start)
	{
		length = buffers->length - buf->start;
	}
	
	memcpy(dst, buf->data + buf->start, length);
	buf->start += length;
	
	if(buf->start == buffers->length)
	{
		/* Flag the current block as avaliable for writing */
		pthread_mutex_lock(&buf->mutex);
		buf->status = BUFFER_EMPTY;
		pthread_mutex_unlock(&buf->mutex);
		pthread_cond_broadcast(&buf->cond);
		
		buffers->out = (buffers->out + 1) % buffers->count;
	}
	
	return(length);
}

static size_t _buffer_write_ptr(buffers_t *buffers, int8_t **src)
{
	buffer_t *buf = &buffers->buffers[buffers->in];
	
	if(buf->length == buffers->length)
	{
		pthread_mutex_lock(&buf->mutex);
		
		if(buf->status == BUFFER_PREFILL)
		{
			buffers->prefill = 0;
			buf->status = BUFFER_READY;
		}
		
		while(buf->status != BUFFER_EMPTY)
		{
			pthread_cond_wait(&buf->cond, &buf->mutex);
		}
		
		pthread_mutex_unlock(&buf->mutex);
		
		buf->length = 0;
	}
	
	*src = buf->data + buf->length;
	
	return(buffers->length - buf->length);
}

static int _buffer_write(buffers_t *buffers, size_t length)
{
	buffer_t *buf = &buffers->buffers[buffers->in];
	
	buf->length += length;
	
	if(buf->length == buffers->length)
	{
		pthread_mutex_lock(&buf->mutex);
		buf->status = (buffers->prefill ? BUFFER_PREFILL : BUFFER_READY);
		pthread_mutex_unlock(&buf->mutex);
		
		buffers->in = (buffers->in + 1) % buffers->count;
	}
	
	return(length);
}

static int _tx_callback(hackrf_transfer *transfer)
{
	hackrf_t *rf = transfer->tx_ctx;
	size_t l = transfer->valid_length;
	uint8_t *buf = transfer->buffer;
	int r;
	
	while(l)
	{
		r = _buffer_read(&rf->buffers, (int8_t *) buf, l);
		
		if(r == 0)
		{
			/* Buffer underrun, fill with zero */
			memset(buf, 0, l);
			l = 0;
		}
		else
		{
			l -= r;
			buf += r;
		}
	}
	
	return(0);
}

static int _tx_callback_hackdac(hackrf_transfer *transfer)
{
	hackrf_t *rf = transfer->tx_ctx;
	size_t l = transfer->valid_length;
	uint8_t *buf = transfer->buffer;
	int r;
	
	if(rf->hackdac_sync_frame_sent == 0)
	{
		/* The first frame sent is the sync frame, which
		 * marks where interleaved a/v data begins */
		
		memset(buf, 0, HACKDAC_USB_AUDIO_BUFFER_SIZE);
		
		((uint32_t *) buf)[0] = HACKDAC_SYNC_MAGIC_1; // Signature
		((uint32_t *) buf)[1] = HACKDAC_SYNC_MAGIC_2; // Signature
		((uint32_t *) buf)[2] = HACKDAC_USB_AUDIO_BUFFER_SIZE; // Length of frame
		
		l -= HACKDAC_USB_AUDIO_BUFFER_SIZE;
		buf += HACKDAC_USB_AUDIO_BUFFER_SIZE;
		
		rf->hackdac_sync_frame_sent = 1;
	}
	
	while(l)
	{
		if(rf->hackdac_frame_padding > 0)
		{
			/* Underrun padding */
			r = rf->hackdac_frame_padding;
			if(r > l) r = l;
			
			memset(buf, 0, r);
			
			rf->hackdac_frame_padding -= r;
		}
		else if(rf->hackdac_frame_phase < HACKRF_AHB_BUFFER_SIZE)
		{
			/* Video phase */
			r = HACKRF_AHB_BUFFER_SIZE - rf->hackdac_frame_phase;
			if(r > l) r = l;
			
			r = _buffer_read(&rf->buffers, (int8_t *) buf, r);
			
			rf->hackdac_frame_phase += r;
		}
		else
		{
			/* Audio phase */
			r = HACKDAC_PHASE_SIZE - rf->hackdac_frame_phase;
			if(r > l) r = l;
			
			/* TODO: Read audio data */
			memset(buf, 0, r);
			
			rf->hackdac_frame_phase += r;
		}
		
		if(rf->hackdac_frame_phase == HACKDAC_PHASE_SIZE)
		{
			rf->hackdac_frame_phase = 0;
		}
		
		if(r == 0)
		{
			/* Buffer underrun, pad remaining transfer buffer
			 * with zeros - rounding up to the AV phase size */
			rf->hackdac_frame_padding = (l + HACKDAC_PHASE_SIZE - 1) / HACKDAC_PHASE_SIZE;
			rf->hackdac_frame_padding *= HACKDAC_PHASE_SIZE;
		}
		
		l -= r;
		buf += r;
	}
	
	return(0);
}

static int _rf_write(void *private, const int16_t *iq_data, size_t samples)
{
	hackrf_t *rf = private;
	int8_t *iq8 = NULL;
	int i, r;
	
	samples *= 2;
	
	while(samples > 0)
	{
		r = _buffer_write_ptr(&rf->buffers, &iq8);
		
		for(i = 0; i < r && i < samples; i++)
		{
			iq8[i] = iq_data[i] >> 8;
		}
		
		_buffer_write(&rf->buffers, i);
		
		iq_data += i;
		samples -= i;
	}
	
	return(RF_OK);
}

static int _rf_write_baseband(void *private, const int16_t *iq_data, size_t samples)
{
	hackrf_t *rf = private;
	int8_t *iq8 = NULL;
	int i, r;
	
	samples *= 2;
	
	while(samples > 0)
	{
		r = _buffer_write_ptr(&rf->buffers, &iq8);
		
		for(i = 0; i < r && i < samples; i += 2)
		{
			int sync = (iq_data[i] > -9000);
			iq8[i + 0] = (iq_data[i] >> 1) & 0xFF;
			iq8[i + 1] = ((iq_data[i] >> 9) & 0x7F) | (sync << 7);
		}
		
		_buffer_write(&rf->buffers, i);
		
		iq_data += i;
		samples -= i;
	}
	
	return(RF_OK);
}

static int _rf_close(void *private)
{
	hackrf_t *rf = private;
	int r;
	
	r = hackrf_stop_tx(rf->d);
	if(r != HACKRF_SUCCESS)
	{
		fprintf(stderr, "hackrf_stop_tx() failed: %s (%d)\n", hackrf_error_name(r), r);
		return(RF_ERROR);
	}
	
	/* Wait until streaming has stopped */
	while(hackrf_is_streaming(rf->d) == HACKRF_TRUE)
	{
		usleep(100);
	}
	
	r = hackrf_close(rf->d);
	if(r != HACKRF_SUCCESS)
	{
		fprintf(stderr, "hackrf_close() failed: %s (%d)\n", hackrf_error_name(r), r);
	}
	
	hackrf_exit();
	
	_buffer_free(&rf->buffers);
	free(rf);
	
	return(RF_OK);
}

int rf_hackrf_open(rf_t *s, const char *serial, uint32_t sample_rate, uint64_t frequency_hz, unsigned int txvga_gain, unsigned char amp_enable, unsigned char baseband)
{
	hackrf_t *rf;
	int r;
	uint8_t rev;
	char str[256];
	
	rf = calloc(1, sizeof(hackrf_t));
	if(!rf)
	{
		return(RF_OUT_OF_MEMORY);
	}
	
	/* Print the library version number */
	fprintf(stderr, "libhackrf version: %s (%s)\n",
		hackrf_library_release(),
		hackrf_library_version());
	
	/* Prepare the HackRF for output */
	r = hackrf_init();
	if(r != HACKRF_SUCCESS)
	{
		fprintf(stderr, "hackrf_init() failed: %s (%d)\n", hackrf_error_name(r), r);
		free(rf);
		return(RF_ERROR);
	}
	
	r = hackrf_open_by_serial(serial, &rf->d);
	if(r != HACKRF_SUCCESS)
	{
		fprintf(stderr, "hackrf_open() failed: %s (%d)\n", hackrf_error_name(r), r);
		free(rf);
		return(RF_ERROR);
	}
	
	/* Print the hardware revision */
	r = hackrf_board_rev_read(rf->d, &rev);
	if(r == HACKRF_SUCCESS)
	{
		fprintf(stderr, "hackrf: Hardware Revision: %s\n", hackrf_board_rev_name(rev));
	}
	
	/* Print the firmware version */
	r = hackrf_version_string_read(rf->d, str, sizeof(str) - 1);
	if(r == HACKRF_SUCCESS)
	{
		char *pstr;
		char hackdac_hardware_type;
		
		fprintf(stderr, "hackrf: Firmware Version: %s\n", str);
		
		/* Test for the hackdac firmware */
		pstr = strstr(str, HACKDAC_FIRMWARE_SUFFIX);
		if(pstr && sscanf(pstr, HACKDAC_FIRMWARE_SUFFIX "-%c-%d", &hackdac_hardware_type, &rf->hackdac_firmware_version) == 2)
		{
			fprintf(stderr, "hackrf: HackDAC Type: %c/%u\n", hackdac_hardware_type + ('A' - 'a'), rf->hackdac_firmware_version);
		}
	}
	
	/* Override RF settings for baseband mode */
	if(baseband)
	{
		if(rf->hackdac_firmware_version == 0)
		{
			fprintf(stderr, "HackDAC required for baseband operation\n");
			free(rf);
			return(RF_ERROR);
		}
		
		frequency_hz = 0;
		txvga_gain = 0;
		amp_enable = 0;
	}
	
	if(rf->hackdac_firmware_version > 0)
	{
		uint8_t flags = 0;
		
		/* Configure HackDAC mode */
		if(baseband)
		{
			flags |= HACKDAC_MODE_BASEBAND;
			flags |= HACKDAC_AUDIO_MODE(HACKDAC_SYNC_AUDIO);
		}
		else
		{
			flags |= HACKDAC_MODE_RF;
			flags |= HACKDAC_AUDIO_MODE(HACKDAC_NO_AUDIO);
		}
		
		r = hackrf_set_hackdac_mode(rf->d, flags);
		if(r != HACKRF_SUCCESS)
		{
			fprintf(stderr, "hackrf_set_hackdac_mode() failed: %s (%d)\n", hackrf_error_name(r), r);
			free(rf);
			return(RF_ERROR);
		}
	}
	
	r = hackrf_set_sample_rate_manual(rf->d, sample_rate, 1);
	if(r != HACKRF_SUCCESS)
	{
		fprintf(stderr, "hackrf_sample_rate_set() failed: %s (%d)\n", hackrf_error_name(r), r);
		free(rf);
		return(RF_ERROR);
	}
	
	r = hackrf_set_baseband_filter_bandwidth(rf->d, hackrf_compute_baseband_filter_bw(sample_rate));
	if(r != HACKRF_SUCCESS)
	{
		fprintf(stderr, "hackrf_baseband_filter_bandwidth_set() failed: %s (%d)\n", hackrf_error_name(r), r);
		free(rf);
		return(RF_ERROR);
	}
	
	r = hackrf_set_freq(rf->d, frequency_hz);
	if(r != HACKRF_SUCCESS)
	{
		fprintf(stderr, "hackrf_set_freq() failed: %s (%d)\n", hackrf_error_name(r), r);
		free(rf);
		return(RF_ERROR);
	}
	
	r = hackrf_set_txvga_gain(rf->d, txvga_gain);
	if(r != HACKRF_SUCCESS)
	{
		fprintf(stderr, "hackrf_set_txvga_gain() failed: %s (%d)\n", hackrf_error_name(r), r);
		free(rf);
		return(RF_ERROR);
	}
	
	r = hackrf_set_amp_enable(rf->d, amp_enable);
	if(r != HACKRF_SUCCESS)
	{
		fprintf(stderr, "hackrf_set_amp_enable() failed: %s (%d)\n", hackrf_error_name(r), r);
		free(rf);
		return(RF_ERROR);
	}
	
	/* Allocate memory for the output buffers, enough for at least 400ms - minimum 4 */
	r = sample_rate * 2 * 4 / 10 / TRANSFER_BUFFER_SIZE;
	if(r < 4) r = 4;
	_buffer_init(&rf->buffers, r, TRANSFER_BUFFER_SIZE);
	
	/* Begin transmitting */
	r = hackrf_start_tx(rf->d, baseband ? _tx_callback_hackdac : _tx_callback, rf);
	if(r != HACKRF_SUCCESS)
	{
		fprintf(stderr, "hackrf_start_tx() failed: %s (%d)\n", hackrf_error_name(r), r);
		free(rf);
		return(RF_ERROR);
	}
	
	/* Register the callback functions */
	s->ctx = rf;
	s->write = baseband ? _rf_write_baseband : _rf_write;
	s->close = _rf_close;
	
	return(RF_OK);
}

