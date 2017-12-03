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
#include "test.h"
#include "ffmpeg.h"
#include "hackrf.h"

int _abort = 0;

static void _sigint_callback_handler(int signum)
{
	fprintf(stderr, "Caught signal %d\n", signum);
	_abort = 1;
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
		"      --videocrypt               Enable Videocrypt I scrambling. Only supported\n"
		"                                 in PAL modes.\n"
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
		"  i      = PAL colour, 25 fps, 625 lines, AM (complex), 6.0 MHz FM audio\n"
		"  b, g   = PAL colour, 25 fps, 625 lines, AM (complex), 5.5 MHz FM audio\n"
		"  pal    = PAL colour, 25 fps, 625 lines, unmodulated (real)\n"
		"  m      = NTSC colour, 30/1.001 fps, 525 lines, AM (complex)\n"
		"  ntsc   = NTSC colour, 30/1.001 fps, 525 lines, unmodulated (real)\n"
		"  a      = No colour, 25 fps, 405 lines, AM (complex)\n"
		"  405    = No colour, 25 fps, 405 lines, unmodulated (real)\n"
		"  240-am = No colour, 25 fps, 240 lines, AM (complex)\n"
		"  240    = No colour, 25 fps, 240 lines, unmodulated (real)\n"
		"\n"
		"NOTE: The number of samples per line is rounded to the nearest integer,\n"
		"which may result in a slight frame rate error.\n"
		"\n"
		"For modes which include audio you also need to ensure the sample rate\n"
		"is adequate to contain both the video signal and audio subcarriers.\n"
		"\n"
		"16MHz works well with PAL modes, and 13.5MHz for NTSC modes.\n"
		"\n"
		"Videocrypt I\n"
		"\n"
		"A video scrambling system used by the Sky TV analogue satellite service in\n"
		"the UK in the 1990s. Each line of the image is cut at a point determined by\n"
		"a pseudorandom number generator, then the two parts are swapped.\n"
		"\n"
		"hacktv only supports the free-access mode, the image is scrambled but a\n"
		"subscription card is not required to decode.\n"
		"\n"
		"Videocrypt is only compatiable with 625 line PAL modes. This version\n"
		"works best when used with samples rates at multiples of 14MHz.\n"
		"\n"
	);
}

#define _OPT_VIDEOCRYPT 1000

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
		{ "videocrypt", no_argument,       0, _OPT_VIDEOCRYPT },
		{ "frequency",  required_argument, 0, 'f' },
		{ "amp",        no_argument,       0, 'a' },
		{ "gain",       required_argument, 0, 'x' },
		{ 0,            0,                 0,  0  }
	};
	static hacktv_t s;
	const vid_configs_t *vid_confs;
	vid_config_t vid_conf;
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
	s.videocrypt = 0;
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
		
		case _OPT_VIDEOCRYPT: /* --videocrypt */
			s.videocrypt = 1;
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
		return(-1);
	}
	
	/* Load the mode configuration */
	for(vid_confs = vid_configs; vid_confs->id != NULL; vid_confs++)
	{
		if(strcmp(s.mode, vid_confs->id) == 0) break;
	}
	
	if(vid_confs->id == NULL)
	{
		fprintf(stderr, "Unrecognised TV mode.\n");
		return(-1);
	}
	
	/* Catch all the signals */
	signal(SIGINT, &_sigint_callback_handler);
	signal(SIGILL, &_sigint_callback_handler);
	signal(SIGFPE, &_sigint_callback_handler);
	signal(SIGSEGV, &_sigint_callback_handler);
	signal(SIGTERM, &_sigint_callback_handler);
	signal(SIGABRT, &_sigint_callback_handler);
	
	memcpy(&vid_conf, vid_confs->conf, sizeof(vid_config_t));
	if(s.gamma > 0)
	{
		vid_conf.gamma = s.gamma;
	}
	
	if(s.videocrypt)
	{
		if(vid_conf.lines != 625 && vid_conf.colour_mode != VID_PAL)
		{
			fprintf(stderr, "Videocrypt I is only compatible with 625 line PAL modes.\n");
		}
		
		vid_conf.videocrypt = 1;
	}
	
	/* Setup video encoder */
	vid_init(&s.vid, s.samplerate, &vid_conf);
	vid_info(&s.vid);
	
	if(strcmp(s.output_type, "hackrf") == 0)
	{
		if(rf_hackrf_open(&s, s.output, s.frequency, s.gain, s.amp) != HACKTV_OK)
		{
			vid_free(&s.vid);
			return(-1);
		}
	}
	else if(strcmp(s.output_type, "file") == 0)
	{
		if(_hacktv_rf_file_open(&s, s.output) != HACKTV_OK)
		{
			vid_free(&s.vid);
			return(-1);
		}
	}
	
	do
	{
		for(c = optind; c < argc && !_abort; c++)
		{
			/* Get a pointer to the output prefix and target */
			pre = argv[c];
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
			
			if(strncmp(pre, "test", l) == 0)
			{
				r = av_test_open(&s.vid);
			}
			else if(strncmp(pre, "ffmpeg", l) == 0)
			{
				r = av_ffmpeg_open(&s.vid, sub);
			}
			else
			{
				r = av_ffmpeg_open(&s.vid, pre);
			}
			
			if(r != HACKTV_OK)
			{
				vid_free(&s.vid);
				return(-1);
			}
			
			while(!_abort)
			{
				size_t samples;
				int16_t *data = vid_next_line(&s.vid, &samples);
				
				if(data == NULL) break;
				
				_hacktv_rf_write(&s, data, samples);
			}
			
			vid_av_close(&s.vid);
		}
	}
	while(s.repeat && !_abort);
	
	_hacktv_rf_close(&s);
	vid_free(&s.vid);
	
	return(0);
}

