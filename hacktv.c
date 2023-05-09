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
#include "file.h"
#include "hackrf.h"

#ifdef HAVE_SOAPYSDR
#include "soapysdr.h"
#endif

#ifdef HAVE_FL2K
#include "fl2k.h"
#endif

volatile int _abort = 0;

static void _sigint_callback_handler(int signum)
{
	fprintf(stderr, "Caught signal %d\n", signum);
	
	if(_abort > 0)
	{
		exit(-1);
	}
	
	_abort = 1;
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
		"      --pixelrate <value>        Set the video pixel rate in Hz. Default: Sample rate\n"
		"  -l, --level <value>            Set the output level. Default: 1.0\n"
		"  -D, --deviation <value>        Override the mode's FM peak deviation. (Hz)\n"
		"  -G, --gamma <value>            Override the mode's gamma correction value.\n"
		"  -i, --interlace                Update image each field instead of each frame.\n"
		"  -r, --repeat                   Repeat the inputs forever.\n"
		"  -v, --verbose                  Enable verbose output.\n"
		"      --teletext <path>          Enable teletext output. (625 line modes only)\n"
		"      --wss <mode>               Enable WSS output. (625 line modes only)\n"
		"      --videocrypt <mode>        Enable Videocrypt I scrambling. (PAL only)\n"
		"      --videocrypt2 <mode>       Enable Videocrypt II scrambling. (PAL only)\n"
		"      --videocrypts <mode>       Enable Videocrypt S scrambling. (PAL only)\n"
		"      --syster                   Enable Nagravision Syster scambling. (PAL only)\n"
		"      --systeraudio              Invert the audio spectrum when using Syster.\n"
		"      --acp                      Enable Analogue Copy Protection signal.\n"
		"      --vits                     Enable VITS test signals.\n"
		"      --filter                   Enable experimental VSB modulation filter.\n"
		"      --nocolour                 Disable the colour subcarrier (PAL, SECAM, NTSC only).\n"
		"      --noaudio                  Suppress all audio subcarriers.\n"
		"      --nonicam                  Disable the NICAM subcarrier if present.\n"
		"      --a2stereo                 Enable Zweikanalton / A2 Stereo, disables NICAM.\n"
		"      --single-cut               Enable D/D2-MAC single cut video scrambling.\n"
		"      --double-cut               Enable D/D2-MAC double cut video scrambling.\n"
		"      --eurocrypt <mode>         Enable Eurocrypt conditional access for D/D2-MAC.\n"
		"      --scramble-audio           Scramble audio data when using D/D2-MAC modes.\n"
		"      --chid <id>                Set the channel ID (D/D2-MAC).\n"
		"      --offset <value>           Add a frequency offset in Hz (Complex modes only).\n"
		"      --passthru <file>          Read and add an int16 complex signal.\n"
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
		"SoapySDR output options\n"
		"\n"
		"  -o, --output soapysdr[:<opts>] Open a SoapySDR device for output.\n"
		"  -f, --frequency <value>        Set the RF frequency in Hz.\n"
		"  -g, --gain <value>             Set the TX level. Default: 0dB\n"
		"  -A, --antenna <name>           Set the antenna.\n"
		"\n"
		"fl2k output options\n"
		"\n"
		"  -o, --output fl2k[:<dev>]      Open an fl2k device for output.\n"
		"\n"
		"  Real signals are output on the Red channel. Complex signals are output\n"
		"  on the Red (I) and Green (Q) channels.\n"
		"\n"
		"  The 0.7v p-p voltage level of the FL2K is too low to create a correct\n"
		"  composite video signal, it will appear too dark without amplification.\n"
		"\n"
		"File output options\n"
		"\n"
		"  -o, --output file:<filename>   Open a file for output. Use - for stdout.\n"
		"  -t, --type <type>              Set the file data type.\n"
		"\n"
		"Supported file types:\n"
		"\n"
		"  uint8\n"
		"  int8\n"
		"  uint16\n"
		"  int16\n"
		"  int32\n"
		"  float\n"
		"\n"
		"  The default output is int16. The TV mode will determine if the output\n"
		"  is real or complex.\n"
		"\n"
		"  If no valid output prefix is provided, file: is assumed.\n"
		"\n"
		"Supported television modes:\n"
		"\n"
		"  i             = PAL colour, 25 fps, 625 lines, AM (complex), 6.0 MHz FM audio\n"
		"  b, g          = PAL colour, 25 fps, 625 lines, AM (complex), 5.5 MHz FM audio\n"
		"  pal-fm        = PAL colour, 25 fps, 625 lines, FM (complex), 6.5 MHz FM audio\n"
		"  pal           = PAL colour, 25 fps, 625 lines, unmodulated (real)\n"
		"  pal-m         = PAL colour, 30/1.001 fps, 525 lines, AM (complex), 4.5 MHz FM audio\n"
		"  525pal        = PAL colour, 30/1.001 fps, 525 lines, unmodulated (real)\n"
		"  m             = NTSC colour, 30/1.001 fps, 525 lines, AM (complex), 4.5 MHz FM audio\n"
		"  ntsc-fm       = NTSC colour, 30/1.001 fps, 525 lines, FM (complex), 6.5 MHz FM audio\n"
		"  ntsc-bs       = NTSC colour, 30/1.001 fps, 525 lines, FM (complex), BS digital audio\n"
		"  ntsc          = NTSC colour, 30/1.001 fps, 525 lines, unmodulated (real)\n"
		"  l             = SECAM colour, 25 fps, 625 lines, AM (complex), 6.5 MHz AM\n"
		"                  audio\n"
		"  d, k          = SECAM colour, 25 fps, 625 lines, AM (complex), 6.5 MHz FM\n"
		"                  audio\n"
		"  secam-i       = SECAM colour, 25 fps, 625 lines, AM (complex), 6.0 MHz FM audio\n"
		"  secam-fm      = SECAM colour, 25 fps, 625 lines, FM (complex), 6.5 MHz FM audio\n"
		"  secam         = SECAM colour, 25 fps, 625 lines, unmodulated (real)\n"
		"  d2mac-fm      = D2-MAC, 25 fps, 625 lines, FM (complex)\n"
		"  d2mac-am      = D2-MAC, 25 fps, 625 lines, AM (complex)\n"
		"  d2mac         = D2-MAC, 25 fps, 625 lines, unmodulated (real)\n"
		"  dmac-fm       = D-MAC, 25 fps, 625 lines, FM (complex)\n"
		"  dmac-am       = D-MAC, 25 fps, 625 lines, AM (complex)\n"
		"  dmac          = D-MAC, 25 fps, 625 lines, unmodulated (real)\n"
		"  e             = No colour, 25 fps, 819 lines, AM (complex)\n"
		"  819           = No colour, 25 fps, 819 lines, unmodulated (real)\n"
		"  a             = No colour, 25 fps, 405 lines, AM (complex)\n"
		"  405           = No colour, 25 fps, 405 lines, unmodulated (real)\n"
		"  240-am        = No colour, 25 fps, 240 lines, AM (complex)\n"
		"  240           = No colour, 25 fps, 240 lines, unmodulated (real)\n"
		"  30-am         = No colour, 12.5 fps, 30 lines, AM (complex)\n"
		"  30            = No colour, 12.5 fps, 30 lines, unmodulated (real)\n"
		"  nbtv-am       = No colour, 12.5 fps, 32 lines, AM (complex)\n"
		"  nbtv          = No colour, 12.5 fps, 32 lines, unmodulated (real)\n"
		"  apollo-fsc-fm = Field sequential colour, 30/1.001 fps, 525 lines, FM (complex)\n"
		"                  1.25 MHz FM audio\n"
		"  apollo-fsc    = Field sequential colour, 30/1.001 fps, 525 lines, unmodulated\n"
		"                  (real)\n"
		"  apollo-fm     = No colour, 10 fps, 320 lines, FM (complex), 1.25 MHz FM audio\n"
		"  apollo        = No colour, 10 fps, 320 lines, unmodulated (real)\n"
		"  m-cbs405      = Field sequential colour, 72 fps, 405 lines, VSB (complex),\n"
		"                  4.5MHz FM audio\n"
		"  cbs405        = Field sequential colour, 72 fps, 405 lines, unmodulated (real)\n"
		"\n"
		"NOTE: The number of samples per line is rounded to the nearest integer,\n"
		"which may result in a slight frame rate error.\n"
		"\n"
		"For modes which include audio you also need to ensure the sample rate\n"
		"is adequate to contain both the video signal and audio subcarriers.\n"
		"\n"
		"16MHz works well with PAL modes, and 13.5MHz for NTSC modes.\n"
		"\n"
		"20.25MHz is ideal for the D/D2-MAC modes, but may not work with all hackrfs.\n"
		"\n"
		"Teletext\n"
		"\n"
		"Teletext is a digital information service transmitted within the VBI lines of\n"
		"the video signal. Developed in the UK in the 1970s, it was used throughout\n"
		"much of Europe until the end of analogue TV in the 2010s.\n"
		"\n"
		"hacktv supports TTI files. The path can be either a single file or a\n"
		"directory. All files in the directory will be loaded.\n"
		"\n"
		"Raw packet sources are also supported with the raw:<source> path name.\n"
		"The input is expected to be 42 byte teletext packets. Use - for stdin.\n"
		"\n"
		"Lines 7-22 and 320-335 are used, 16 lines per field.\n"
		"\n"
		"Teletext support in hacktv is only compatible with 625 line PAL modes.\n"
		"NTSC and SECAM variations exist and may be supported in the future.\n"
		"\n"
		"WSS (Widescreen Signaling)\n"
		"\n"
		"WSS provides a method to signal to a TV the intended aspect ratio of\n"
		"the video. The following modes are supported:\n"
		"\n"
		"  4:3            = Video is 4:3.\n"
		"  16:9           = Video is 16:9 (Anamorphic).\n"
		"  14:9-letterbox = Crop a 4:3 video to 14:9.\n"
		"  16:9-letterbox = Crop a 4:3 video to 16:9.\n"
		"  auto           = Automatically switch between 4:3 and 16:9.\n"
		"\n"
		"Currently only supported in 625 line modes. A 525 line variant exists and\n"
		"may be supported in future.\n"
		"\n"
		"Videocrypt I\n"
		"\n"
		"A video scrambling system used by the Sky TV analogue satellite service in\n"
		"the UK in the 1990s. Each line of the image is cut at a point determined by\n"
		"a pseudorandom number generator, then the two parts are swapped.\n"
		"\n"
		"hacktv supports the following modes:\n"
		"\n"
		"  free        = Free-access, no subscription card is required to decode.\n"
		"  conditional = A valid Sky card is required to decode. Sample data from MTV.\n"
		"\n"
		"Videocrypt is only compatible with 625 line PAL modes. This version\n"
		"works best when used with samples rates at multiples of 14MHz.\n"
		"\n"
		"Videocrypt II\n"
		"\n"
		"A variation of Videocrypt I used throughout Europe. The scrambling method is\n"
		"identical to VC1, but has a higher VBI data rate.\n"
		"\n"
		"hacktv supports the following modes:\n"
		"\n"
		"  free        = Free-access, no subscription card is required to decode.\n"
		"\n"
		"Both VC1 and VC2 cannot be used together except if both are in free-access mode.\n"
		"\n"
		"Videocrypt S\n"
		"\n"
		"A variation of Videocrypt II used on the short lived BBC Select service. This\n"
		"mode uses line-shuffling rather than line cut-and-rotate.\n"
		"\n"
		"hacktv supports the following modes:\n"
		"\n"
		"  free        = Free-access, no subscription card is required to decode.\n"
		"  conditional = A valid BBC Select card is required to decode. (Does not work yet)\n"
		"\n"
		"Audio inversion is not yet supported.\n"
		"\n"
		"Nagravision Syster\n"
		"\n"
		"Another video scrambling system used in the 1990s in Europe. The video lines\n"
		"are vertically shuffled within a field.\n"
		"\n"
		"Syster is only compatible with 625 line PAL modes and does not currently work\n"
		"with most hardware.\n"
		"\n"
		"Some decoders will invert the audio around 12.8 kHz. For these devices you need\n"
		"to use the --systeraudio option.\n"
		"\n"
		"Eurocrypt\n"
		"\n"
		"Conditional access (CA) system used by D/D2MAC services, M and S2 versions are\n"
		"supported.\n"
		"\n"
		"hacktv supports the following modes:\n"
		"\n"
		"  filmnet     = (M) A valid FilmNet card is required to decode.\n"
		"  tv1000      = (M) A valid Viasat card is required to decode.\n"
		"  ctv         = (M) A valid CTV card is required to decode.\n"
		"  ctvs        = (S) A valid CTV card is required to decode.\n"
		"  tvplus      = (M) A valid TV Plus (Netherlands) card is required to decode.\n"
		"  tvs         = (S) A valid TVS (Denmark) card is required to decode.\n"
		"  rdv         = (S) A valid RDV card is required to decode.\n"
		"  nrk         = (S) A valid NRK card is required to decode.\n"
		"\n"
		"MultiMac style cards can also be used.\n"
		"\n"
	);
}

