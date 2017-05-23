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
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include "hacktv.h"
#include "ffmpeg.h"
#include "hackrf.h"

int _abort = 0;

static void _sigint_callback_handler(int signum)
{
	fprintf(stderr, "Caught signal %d\n", signum);
	_abort = 1;
}

/* AV test pattern source */
typedef struct {
	uint32_t *video;
	int16_t *audio;
} av_test_t;

static uint32_t *_hacktv_av_test_read_video(void *private)
{
	av_test_t *av = private;
	return(av->video);
}

static int16_t *_hacktv_av_test_read_audio(void *private, size_t samples)
{
	av_test_t *av = private;
	return(av->audio);
}

static int _hacktv_av_test_close(void *private)
{
	av_test_t *av = private;
	if(av->video) free(av->video);
	if(av->audio) free(av->audio);
	free(av);
	return(HACKTV_OK);
}

static int _hacktv_av_test_open(hacktv_t *s)
{
	uint32_t const bars[8] = {
		0x000000,
		0x0000FF,
		0xFF0000,
		0xFF00FF,
		0x00FF00,
		0x00FFFF,
		0xFFFF00,
		0xFFFFFF,
	};
	av_test_t *av;
	int c, x, y;
	
	av = calloc(1, sizeof(av_test_t));
	if(!av)
	{
		return(HACKTV_OUT_OF_MEMORY);
	}

	/* Generate a basic test pattern */
	av->video = malloc(vid_get_framebuffer_length(&s->vid));
	if(!av->video)
	{
		free(av);
		return(HACKTV_OUT_OF_MEMORY);
	}
	
	for(y = 0; y < s->vid.conf.active_lines; y++)
	{
		for(x = 0; x < s->vid.active_width; x++)
		{
			if(y < 400)
			{
				/* 100% colour bars */
				c = 7 - x * 8 / s->vid.active_width;
				c = bars[c];
			}
			else if(y < 420)
			{
				/* 100% red */
				c = 0xFF0000;
			}
			else if(y < 440)
			{
				/* Gradient black to white */
				c = x * 0xFF / (s->vid.active_width - 1);
				c = c << 16 | c << 8 | c;
			}
			else
			{
				/* 8 level grey bars */
				c = x * 0xFF / (s->vid.active_width - 1);
				c &= 0xE0;
				c = c | (c >> 3) | (c >> 6);
				c = c << 16 | c << 8 | c;
			}
			
			av->video[y * s->vid.active_width + x] = c;
		}
	}
	
	/* TODO audio */
	av->audio = NULL;
	
	/* Register the callback functions */
	s->av_private = av;
	s->av_read_video = _hacktv_av_test_read_video;
	s->av_read_audio = _hacktv_av_test_read_audio;
	s->av_close = _hacktv_av_test_close;
	
	return(HACKTV_OK);
}

/* File sink */
typedef struct {
	FILE *f;
	int type;
} rf_file_t;

static int _hacktv_rf_file_write(void *private, int16_t *iq_data, size_t samples)
{
	rf_file_t *rf = private;
	int i;
	
	if(rf->type == HACKTV_INT16_COMPLEX)
	{
		/* Write the IQ real/imaginary components */
		fwrite(iq_data, sizeof(int16_t), samples * 2, rf->f);
	}
	else if(rf->type == HACKTV_INT16_REAL)
	{
		int16_t *a, *b;
		
		/* We only want the I/real component */
		a = &iq_data[1];
		b = &iq_data[2];
		
		for(i = 1; i < samples; i++)
		{
			*a = *b;
			a += 1;
			b += 2;
		}
		
		fwrite(iq_data, sizeof(int16_t), samples, rf->f);
	}
	
	return(HACKTV_OK);
}

static int _hacktv_rf_file_close(void *private)
{
	rf_file_t *rf = private;
	if(rf->f && rf->f != stdout) fclose(rf->f);
	free(rf);
	
	return(HACKTV_OK);
}

static int _hacktv_rf_file_open(hacktv_t *s, char *filename)
{
	rf_file_t *rf = calloc(1, sizeof(rf_file_t));
	
	rf->type = s->vid.conf.output_type;
	
	if(filename == NULL)
	{
		fprintf(stderr, "No output filename provided.\n");
		free(rf);
		return(HACKTV_ERROR);
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
			free(rf);
			return(HACKTV_ERROR);
		}
	}
	
	/* Register the callback functions */
	s->rf_private = rf;
	s->rf_write = _hacktv_rf_file_write;
	s->rf_close = _hacktv_rf_file_close;
	
	return(HACKTV_OK);
}

/* AV source callback handlers */
static uint32_t *_hacktv_av_read_video(hacktv_t *s)
{
	if(s->av_read_video) return(s->av_read_video(s->av_private));
	return(NULL);
}

static int16_t *_hacktv_av_read_audio(hacktv_t *s, size_t samples)
{
	if(s->av_read_audio)
	{
		return(s->av_read_audio(s->av_private, samples));
	}
	
	return(NULL);
}

