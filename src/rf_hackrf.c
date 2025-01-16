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
#include "fifo.h"
#include "fir.h"

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

typedef struct {
	
	/* HackRF device */
	hackrf_device *d;
	uint32_t sample_rate;
	
	/* HackDAC device */
	int hackdac_firmware_version; /* 0 if not present */
	int hackdac_sync_frame_sent;
	int hackdac_frame_phase;
	int hackdac_frame_padding;
	fir_int16_t hackdac_audio_resampler[2];
	
	/* Buffers */
	fifo_t buffers;
	fifo_reader_t buffers_reader;
	
	fifo_t audio_buffers;
	fifo_reader_t audio_buffers_reader;
	
	/* Stats */
	uint32_t stats_counter;
	uint32_t num_shortfalls;
	
} hackrf_t;

static int _tx_callback(hackrf_transfer *transfer)
{
	hackrf_t *rf = transfer->tx_ctx;
	size_t l = transfer->valid_length;
	uint8_t *pbuf, *buf = transfer->buffer;
	int r;
	
	while(l)
	{
		r = fifo_read(&rf->buffers_reader, (void **) &pbuf, l, 0);
		
		if(r == 0)
		{
			/* Buffer underrun, fill with zero */
			if(rf->buffers_reader.prefill == NULL) fprintf(stderr, "U");
			
			memset(buf, 0, l);
			l = 0;
		}
		else if(r < 0)
		{
			/* EOF, stop transmitting */
			fifo_reader_close(&rf->buffers_reader);
			return(-1);
		}
		else
		{
			memcpy(buf, pbuf, r);
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
	uint8_t *pbuf, *buf = transfer->buffer;
	fifo_reader_t *reader = NULL;
	int r;
	
	if(rf->hackdac_sync_frame_sent < 3)
	{
		/* Send out three blank frames before anything else */
		memset(buf, 0, l);
		l -= l;
		buf += l;
		rf->hackdac_sync_frame_sent++;
	}
	else if(rf->hackdac_sync_frame_sent == 3)
	{
		/* Next send the sync frame, which marks where
		 * interleaved a/v data begins */
		
		memset(buf, 0, HACKDAC_USB_AUDIO_BUFFER_SIZE);
		
		((uint32_t *) buf)[0] = HACKDAC_SYNC_MAGIC_1; // Signature
		((uint32_t *) buf)[1] = HACKDAC_SYNC_MAGIC_2; // Signature
		((uint32_t *) buf)[2] = HACKDAC_USB_AUDIO_BUFFER_SIZE; // Length of frame
		
		l -= HACKDAC_USB_AUDIO_BUFFER_SIZE;
		buf += HACKDAC_USB_AUDIO_BUFFER_SIZE;
		
		rf->hackdac_sync_frame_sent++;
		rf->hackdac_frame_phase = 0;
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
			l -= r;
			buf += r;
			continue;
		}
		
		if(rf->hackdac_frame_phase < HACKRF_AHB_BUFFER_SIZE)
		{
			/* Video phase */
			reader = &rf->buffers_reader;
			r = HACKRF_AHB_BUFFER_SIZE - rf->hackdac_frame_phase;
		}
		else
		{
			/* Audio phase */
			reader = &rf->audio_buffers_reader;
			r = HACKDAC_PHASE_SIZE - rf->hackdac_frame_phase;
		}
		
		if(r > l) r = l;
		
		if(reader != NULL)
		{
			r = fifo_read(reader, (void **) &pbuf, r, 0);
			memcpy(buf, pbuf, r);
		}
		else
		{
			memset(buf, 0, r);
		}
		
		if(r > 0)
		{
			rf->hackdac_frame_phase += r;
			if(rf->hackdac_frame_phase == HACKDAC_PHASE_SIZE)
			{
				rf->hackdac_frame_phase = 0;
			}
			
			l -= r;
			buf += r;
		}
		else if(r == 0)
		{
			/* Buffer underrun, pad remaining transfer buffer
			 * with zeros - rounding up to the AV phase size */
			if(reader->prefill == NULL) fprintf(stderr, "U");
			rf->hackdac_frame_padding = (l + HACKDAC_PHASE_SIZE - 1) / HACKDAC_PHASE_SIZE;
			rf->hackdac_frame_padding *= HACKDAC_PHASE_SIZE;
		}
		else if(r < 0)
		{
			/* EOF, stop transmission */
			fifo_reader_close(&rf->buffers_reader);
			fifo_reader_close(&rf->audio_buffers_reader);
			return(-1);
		}
	}
	
	return(0);
}

static void _rf_write_print_stats(hackrf_t *rf, size_t samples)
{
	hackrf_m0_state state;
	int r;
	
	/* Only run this after at least 1 second of samples */
	rf->stats_counter += samples;
	if(rf->stats_counter < rf->sample_rate) return;
	
	rf->stats_counter -= rf->sample_rate;
	
	/* Fetch stats from the hackrf */
	r = hackrf_get_m0_state(rf->d, &state);
	
	if(r == HACKRF_SUCCESS && state.num_shortfalls != rf->num_shortfalls)
	{
		/* The number of shortfalls/underruns has changed, print a warning */
		fprintf(stderr, "hackrf: %u underrun%s, longest %u bytes\n",
			state.num_shortfalls, state.num_shortfalls != 1 ? "s" : "",
			state.longest_shortfall
		);
		
		rf->num_shortfalls = state.num_shortfalls;
	}
}

static int _rf_write(void *private, const int16_t *iq_data, size_t samples)
{
	hackrf_t *rf = private;
	int8_t *iq8 = NULL;
	int i, r;
	
	/* Report some stats every ~1 second */
	_rf_write_print_stats(rf, samples);
	
	r = 0;
	samples *= 2;
	
	while(samples > 0)
	{
		r = fifo_write_ptr(&rf->buffers, (void **) &iq8, 1);
		
		if(r < 0) break;
		
		for(i = 0; i < r && i < samples; i++)
		{
			iq8[i] = iq_data[i] >> 8;
		}
		
		fifo_write(&rf->buffers, i);
		
		iq_data += i;
		samples -= i;
	}
	
	return(r >= 0 ? RF_OK : RF_ERROR);
}

static int _rf_write_baseband(void *private, const int16_t *iq_data, size_t samples)
{
	hackrf_t *rf = private;
	int8_t *iq8 = NULL;
	int i, r = 0;
	
	/* Report some stats every ~1 second */
	_rf_write_print_stats(rf, samples);
	
	samples *= 2;
	
	while(samples > 0)
	{
		r = fifo_write_ptr(&rf->buffers, (void **) &iq8, 1);
		
		if(r < 0) break;
		
		for(i = 0; i < r && i < samples; i += 2)
		{
			int sync = (iq_data[i] > -9000);
			iq8[i + 0] = (iq_data[i] >> 1) & 0xFF;
			iq8[i + 1] = ((iq_data[i] >> 9) & 0x7F) | (sync << 7);
		}
		
		fifo_write(&rf->buffers, i);
		
		iq_data += i;
		samples -= i;
	}
	
	return(r >= 0 ? RF_OK : RF_ERROR);
}

static int _rf_write_baseband_audio(void *private, const int16_t *audio, size_t samples)
{
	hackrf_t *rf = private;
	
	if(audio == NULL) return(RF_OK);
	
	fir_int16_feed(&rf->hackdac_audio_resampler[0], audio + 0, samples / 2, 2);
	fir_int16_feed(&rf->hackdac_audio_resampler[1], audio + 1, samples / 2, 2);
	
	while(1)
	{
		int16_t *buf;
		size_t buf_len;
		size_t r;
		
		r = fifo_write_ptr(&rf->audio_buffers, (void **) &buf, 1);
		if(r < 0) break;
		
		buf_len  = fir_int16_process(&rf->hackdac_audio_resampler[0], buf + 0, r / sizeof(int16_t) / 2, 2);
		buf_len += fir_int16_process(&rf->hackdac_audio_resampler[1], buf + 1, r / sizeof(int16_t) / 2, 2);
		buf_len *= sizeof(int16_t);
		if(buf_len == 0) break;
		
		fifo_write(&rf->audio_buffers, buf_len);
	}
	
	return(RF_OK);
}

static int _rf_close(void *private)
{
	hackrf_t *rf = private;
	int r;
	
	fifo_close(&rf->buffers);
	if(rf->audio_buffers.count) fifo_close(&rf->audio_buffers);
	
	fifo_free(&rf->buffers);
	if(rf->audio_buffers.count) fifo_free(&rf->audio_buffers);
	
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
	
	fir_int16_free(&rf->hackdac_audio_resampler[0]);
	fir_int16_free(&rf->hackdac_audio_resampler[1]);
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
	
	rf->sample_rate = sample_rate;
	
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
		size_t len;
		
		if(rf->hackdac_firmware_version == 0)
		{
			fprintf(stderr, "HackDAC required for baseband operation\n");
			free(rf);
			return(RF_ERROR);
		}
		
		frequency_hz = 0;
		txvga_gain = 0;
		amp_enable = 0;
		
		/* Initalise the audio resamplers */
		/* TODO: Don't hardwire the input sample rate */
		for(int i = 0; i < 2; i++)
		{
			r = fir_int16_resampler_init(
				&rf->hackdac_audio_resampler[i],
				(r64_t) { rf->sample_rate, 64 },
				(r64_t) { 32000, 1 }
			);
		}
		
		/* Allocate memory for the output audio buffers,
		 * enough for at least 400ms (10ms blocks x40) */
		len = fir_int16_output_size(&rf->hackdac_audio_resampler[0], 320);
		fifo_init(&rf->audio_buffers, 40, len * 2 * sizeof(int16_t));
		fifo_reader_init(&rf->audio_buffers_reader, &rf->audio_buffers, 0);
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
	
	r = hackrf_set_sample_rate_manual(rf->d, rf->sample_rate, 1);
	if(r != HACKRF_SUCCESS)
	{
		fprintf(stderr, "hackrf_sample_rate_set() failed: %s (%d)\n", hackrf_error_name(r), r);
		free(rf);
		return(RF_ERROR);
	}
	
	r = hackrf_set_baseband_filter_bandwidth(rf->d, hackrf_compute_baseband_filter_bw(rf->sample_rate));
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
	r = rf->sample_rate * 2 * 4 / 10 / TRANSFER_BUFFER_SIZE;
	if(r < 4) r = 4;
	fifo_init(&rf->buffers, r, TRANSFER_BUFFER_SIZE);
	fifo_reader_init(&rf->buffers_reader, &rf->buffers, r / 2);
	
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
	s->write_audio = baseband ? _rf_write_baseband_audio : NULL;
	s->close = _rf_close;
	
	return(RF_OK);
}

