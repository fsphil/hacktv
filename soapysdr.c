/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2017 Philip Heron <phil@sanslogic.co.uk>                    */
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
#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include "hacktv.h"

typedef struct {
	
	/* SoapySDR device and stream */
	SoapySDRDevice *d;
	SoapySDRStream *s;
	
} soapysdr_t;

static int _rf_write(void *private, int16_t *iq_data, size_t samples)
{
	soapysdr_t *rf = private;
	const void *buffs[] = { iq_data };
	int flags = 0;
	
	SoapySDRDevice_writeStream(rf->d, rf->s, buffs, samples, &flags, 0, 100000);
	
	return(HACKTV_OK);
}

static int _rf_close(void *private)
{
	soapysdr_t *rf = private;
	
	SoapySDRDevice_deactivateStream(rf->d, rf->s, 0, 0);
	SoapySDRDevice_closeStream(rf->d, rf->s);
	
	SoapySDRDevice_unmake(rf->d);
	
	return(HACKTV_OK);
}

int rf_soapysdr_open(hacktv_t *s, const char *device, unsigned int frequency_hz, unsigned int gain, const char *antenna)
{
	soapysdr_t *rf;
	SoapySDRKwargs *results;
	size_t length;
	
	if(s->vid.conf.output_type != HACKTV_INT16_COMPLEX)
	{
		fprintf(stderr, "rf_soapysdr_open(): Unsupported mode output type for this device.\n");
		return(HACKTV_ERROR);
	}
	
	rf = calloc(1, sizeof(soapysdr_t));
	if(!rf)
	{
		return(HACKTV_OUT_OF_MEMORY);
	}
	
	/* Display a list of devices */
	results = SoapySDRDevice_enumerate(NULL, &length);
	
	if(length <= 0)
	{
		fprintf(stderr, "No SoapySDR devices found.\n");
		free(rf);
		return(HACKTV_ERROR);
	}
	
	/*for(i = 0; i < length; i++)
	{
		fprintf(stderr, "Found device #%ld: ", i);
		
		for(j = 0; j < results[i].size; j++)
		{
			fprintf(stderr, "%s=%s, ", results[i].keys[j], results[i].vals[j]);
		}
		
		fprintf(stderr, "\n");
	}*/
	
	SoapySDRKwargsList_clear(results, length);
	
	/* Prepare the device for output */
	rf->d = SoapySDRDevice_makeStrArgs(device);
	
	if(rf->d == NULL)
	{
		fprintf(stderr, "SoapySDRDevice_make() failed: %s\n", SoapySDRDevice_lastError());
		free(rf);
		return(HACKTV_ERROR);
	}
	
	if(SoapySDRDevice_setSampleRate(rf->d, SOAPY_SDR_TX, 0, s->vid.sample_rate) != 0)
	{
		fprintf(stderr, "SoapySDRDevice_setSampleRate() failed: %s\n", SoapySDRDevice_lastError());
		free(rf);
		return(HACKTV_ERROR);
	}
	
	if(SoapySDRDevice_setFrequency(rf->d, SOAPY_SDR_TX, 0, frequency_hz, NULL) != 0)
	{
		fprintf(stderr, "SoapySDRDevice_setFrequency() failed: %s\n", SoapySDRDevice_lastError());
		free(rf);
		return(HACKTV_ERROR);
	}
	
	if(SoapySDRDevice_setGain(rf->d, SOAPY_SDR_TX, 0, gain) != 0)
	{
		fprintf(stderr, "SoapySDRDevice_setGain() failed: %s\n", SoapySDRDevice_lastError());
		free(rf);
		return(HACKTV_ERROR);
	}
	
	if(antenna && SoapySDRDevice_setAntenna(rf->d, SOAPY_SDR_TX, 0, antenna) != 0)
	{
		fprintf(stderr, "SoapySDRDevice_setAntenna() failed: %s\n", SoapySDRDevice_lastError());
		free(rf);
		return(HACKTV_ERROR);
	}
	
	if(SoapySDRDevice_setupStream(rf->d, &rf->s, SOAPY_SDR_TX, SOAPY_SDR_CS16, NULL, 0, NULL) != 0)
	{
		fprintf(stderr, "SoapySDRDevice_setupStream() failed: %s\n", SoapySDRDevice_lastError());
		free(rf);
		return(HACKTV_ERROR);
	}
	
	SoapySDRDevice_activateStream(rf->d, rf->s, 0, 0, 0);
	
	/* Register the callback functions */
	s->rf_private = rf;
	s->rf_write = _rf_write;
	s->rf_close = _rf_close;
	
	return(HACKTV_OK);
};

