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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "rf.h"

/* File sink */
typedef struct {
	FILE *f;
	void *data;
	size_t data_size;
	size_t samples;
	int complex;
	int type;
} rf_file_t;

static int _rf_file_write_uint8_real(void *private, int16_t *iq_data, size_t samples)
{
	rf_file_t *rf = private;
	uint8_t *u8 = rf->data;
	int i;
	
	while(samples)
	{
		for(i = 0; i < rf->samples && i < samples; i++, iq_data += 2)
		{
			u8[i] = (iq_data[0] - INT16_MIN) >> 8;
		}
		
		fwrite(rf->data, rf->data_size, i, rf->f);
		
		samples -= i;
	}
	
	return(RF_OK);
}

static int _rf_file_write_int8_real(void *private, int16_t *iq_data, size_t samples)
{
	rf_file_t *rf = private;
	int8_t *i8 = rf->data;
	int i;
	
	while(samples)
	{
		for(i = 0; i < rf->samples && i < samples; i++, iq_data += 2)
		{
			i8[i] = iq_data[0] >> 8;
		}
		
		fwrite(rf->data, rf->data_size, i, rf->f);
		
		samples -= i;
	}
	
	return(RF_OK);
}

static int _rf_file_write_uint16_real(void *private, int16_t *iq_data, size_t samples)
{
	rf_file_t *rf = private;
	uint16_t *u16 = rf->data;
	int i;
	
	while(samples)
	{
		for(i = 0; i < rf->samples && i < samples; i++, iq_data += 2)
		{
			u16[i] = (iq_data[0] - INT16_MIN);
		}
		
		fwrite(rf->data, rf->data_size, i, rf->f);
		
		samples -= i;
	}
	
	return(RF_OK);
}

static int _rf_file_write_int16_real(void *private, int16_t *iq_data, size_t samples)
{
	rf_file_t *rf = private;
	int16_t *i16 = rf->data;
	int i;
	
	while(samples)
	{
		for(i = 0; i < rf->samples && i < samples; i++, iq_data += 2)
		{
			i16[i] = iq_data[0];
		}
		
		fwrite(rf->data, rf->data_size, i, rf->f);
		
		samples -= i;
	}
	
	return(RF_OK);
}

static int _rf_file_write_int32_real(void *private, int16_t *iq_data, size_t samples)
{
	rf_file_t *rf = private;
	int32_t *i32 = rf->data;
	int i;
	
	while(samples)
	{
		for(i = 0; i < rf->samples && i < samples; i++, iq_data += 2)
		{
			i32[i] = (iq_data[0] << 16) + iq_data[0];
		}
		
		fwrite(rf->data, rf->data_size, i, rf->f);
		
		samples -= i;
	}
	
	return(RF_OK);
}

static int _rf_file_write_float_real(void *private, int16_t *iq_data, size_t samples)
{
	rf_file_t *rf = private;
	float *f32 = rf->data;
	int i;
	
	while(samples)
	{
		for(i = 0; i < rf->samples && i < samples; i++, iq_data += 2)
		{
			f32[i] = (float) iq_data[0] * (1.0 / 32767.0);
		}
		
		fwrite(rf->data, rf->data_size, i, rf->f);
		
		samples -= i;
	}
	
	return(RF_OK);
}

static int _rf_file_write_uint8_complex(void *private, int16_t *iq_data, size_t samples)
{
	rf_file_t *rf = private;
	uint8_t *u8 = rf->data;
	int i;
	
	while(samples)
	{
		for(i = 0; i < rf->samples && i < samples; i++, iq_data += 2)
		{
			u8[i * 2 + 0] = (iq_data[0] - INT16_MIN) >> 8;
			u8[i * 2 + 1] = (iq_data[1] - INT16_MIN) >> 8;
		}
		
		fwrite(rf->data, rf->data_size, i, rf->f);
		
		samples -= i;
	}
	
	return(RF_OK);
}

static int _rf_file_write_int8_complex(void *private, int16_t *iq_data, size_t samples)
{
	rf_file_t *rf = private;
	int8_t *i8 = rf->data;
	int i;
	
	while(samples)
	{
		for(i = 0; i < rf->samples && i < samples; i++, iq_data += 2)
		{
			i8[i * 2 + 0] = iq_data[0] >> 8;
			i8[i * 2 + 1] = iq_data[1] >> 8;
		}
		
		fwrite(rf->data, rf->data_size, i, rf->f);
		
		samples -= i;
	}
	
	return(RF_OK);
}

static int _rf_file_write_uint16_complex(void *private, int16_t *iq_data, size_t samples)
{
	rf_file_t *rf = private;
	uint16_t *u16 = rf->data;
	int i;
	
	while(samples)
	{
		for(i = 0; i < rf->samples && i < samples; i++, iq_data += 2)
		{
			u16[i * 2 + 0] = (iq_data[0] - INT16_MIN);
			u16[i * 2 + 1] = (iq_data[1] - INT16_MIN);
		}
		
		fwrite(rf->data, rf->data_size, i, rf->f);
		
		samples -= i;
	}
	
	return(RF_OK);
}