static int _hacktv_av_close(hacktv_t *s)
{
	if(s->av_close)
	{
		return(s->av_close(s->av_private));
	}
	
	return(HACKTV_ERROR);
}

/* RF sink callback handlers */
static int _hacktv_rf_write(hacktv_t *s, int16_t *iq_data, size_t samples)
{
	if(s->rf_write)
	{
		return(s->rf_write(s->rf_private, iq_data, samples));
	}
	
	return(HACKTV_ERROR);
}

static int _hacktv_rf_close(hacktv_t *s)
{
	if(s->rf_close)
	{
		return(s->rf_close(s->rf_private));
	}
	
	return(HACKTV_OK);
}

static void print_usage(void)
{
	printf(
		"\n"
		"Usage: hacktv [options] input [input...]\n"
		"\n"
		"  -o, --output <target>          Set the output device or file, Default: hackrf\n"
		"  -m, --mode <name>              Set the television mode. Default: i\n"
		"  -s, --samplerate <value>       Set the sample rate in Hz. Default: 16MHz\n"
		"  -G, --gamma <value>            Override the mode's gamma correction value.\n"
		"  -r, --repeat                   Repeat the inputs forever.\n"
		"  -v, --verbose                  Enable verbose output.\n"
		"\n"
		"Input options\n"
		"\n"
		"  test:colourbars    Generate and transmit a test pattern.\n"
		"  ffmpeg:<file|url>  Decode and transmit a video file with ffmpeg.\n"
		"\n"
		"  If no valid input prefix is provided, ffmpeg: is assumed.\n"
		"\n"
		"HackRF output options\n"
		"\n"
		"  -o, --output hackrf[:<serial>] Open a HackRF for output.\n"
		"  -f, --frequency <value>        Set the RF frequency in Hz, 0MHz to 7250MHz.\n"
		"  -a, --amp                      Enable the TX RF amplifier.\n"
		"  -g, --gain <value>             Set the TX VGA (IF) gain, 0-47dB. Default: 0dB\n"
		"\n"
		"  Only modes with a complex output are supported by the HackRF.\n"
		"\n"
		"File output options\n"
		"\n"
		"  -o, --output file:<filename>   Open a file for output. Use - for stdout.\n"
		"\n"
		"  If no valid output prefix is provided, file: is assumed.\n"
		"  The output format is int16, native endian. The TV mode will\n"
		"  determine if the output is real or complex.\n"
		"\n"
		"Supported television modes:\n"
		"\n"
		"  i    = PAL colour, 25 fps, 625 lines, AM (complex)\n"
		"  pal  = PAL colour, 25 fps, 625 lines, unmodulated (real)\n"
		"  m    = NTSC colour, 30/1.001 fps, 525 lines, AM (complex)\n"
		"  ntsc = NTSC colour, 30/1.001 fps, 525 lines, unmodulated (real)\n"
		"  a    = No colour, 25 fps, 405 lines, AM (complex)\n"
		"  405  = No colour, 25 fps, 405 lines, unmodulated (real)\n"
		"\n"
		"NOTE: The number of samples per line is rounded to the nearest integer,\n"
		"which may result in a slight frame rate error.\n"
		"\n"
		"For modes which include audio you also need to ensure the sample rate\n"
		"is adequate to contain both the video signal and audio subcarriers.\n"
		"\n"
		"16MHz works well with PAL modes, and 13.5MHz for NTSC modes.\n"
		"\n"
	);
}