enum {
	_OPT_TELETEXT = 1000,
	_OPT_WSS,
	_OPT_VIDEOCRYPT,
	_OPT_VIDEOCRYPT2,
	_OPT_VIDEOCRYPTS,
	_OPT_SYSTER,
	_OPT_SYSTERAUDIO,
	_OPT_EUROCRYPT,
	_OPT_ACP,
	_OPT_VITS,
	_OPT_FILTER,
	_OPT_NOCOLOUR,
	_OPT_NOAUDIO,
	_OPT_NONICAM,
	_OPT_A2STEREO,
	_OPT_SINGLE_CUT,
	_OPT_DOUBLE_CUT,
	_OPT_SCRAMBLE_AUDIO,
	_OPT_CHID,
	_OPT_OFFSET,
	_OPT_PASSTHRU,
	_OPT_PIXELRATE,
};

int main(int argc, char *argv[])
{
	int c;
	int option_index;
	static struct option long_options[] = {
		{ "output",         required_argument, 0, 'o' },
		{ "mode",           required_argument, 0, 'm' },
		{ "samplerate",     required_argument, 0, 's' },
		{ "pixelrate",      required_argument, 0, _OPT_PIXELRATE },
		{ "level",          required_argument, 0, 'l' },
		{ "deviation",      required_argument, 0, 'D' },
		{ "gamma",          required_argument, 0, 'G' },
		{ "interlace",      no_argument,       0, 'i' },
		{ "repeat",         no_argument,       0, 'r' },
		{ "verbose",        no_argument,       0, 'v' },
		{ "teletext",       required_argument, 0, _OPT_TELETEXT },
		{ "wss",            required_argument, 0, _OPT_WSS },
		{ "videocrypt",     required_argument, 0, _OPT_VIDEOCRYPT },
		{ "videocrypt2",    required_argument, 0, _OPT_VIDEOCRYPT2 },
		{ "videocrypts",    required_argument, 0, _OPT_VIDEOCRYPTS },
		{ "syster",         no_argument,       0, _OPT_SYSTER },
		{ "systeraudio",    no_argument,       0, _OPT_SYSTERAUDIO },
		{ "acp",            no_argument,       0, _OPT_ACP },
		{ "vits",           no_argument,       0, _OPT_VITS },
		{ "filter",         no_argument,       0, _OPT_FILTER },
		{ "nocolour",       no_argument,       0, _OPT_NOCOLOUR },
		{ "nocolor",        no_argument,       0, _OPT_NOCOLOUR },
		{ "noaudio",        no_argument,       0, _OPT_NOAUDIO },
		{ "nonicam",        no_argument,       0, _OPT_NONICAM },
		{ "a2stereo",       no_argument,       0, _OPT_A2STEREO },
		{ "single-cut",     no_argument,       0, _OPT_SINGLE_CUT },
		{ "double-cut",     no_argument,       0, _OPT_DOUBLE_CUT },
		{ "eurocrypt",      required_argument, 0, _OPT_EUROCRYPT },
		{ "scramble-audio", no_argument,       0, _OPT_SCRAMBLE_AUDIO },
		{ "chid",           required_argument, 0, _OPT_CHID },
		{ "offset",         required_argument, 0, _OPT_OFFSET },
		{ "passthru",       required_argument, 0, _OPT_PASSTHRU },
		{ "frequency",      required_argument, 0, 'f' },
		{ "amp",            no_argument,       0, 'a' },
		{ "gain",           required_argument, 0, 'g' },
		{ "antenna",        required_argument, 0, 'A' },
		{ "type",           required_argument, 0, 't' },
		{ 0,                0,                 0,  0  }
	};
	static hacktv_t s;
	const vid_configs_t *vid_confs;
	vid_config_t vid_conf;
	char *pre, *sub;
	int l;
	int r;
	
	/* Disable console output buffer in Windows */
	#ifdef WIN32
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	#endif
	
	/* Initialise the state */
	memset(&s, 0, sizeof(hacktv_t));
	
	/* Default configuration */
	s.output_type = "hackrf";
	s.output = NULL;
	s.mode = "i";
	s.samplerate = 16000000;
	s.pixelrate = 0;
	s.level = 1.0;
	s.deviation = -1;
	s.gamma = -1;
	s.interlace = 0;
	s.repeat = 0;
	s.verbose = 0;
	s.teletext = NULL;
	s.wss = NULL;
	s.videocrypt = NULL;
	s.videocrypt2 = NULL;
	s.videocrypts = NULL;
	s.syster = 0;
	s.systeraudio = 0;
	s.acp = 0;
	s.vits = 0;
	s.filter = 0;
	s.nocolour = 0;
	s.noaudio = 0;
	s.nonicam = 0;
	s.a2stereo = 0;
	s.scramble_video = 0;
	s.scramble_audio = 0;
	s.chid = -1;
	s.frequency = 0;
	s.amp = 0;
	s.gain = 0;
	s.antenna = NULL;
	s.file_type = HACKTV_INT16;
	
	opterr = 0;
	while((c = getopt_long(argc, argv, "o:m:s:D:G:irvf:al:g:A:t:", long_options, &option_index)) != -1)
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
			else if(strcmp(pre, "soapysdr") == 0)
			{
#ifdef HAVE_SOAPYSDR
				s.output_type = "soapysdr";
				s.output = sub;
#else
				fprintf(stderr, "SoapySDR support is not available in this build of hacktv.\n");
				return(-1);
#endif
			}
			else if(strcmp(pre, "fl2k") == 0)
			{
#ifdef HAVE_FL2K
				s.output_type = "fl2k";
				s.output = sub;
#else
				fprintf(stderr, "FL2K support is not available in this build of hacktv.\n");
				return(-1);
#endif
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
		
		case _OPT_PIXELRATE: /* --pixelrate <value> */
			s.pixelrate = atoi(optarg);
			break;
		
		case 'l': /* -l, --level <value> */
			s.level = atof(optarg);
			break;
		
		case 'D': /* -D, --deviation <value> */
			s.deviation = atof(optarg);
			break;
		
		case 'G': /* -G, --gamma <value> */
			s.gamma = atof(optarg);
			break;
		
		case 'i': /* -i, --interlace */
			s.interlace = 1;
			break;
		
		case 'r': /* -r, --repeat */
			s.repeat = 1;
			break;
		
		case 'v': /* -v, --verbose */
			s.verbose = 1;
			break;
		
		case _OPT_TELETEXT: /* --teletext <path> */
			s.teletext = optarg;
			break;
		
		case _OPT_WSS: /* --wss <mode> */
			s.wss = optarg;
			break;
		
		case _OPT_VIDEOCRYPT: /* --videocrypt */
			s.videocrypt = optarg;
			break;
		
		case _OPT_VIDEOCRYPT2: /* --videocrypt2 */
			s.videocrypt2 = optarg;
			break;
		
		case _OPT_VIDEOCRYPTS: /* --videocrypts */
			s.videocrypts = optarg;
			break;
		
		case _OPT_SYSTER: /* --syster */
			s.syster = 1;
			break;
		
		case _OPT_SYSTERAUDIO: /* --systeraudio */
			s.systeraudio = 1;
			break;
		
		case _OPT_ACP: /* --acp */
			s.acp = 1;
			break;
		
		case _OPT_VITS: /* --vits */
			s.vits = 1;
			break;
		
		case _OPT_FILTER: /* --filter */
			s.filter = 1;
			break;
		
		case _OPT_NOCOLOUR: /* --nocolour / --nocolor */
			s.nocolour = 1;
			break;
		
		case _OPT_NOAUDIO: /* --noaudio */
			s.noaudio = 1;
			break;
		
		case _OPT_NONICAM: /* --nonicam */
			s.nonicam = 1;
			break;
		
		case _OPT_A2STEREO: /* --a2stereo */
			s.a2stereo = 1;
			break;
		
		case _OPT_SINGLE_CUT: /* --single-cut */
			s.scramble_video = 1;
			break;
		
		case _OPT_DOUBLE_CUT: /* --double-cut */
			s.scramble_video = 2;
			break;
		
		case _OPT_EUROCRYPT: /* --eurocrypt */
			s.eurocrypt = optarg;
			break;
		
		case _OPT_SCRAMBLE_AUDIO: /* --scramble-audio */
			s.scramble_audio = 1;
			break;
		
		case _OPT_CHID: /* --chid <id> */
			s.chid = strtol(optarg, NULL, 0);
			break;
		
		case _OPT_OFFSET: /* --offset <value Hz> */
			s.offset = (int64_t) strtod(optarg, NULL);
			break;
		
		case _OPT_PASSTHRU: /* --passthru <path> */
			s.passthru = optarg;
			break;
		
		case 'f': /* -f, --frequency <value> */
			s.frequency = (uint64_t) strtod(optarg, NULL);
			break;
		
		case 'a': /* -a, --amp */
			s.amp = 1;
			break;
		
		case 'g': /* -g, --gain <value> */
			s.gain = atoi(optarg);
			break;
		
		case 'A': /* -A, --antenna <name> */
			s.antenna = optarg;
			break;
		
		case 't': /* -t, --type <type> */
			
			if(strcmp(optarg, "uint8") == 0)
			{
				s.file_type = HACKTV_UINT8;
			}
			else if(strcmp(optarg, "int8") == 0)
			{
				s.file_type = HACKTV_INT8;
			}
			else if(strcmp(optarg, "uint16") == 0)
			{
				s.file_type = HACKTV_UINT16;
			}
			else if(strcmp(optarg, "int16") == 0)
			{
				s.file_type = HACKTV_INT16;
			}
			else if(strcmp(optarg, "int32") == 0)
			{
				s.file_type = HACKTV_INT32;
			}
			else if(strcmp(optarg, "float") == 0)
			{
				s.file_type = HACKTV_FLOAT;
			}
			else
			{
				fprintf(stderr, "Unrecognised file data type.\n");
				return(-1);
			}
			
			break;
		
		case '?':
			print_usage();
			return(0);
		}
	}
	
	if(optind >= argc)
	{
		fprintf(stderr, "No input specified.\n");
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
	
	if(s.deviation > 0)
	{
		/* Override the FM deviation value */
		vid_conf.fm_deviation = s.deviation;
	}
	
	if(s.gamma > 0)
	{
		/* Override the gamma value */
		vid_conf.gamma = s.gamma;
	}
	
	if(s.interlace)
	{
		vid_conf.interlace = 1;
	}
	
	if(s.nocolour)
	{
		if(vid_conf.colour_mode == VID_PAL ||
		   vid_conf.colour_mode == VID_SECAM ||
		   vid_conf.colour_mode == VID_NTSC)
		{
			vid_conf.colour_mode = VID_NONE;
		}
	}
	
	if(s.noaudio > 0)
	{
		/* Disable all audio sub-carriers */
		vid_conf.fm_mono_level = 0;
		vid_conf.fm_left_level = 0;
		vid_conf.fm_right_level = 0;
		vid_conf.am_audio_level = 0;
		vid_conf.nicam_level = 0;
		vid_conf.dance_level = 0;
		vid_conf.fm_mono_carrier = 0;
		vid_conf.fm_left_carrier = 0;
		vid_conf.fm_right_carrier = 0;
		vid_conf.nicam_carrier = 0;
		vid_conf.dance_carrier = 0;
		vid_conf.am_mono_carrier = 0;
	}
	
	if(s.nonicam > 0)
	{
		/* Disable the NICAM sub-carrier */
		vid_conf.nicam_level = 0;
		vid_conf.nicam_carrier = 0;
	}
	
	if(s.a2stereo > 0)
	{
		vid_conf.a2stereo = 1;
	}
	
	vid_conf.scramble_video = s.scramble_video;
	vid_conf.scramble_audio = s.scramble_audio;
	
	vid_conf.level *= s.level;
	
	if(s.teletext)
	{
		if(vid_conf.lines != 625)
		{
			fprintf(stderr, "Teletext is only available with 625 line modes.\n");
			return(-1);
		}
		
		vid_conf.teletext = s.teletext;
	}
	
	if(s.wss)
	{
		if(vid_conf.lines != 625)
		{
			fprintf(stderr, "WSS is only available with 625 line modes.\n");
			return(-1);
		}
		
		vid_conf.wss = s.wss;
	}
	
	if(s.videocrypt)
	{
		if(vid_conf.lines != 625 && vid_conf.colour_mode != VID_PAL)
		{
			fprintf(stderr, "Videocrypt I is only compatible with 625 line PAL modes.\n");
			return(-1);
		}
		
		vid_conf.videocrypt = s.videocrypt;
	}
	
	if(s.videocrypt2)
	{
		if(vid_conf.lines != 625 && vid_conf.colour_mode != VID_PAL)
		{
			fprintf(stderr, "Videocrypt II is only compatible with 625 line PAL modes.\n");
			return(-1);
		}
		
		/* Only allow both VC1 and VC2 if both are in free-access mode */
		if(s.videocrypt && !(strcmp(s.videocrypt, "free") == 0 && strcmp(s.videocrypt2, "free") == 0))
		{
			fprintf(stderr, "Videocrypt I and II cannot be used together except in free-access mode.\n");
			return(-1);
		}
		
		vid_conf.videocrypt2 = s.videocrypt2;
	}
	
	if(s.videocrypts)
	{
		if(vid_conf.lines != 625 && vid_conf.colour_mode != VID_PAL)
		{
			fprintf(stderr, "Videocrypt S is only compatible with 625 line PAL modes.\n");
			return(-1);
		}
		
		if(s.videocrypt || s.videocrypt2)
		{
			fprintf(stderr, "Using multiple scrambling modes is not supported.\n");
			return(-1);
		}
		
		vid_conf.videocrypts = s.videocrypts;
	}
	
	if(s.syster)
	{
		if(vid_conf.lines != 625 && vid_conf.colour_mode != VID_PAL)
		{
			fprintf(stderr, "Nagravision Syster is only compatible with 625 line PAL modes.\n");
			return(-1);
		}
		
		if(vid_conf.videocrypt || vid_conf.videocrypt2 || vid_conf.videocrypts)
		{
			fprintf(stderr, "Using multiple scrambling modes is not supported.\n");
			return(-1);
		}
		
		vid_conf.syster = 1;
		vid_conf.systeraudio = s.systeraudio;
	}
	
	if(s.eurocrypt)
	{
		if(vid_conf.type != VID_MAC)
		{
			fprintf(stderr, "Eurocrypt is only compatible with D/D2-MAC modes.\n");
			return(-1);
		}
		
		if(vid_conf.scramble_video == 0)
		{
			/* Default to single-cut scrambling if none was specified */
			vid_conf.scramble_video = 1;
		}
		
		vid_conf.eurocrypt = s.eurocrypt;
	}
	
	if(s.acp)
	{
		if(vid_conf.lines != 625 && vid_conf.lines != 525)
		{
			fprintf(stderr, "Analogue Copy Protection is only compatible with 525 and 625 line modes.\n");
			return(-1);
		}
		
		if(vid_conf.videocrypt || vid_conf.videocrypt2 || vid_conf.videocrypts || vid_conf.syster)
		{
			fprintf(stderr, "Analogue Copy Protection cannot be used with video scrambling enabled.\n");
			return(-1);
		}
		
		vid_conf.acp = 1;
	}
	
	if(s.vits)
	{
		if(vid_conf.type != VID_RASTER_625 &&
		   vid_conf.type != VID_RASTER_525)
		{
			fprintf(stderr, "VITS is only currently supported for 625 and 525 line raster modes.\n");
			return(-1);
		}
		
		vid_conf.vits = 1;
	}
	
	if(vid_conf.type == VID_MAC && s.chid >= 0)
	{
		vid_conf.chid = (uint16_t) s.chid;
	}
	
	if(s.filter)
	{
		vid_conf.vfilter = 1;
	}
	
	vid_conf.offset = s.offset;
	vid_conf.passthru = s.passthru;
	
	/* Setup video encoder */
	r = vid_init(&s.vid, s.samplerate, s.pixelrate, &vid_conf);
	if(r != VID_OK)
	{
		fprintf(stderr, "Unable to initialise video encoder.\n");
		return(-1);
	}
	
	vid_info(&s.vid);
	
	if(strcmp(s.output_type, "hackrf") == 0)
	{
		if(rf_hackrf_open(&s, s.output, s.frequency, s.gain, s.amp) != HACKTV_OK)
		{
			vid_free(&s.vid);
			return(-1);
		}
	}
#ifdef HAVE_SOAPYSDR
	else if(strcmp(s.output_type, "soapysdr") == 0)
	{
		if(rf_soapysdr_open(&s, s.output, s.frequency, s.gain, s.antenna) != HACKTV_OK)
		{
			vid_free(&s.vid);
			return(-1);
		}
	}
#endif
#ifdef HAVE_FL2K
	else if(strcmp(s.output_type, "fl2k") == 0)
	{
		if(rf_fl2k_open(&s, s.output) != HACKTV_OK)
		{
			vid_free(&s.vid);
			return(-1);
		}
	}
#endif
	else if(strcmp(s.output_type, "file") == 0)
	{
		if(rf_file_open(&s, s.output, s.file_type) != HACKTV_OK)
		{
			vid_free(&s.vid);
			return(-1);
		}
	}
	
	av_ffmpeg_init();
	
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
				/* Error opening this source. Move to the next */
				continue;
			}
			
			while(!_abort)
			{
				size_t samples;
				int16_t *data = vid_next_line(&s.vid, &samples);
				
				if(data == NULL) break;
				
				if(_hacktv_rf_write(&s, data, samples) != HACKTV_OK) break;
			}
			
			vid_av_close(&s.vid);
		}
	}
	while(s.repeat && !_abort);
	
	_hacktv_rf_close(&s);
	vid_free(&s.vid);
	
	av_ffmpeg_deinit();
	
	fprintf(stderr, "\n");
	
	return(0);
}