static int _rf_file_write_int16_complex(void *private, int16_t *iq_data, size_t samples)
{
	rf_file_t *rf = private;
	
	fwrite(iq_data, sizeof(int16_t) * 2, samples, rf->f);
	
	return(RF_OK);
}

static int _rf_file_write_int32_complex(void *private, int16_t *iq_data, size_t samples)
{
	rf_file_t *rf = private;
	int32_t *i32 = rf->data;
	int i;
	
	while(samples)
	{
		for(i = 0; i < rf->samples && i < samples; i++, iq_data += 2)
		{
			i32[i * 2 + 0] = (iq_data[0] << 16) + iq_data[0];
			i32[i * 2 + 1] = (iq_data[1] << 16) + iq_data[1];
		}
		
		fwrite(rf->data, rf->data_size, i, rf->f);
		
		samples -= i;
	}
	
	return(RF_OK);
}

static int _rf_file_write_float_complex(void *private, int16_t *iq_data, size_t samples)
{
	rf_file_t *rf = private;
	float *f32 = rf->data;
	int i;
	
	while(samples)
	{
		for(i = 0; i < rf->samples && i < samples; i++, iq_data += 2)
		{
			f32[i * 2 + 0] = (float) iq_data[0] * (1.0 / 32767.0);
			f32[i * 2 + 1] = (float) iq_data[1] * (1.0 / 32767.0);
		}
		
		fwrite(rf->data, rf->data_size, i, rf->f);
		
		samples -= i;
	}
	
	return(RF_OK);
}

static int _rf_file_close(void *private)
{
	rf_file_t *rf = private;
	
	if(rf->f && rf->f != stdout) fclose(rf->f);
	if(rf->data) free(rf->data);
	free(rf);
	
	return(RF_OK);
}

int rf_file_open(rf_t *s, char *filename, int type, int complex)
{
	rf_file_t *rf = calloc(1, sizeof(rf_file_t));
	
	if(!rf)
	{
		perror("calloc");
		return(RF_ERROR);
	}
	
	rf->complex = complex != 0;
	rf->type = type;
	
	if(filename == NULL)
	{
		fprintf(stderr, "No output filename provided.\n");
		_rf_file_close(rf);
		return(RF_ERROR);
	}
	else if(strcmp(filename, "-") == 0)
	{
		rf->f = stdout;
	}
	else
	{
		rf->f = fopen(filename, "wb");	
		
		if(!rf->f)
		{
			perror("fopen");
			_rf_file_close(rf);
			return(RF_ERROR);
		}
	}
	
	/* Find the size of the output data type */
	switch(type)
	{
	case RF_UINT8:  rf->data_size = sizeof(uint8_t);  break;
	case RF_INT8:   rf->data_size = sizeof(int8_t);   break;
	case RF_UINT16: rf->data_size = sizeof(uint16_t); break;
	case RF_INT16:  rf->data_size = sizeof(int16_t);  break;
	case RF_INT32:  rf->data_size = sizeof(int32_t);  break;
	case RF_FLOAT:  rf->data_size = sizeof(float);    break;
	default:
		fprintf(stderr, "%s: Unrecognised data type %d\n", __func__, type);
		_rf_file_close(rf);
		return(RF_ERROR);
	}
	
	/* Double the size for complex types */
	if(rf->complex) rf->data_size *= 2;
	
	/* Number of samples in the temporary buffer */
	rf->samples = 4096;
	
	/* Allocate the memory, unless the output is int16 complex */
	if(rf->type != RF_INT16 || !rf->complex)
	{
		rf->data = malloc(rf->data_size * rf->samples);
		if(!rf->data)
		{
			perror("malloc");
			_rf_file_close(rf);
			return(RF_ERROR);
		}
	}
	
	/* Register the callback functions */
	s->ctx = rf;
	s->close = _rf_file_close;
	
	switch(type)
	{
	case RF_UINT8:  s->write = rf->complex ? _rf_file_write_uint8_complex  : _rf_file_write_uint8_real;  break;
	case RF_INT8:   s->write = rf->complex ? _rf_file_write_int8_complex   : _rf_file_write_int8_real;   break;
	case RF_UINT16: s->write = rf->complex ? _rf_file_write_uint16_complex : _rf_file_write_uint16_real; break;
	case RF_INT16:  s->write = rf->complex ? _rf_file_write_int16_complex  : _rf_file_write_int16_real;  break;
	case RF_INT32:  s->write = rf->complex ? _rf_file_write_int32_complex  : _rf_file_write_int32_real;  break;
	case RF_FLOAT:  s->write = rf->complex ? _rf_file_write_float_complex  : _rf_file_write_float_real;  break;
	}
	
	return(RF_OK);
}