int main(int argc, char *argv[])
{
	int c;
	int option_index;
	static struct option long_options[] = {
		{ "output",     required_argument, 0, '0' },
		{ "mode",       required_argument, 0, 'm' },
		{ "samplerate", required_argument, 0, 's' },
		{ "gamma",      required_argument, 0, 'G' },
		{ "repeat",     no_argument,       0, 'r' },
		{ "verbose",    no_argument,       0, 'v' },
		{ "frequency",  required_argument, 0, 'f' },
		{ "amp",        no_argument,       0, 'a' },
		{ "gain",       required_argument, 0, 'x' },
		{ 0,            0,                 0,  0  }
	};
	static hacktv_t s;
	uint32_t *framebuffer;
	vid_config_t vid_conf;
	const vid_config_t *vid_ptr;
	char *pre, *sub;
	int l;
	int r;
	
	/* Initialise the state */
	memset(&s, 0, sizeof(hacktv_t));
	
	/* Default configuration */
	s.output_type = "hackrf";
	s.output = NULL;
	s.mode = "i";
	s.samplerate = 16000000;
	s.gamma = -1;
	s.repeat = 0;
	s.verbose = 0;
	s.frequency = 0;
	s.amp = 0;
	s.gain = 0;
	
	opterr = 0;
	while((c = getopt_long(argc, argv, "o:m:s:G:rvf:ag:", long_options, &option_index)) != -1)
	{
		switch(c)
		{
		case 'o': /* -o, --output <[type:]target> */
			
			/* Get a pointer to the output prefix and target */
			pre = optarg;
			sub = strchr(pre, ':');
			
			if(sub != NULL)
			{
				/* Split the optarg into two */
				*sub = '\0';
				sub++;
			}
			
			/* Try to match the prefix with a known type */
			if(strcmp(pre, "file") == 0)
			{
				s.output_type = "file";
				s.output = sub;
			}
			else if(strcmp(pre, "hackrf") == 0)
			{
				s.output_type = "hackrf";
				s.output = sub;
			}
			else
			{
				/* Unrecognised output type, default to file */
				if(sub != NULL)
				{
					/* Recolonise */
					sub--;
					*sub = ':';
				}
				
				s.output_type = "file";
				s.output = pre;
			}
			
			break;
		
		case 'm': /* -m, --mode <name> */
			s.mode = optarg;
			break;
		
		case 's': /* -s, --samplerate <value> */
			s.samplerate = atoi(optarg);
			break;
		
		case 'G': /* -G, --gamma <value> */
			s.gamma = atof(optarg);
			break;
		
		case 'r': /* -r, --repeat */
			s.repeat = 1;
			break;
		
		case 'v': /* -v, --verbose */
			s.verbose = 1;
			break;
		
		case 'f': /* -f, --frequency <value> */
			s.frequency = atol(optarg);
			break;
		
		case 'a': /* -a, --amp */
			s.amp = 1;
			break;
		
		case 'g': /* -g, --gain <value> */
			s.gain = atoi(optarg);
			break;
		
		case '?':
			print_usage();
			return(0);
		}
	}
	
	if(optind >= argc)
	{
		printf("No input specified.\n");
		print_usage();
		return(-1);
	}
	
	/* Load the mode configuration */
	if(strcmp(s.mode, "i") == 0)
	{
		vid_ptr = &vid_config_pal_i;
	}
	else if(strcmp(s.mode, "pal") == 0)
	{
		vid_ptr = &vid_config_pal;
	}
	else if(strcmp(s.mode, "m") == 0)
	{
		vid_ptr = &vid_config_ntsc_m;
	}
	else if(strcmp(s.mode, "ntsc") == 0)
	{
		vid_ptr = &vid_config_ntsc;
	}
	else if(strcmp(s.mode, "a") == 0)
	{
		vid_ptr = &vid_config_405_a;
	}
	else if(strcmp(s.mode, "405") == 0)
	{
		vid_ptr = &vid_config_405;
	}
	else
	{
		fprintf(stderr, "Unrecognised TV mode.\n");
		print_usage();
		return(-1);
	}
	
	/* Catch all the signals */
	signal(SIGINT, &_sigint_callback_handler);
	signal(SIGILL, &_sigint_callback_handler);
	signal(SIGFPE, &_sigint_callback_handler);
	signal(SIGSEGV, &_sigint_callback_handler);
	signal(SIGTERM, &_sigint_callback_handler);
	signal(SIGABRT, &_sigint_callback_handler);
	
	memcpy(&vid_conf, vid_ptr, sizeof(vid_config_t));
	if(s.gamma > 0)
	{
		vid_conf.gamma = s.gamma;
	}
	
	/* Setup video encoder */
	vid_init(&s.vid, s.samplerate, &vid_conf);
	vid_info(&s.vid);
	
	if(strcmp(s.output_type, "hackrf") == 0)
	{
		if(rf_hackrf_open(&s, s.output, s.frequency, s.gain, s.amp) != HACKTV_OK)
		{
			_hacktv_av_close(&s);
			vid_free(&s.vid);
			return(-1);
		}
	}
	else if(strcmp(s.output_type, "file") == 0)
	{
		if(_hacktv_rf_file_open(&s, s.output) != HACKTV_OK)
		{
			_hacktv_av_close(&s);
			vid_free(&s.vid);
			return(-1);
		}
	}
	
	do
	{
		for(; optind < argc; optind++)
		{
			/* Get a pointer to the output prefix and target */
			pre = argv[optind];
			sub = strchr(pre, ':');
			
			if(sub != NULL)
			{
				l = sub - pre;
				sub++;
			}
			else
			{
				l = strlen(pre);
			}
			
			if(strncmp(argv[optind], "test", l) == 0)
			{
				r = _hacktv_av_test_open(&s);
			}
			else if(strncmp(argv[optind], "ffmpeg", l) == 0)
			{
				r = av_ffmpeg_open(&s, sub);
			}
			else
			{
				r = av_ffmpeg_open(&s, pre);
			}
			
			if(r != HACKTV_OK)
			{
				vid_free(&s.vid);
				return(-1);
			}
			
			while((framebuffer = _hacktv_av_read_video(&s)) && !_abort)
			{
				vid_set_framebuffer(&s.vid, framebuffer);
				
				for(l = 0; l < s.vid.conf.lines; l++)
				{
					_hacktv_rf_write(&s, vid_next_line(&s.vid), s.vid.width);
				}
			}
			
			_hacktv_av_close(&s);
		}
	}
	while(s.repeat && !_abort);
	
	_hacktv_rf_close(&s);
	vid_free(&s.vid);
	
	return(0);
}

