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
#include "av.h"
#include "rf.h"

static volatile sig_atomic_t _abort = 0;
static volatile sig_atomic_t _signal = 0;

static void _sigint_callback_handler(int signum)
{
	_abort = 1;
	_signal = signum;
}

static void print_version(void)
{
	printf("hacktv %s\n", VERSION);
}

static void print_usage(void)
{
	printf(
		"\n"
		"Usage: hacktv [options] input [input...]\n"
		"\n"
		"  -o, --output <target>          Set the output device or file, Default: hackrf\n"
		"  -m, --mode <name>              Set the television mode. Default: i\n"
		"      --list-modes               List available modes and exit.\n"
		"  -s, --samplerate <value>       Set the sample rate in Hz. Default: 16MHz\n"
		"      --pixelrate <value>        Set the video pixel rate in Hz. Default: Sample rate\n"
		"  -l, --level <value>            Set the output level. Default: 1.0\n"
		"  -D, --deviation <value>        Override the mode's FM peak deviation. (Hz)\n"
		"  -G, --gamma <value>            Override the mode's gamma correction value.\n"
		"  -i, --interlace                Update image each field instead of each frame.\n"
		"      --fit <mode>               Set fit mode (stretch, fill, fit, or none), Default: stretch\n"
		"      --min-aspect <value>       Set the minimum aspect ratio for fit mode.\n"
		"      --max-aspect <value>       Set the maximum aspect ratio for fit mode.\n"
		"  -r, --repeat                   Repeat the inputs forever.\n"
		"      --shuffle                  Randomly shuffle the inputs.\n"
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
		"      --cc608                    Enable CEA/EIA-608 closed-caption pass through.\n"
		"      --vitc                     Enable VITC time code.\n"
		"      --filter                   Enable experimental VSB modulation filter.\n"
		"      --nocolour                 Disable the colour subcarrier (PAL, SECAM, NTSC only).\n"
		"      --s-video                  Output colour subcarrier on second channel.\n"
		"                                 (PAL, NTSC, SECAM baseband modes only).\n"
		"      --volume <value>           Adjust volume. Takes floats as argument.\n"
		"      --noaudio                  Suppress all audio subcarriers.\n"
		"      --nonicam                  Disable the NICAM subcarrier if present.\n"
		"      --a2stereo                 Enable Zweikanalton / A2 Stereo, disables NICAM.\n"
		"      --single-cut               Enable D/D2-MAC single cut video scrambling.\n"
		"      --double-cut               Enable D/D2-MAC double cut video scrambling.\n"
		"      --eurocrypt <mode>         Enable Eurocrypt conditional access for D/D2-MAC.\n"
		"      --ec-mat-rating <rating>   Enable Eurocrypt maturity rating.\n"
		"      --ec-ppv <pnum,cost>       Enable Eurocrypt PPV.\n"
		"      --scramble-audio           Scramble audio data when using D/D2-MAC modes.\n"
		"      --chid <id>                Set the channel ID (D/D2-MAC).\n"
		"      --showecm                  Show input and output control words for scrambled modes.\n"
		"      --mac-audio-stereo         Use stereo audio (D/D2-MAC). (Default)\n"
		"      --mac-audio-mono           Use mono audio (D/D2-MAC).\n"
		"      --mac-audio-high-quality   Use high quality 32 kHz audio (D/D2-MAC).\n"
		"                                 (Default)\n"
		"      --mac-audio-medium-quality Use medium quality 16 kHz audio (D/D2-MAC).\n"
		"      --mac-audio-companded      Use companded audio compression (D/D2-MAC).\n"
		"                                 (Default)\n"
		"      --mac-audio-linear         Use linear audio. (D/D2-MAC).\n"
		"      --mac-audio-l1-protection  Use first level protection (D/D2-MAC).\n"
		"                                 (Default)\n"
		"      --mac-audio-l2-protection  Use second level protection (D/D2-MAC).\n"
		"      --sis <mode>               Enable Sound-in-Syncs (dcsis only)\n"
		"      --swap-iq                  Swap the I and Q channels to invert the spectrum.\n"
		"                                 Applied before offset and passthru. (Complex modes only).\n"
		"      --offset <value>           Add a frequency offset in Hz (Complex modes only).\n"
		"      --passthru <file>          Read and add an int16 complex signal.\n"
		"      --invert-video             Invert the composite video signal sync and\n"
		"                                 white levels.\n"
		"      --secam-field-id           Enable SECAM field identification.\n"
		"      --secam-field-id-lines <x> Set the number of lines per field used for SECAM field\n"
		"                                 identification. (1-9, default: 9)\n"
		"      --json                     Output a JSON array when used with --list-modes.\n"
		"      --version                  Print the version number and exit.\n"
		"\n"
		"Input options\n"
		"\n"
		"  test:colourbars    Generate and transmit a test pattern.\n"
		"  ffmpeg:<file|url>  Decode and transmit a video file with ffmpeg.\n"
		"\n"
		"  If no valid input prefix is provided, ffmpeg: is assumed.\n"
		"\n"
		"ffmpeg input options\n"
		"\n"
		"      --ffmt <format>            Force input file format.\n"
		"      --fopts <option=value[:option2=value]>\n"
		"                                 Pass option(s) to ffmpeg.\n"
		"\n"
		"HackRF output options\n"
		"\n"
		"  -o, --output hackrf[:<serial>] Open a HackRF for output.\n"
		"  -f, --frequency <value>        Set the RF frequency in Hz, 0MHz to 7250MHz.\n"
		"  -a, --amp                      Enable the TX RF amplifier.\n"
		"  -g, --gain <value>             Set the TX VGA (IF) gain, 0-47dB. Default: 0dB\n"
		"\n"
		"  Only complex modes (RF) are supported by the HackRF.\n"
		"  Baseband modes are support with the addition of a HackDAC module.\n"
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
		"      --fl2k-audio <mode>        Audio mode (none, stereo, spdif), default: none\n"
		"\n"
		"  Each of the FL2K's three output channels can be used for:\n"
		"\n"
		"  Red: Baseband video / Complex I signal\n"
		"  Green: Complex Q signal / Analogue audio (Left) / Nothing\n"
		"  Blue: Analogue audio (Right) / Digital audio (S/PDIF) / Nothing\n"
		"\n"
		"  The 0.7v p-p voltage level of the FL2K is too low to create a correct\n"
		"  composite video signal, it will appear too dark without amplification.\n"
		"\n"
		"  Digital S/PDIF audio is currently fixed at 16-bit, 32 kHz. Not all\n"
		"  decoders work at this sample rate.\n"
		"\n"
		"  Analogue audio is limited to 8-bits. The LSB is delta-sigma modulated at\n"
		"  the FL2K sample rate with the lower 8-bits of the 16-bit audio and may be\n"
		"  recovered by using a low pass filter of ~16 kHz on the output.\n"
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
		"  4:3             = 4:3 video\n"
		"  14:9-letterbox  = 14:9 video centred\n"
		"  14:9-top        = 14:9 video at top\n"
		"  16:9-letterbox  = 16:9 video centred\n"
		"  16:9-top        = 16:9 video at top\n"
		"  16:9+-letterbox = >16:9 video centred\n"
		"  14:9-window     = 4:3 video with a 14:9 protected window\n"
		"  16:9            = 16:9 video (Anamorphic)\n"
		"  auto            = Automatically switch between 4:3 and 16:9 modes.\n"
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
		"  cplus       = (3DES) A valid Canal+ Nordic card is required to decode.\n"
		"  tv3update   = (M) Autoupdate mode with included PIC/EEPROM files.\n"
		"  cplusfr43   = (M) Autoupdating mode for Canal+ France cards.\n"
		"  cplusfr169  = (M) Autoupdating mode for Canal+ France cards.\n"
		"  cinecfr     = (M) Autoupdating mode for Canal+ France cards.\n"
		"\n"
		"MultiMac style cards can also be used.\n"
		"\n"
	);
}

/* fputs() a string with JSON-style escape sequences */
static int _fputs_json(const char *str, FILE *stream)
{
	int c;
	
	for(c = 0; *str; str++)
	{
		const char *s = NULL;
		int r;
		
		switch(*str)
		{
		case '"': s = "\\\""; break;
		case '\\': s = "\\\\"; break;
		//case '/': s = "\\/"; break;
		case '\b': s = "\\b"; break;
		case '\f': s = "\\f"; break;
		case '\n': s = "\\n"; break;
		case '\r': s = "\\r"; break;
		case '\t': s = "\\t"; break;
		}
		
		if(s) r = fputs(s, stream);
		else r = fputc(*str, stream) == EOF ? EOF : 1;
		
		if(r == EOF)
		{
			return(c > 0 ? c : EOF);
		}
		
		c += r;
	}
	
	return(c);
}

/* List all avaliable modes, optionally formatted as a JSON array */
static void _list_modes(int json)
{
	const vid_configs_t *vc;
	
	if(json) printf("[\n");
	
	/* Load the mode configuration */
	for(vc = vid_configs; vc->id != NULL; vc++)
	{
		if(json)
		{
			printf("  {\n    \"id\": \"");
			_fputs_json(vc->id, stdout);
			printf("\",\n    \"description\": \"");
			_fputs_json(vc->desc ? vc->desc : "", stdout);
			printf("\"\n  }%s\n", vc[1].id != NULL ? "," : "");
		}
		else
		{
			printf("  %-14s = %s\n", vc->id, vc->desc ? vc->desc : "");
		}
	}
	
	if(json) printf("]\n");
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
	_OPT_VITC,
	_OPT_CC608,
	_OPT_FILTER,
	_OPT_NOCOLOUR,
	_OPT_S_VIDEO,
	_OPT_VOLUME,
	_OPT_NOAUDIO,
	_OPT_NONICAM,
	_OPT_A2STEREO,
	_OPT_SINGLE_CUT,
	_OPT_DOUBLE_CUT,
	_OPT_SCRAMBLE_AUDIO,
	_OPT_CHID,
	_OPT_SHOW_ECM,
	_OPT_NODATE,
	_OPT_EC_MAT_RATING,
	_OPT_EC_PPV,
	_OPT_MAC_AUDIO_STEREO,
	_OPT_MAC_AUDIO_MONO,
	_OPT_MAC_AUDIO_HIGH_QUALITY,
	_OPT_MAC_AUDIO_MEDIUM_QUALITY,
	_OPT_MAC_AUDIO_COMPANDED,
	_OPT_MAC_AUDIO_LINEAR,
	_OPT_MAC_AUDIO_L1_PROTECTION,
	_OPT_MAC_AUDIO_L2_PROTECTION,
	_OPT_SIS,
	_OPT_SWAP_IQ,
	_OPT_OFFSET,
	_OPT_PASSTHRU,
	_OPT_INVERT_VIDEO,
	_OPT_RAW_BB_FILE,
	_OPT_RAW_BB_BLANKING,
	_OPT_RAW_BB_WHITE,
	_OPT_SECAM_FIELD_ID,
	_OPT_SECAM_FIELD_ID_LINES,
	_OPT_FFMT,
	_OPT_FOPTS,
	_OPT_PIXELRATE,
	_OPT_LIST_MODES,
	_OPT_JSON,
	_OPT_SHUFFLE,
	_OPT_FIT,
	_OPT_MIN_ASPECT,
	_OPT_MAX_ASPECT,
	_OPT_LETTERBOX,
	_OPT_PILLARBOX,
	_OPT_FL2K_AUDIO,
	_OPT_THREADS,
	_OPT_VERSION,
};

int main(int argc, char *argv[])
{
	int c;
	int option_index;
	static struct option long_options[] = {
		{ "output",         required_argument, 0, 'o' },
		{ "mode",           required_argument, 0, 'm' },
		{ "list-modes",     no_argument,       0, _OPT_LIST_MODES },
		{ "samplerate",     required_argument, 0, 's' },
		{ "pixelrate",      required_argument, 0, _OPT_PIXELRATE },
		{ "level",          required_argument, 0, 'l' },
		{ "deviation",      required_argument, 0, 'D' },
		{ "gamma",          required_argument, 0, 'G' },
		{ "interlace",      no_argument,       0, 'i' },
		{ "fit",            required_argument, 0, _OPT_FIT },
		{ "min-aspect",     required_argument, 0, _OPT_MIN_ASPECT },
		{ "max-aspect",     required_argument, 0, _OPT_MAX_ASPECT },
		{ "letterbox",      no_argument,       0, _OPT_LETTERBOX },
		{ "pillarbox",      no_argument,       0, _OPT_PILLARBOX },
		{ "repeat",         no_argument,       0, 'r' },
		{ "shuffle",        no_argument,       0, _OPT_SHUFFLE },
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
		{ "vitc",           no_argument,       0, _OPT_VITC },
		{ "cc608",          no_argument,       0, _OPT_CC608 },
		{ "filter",         no_argument,       0, _OPT_FILTER },
		{ "nodate",         no_argument,       0, _OPT_NODATE },
		{ "nocolour",       no_argument,       0, _OPT_NOCOLOUR },
		{ "nocolor",        no_argument,       0, _OPT_NOCOLOUR },
		{ "s-video",        no_argument,       0, _OPT_S_VIDEO },
		{ "volume",         required_argument, 0, _OPT_VOLUME },
		{ "noaudio",        no_argument,       0, _OPT_NOAUDIO },
		{ "nonicam",        no_argument,       0, _OPT_NONICAM },
		{ "a2stereo",       no_argument,       0, _OPT_A2STEREO },
		{ "single-cut",     no_argument,       0, _OPT_SINGLE_CUT },
		{ "double-cut",     no_argument,       0, _OPT_DOUBLE_CUT },
		{ "eurocrypt",      required_argument, 0, _OPT_EUROCRYPT },
		{ "ec-mat-rating",  required_argument, 0, _OPT_EC_MAT_RATING },
		{ "ec-ppv",         optional_argument, 0, _OPT_EC_PPV },
		{ "scramble-audio", no_argument,       0, _OPT_SCRAMBLE_AUDIO },
		{ "chid",           required_argument, 0, _OPT_CHID },
		{ "mac-audio-stereo", no_argument,     0, _OPT_MAC_AUDIO_STEREO },
		{ "mac-audio-mono", no_argument,       0, _OPT_MAC_AUDIO_MONO },
		{ "mac-audio-high-quality", no_argument, 0, _OPT_MAC_AUDIO_HIGH_QUALITY },
		{ "mac-audio-medium-quality", no_argument, 0, _OPT_MAC_AUDIO_MEDIUM_QUALITY },
		{ "mac-audio-companded", no_argument,  0, _OPT_MAC_AUDIO_COMPANDED },
		{ "mac-audio-linear", no_argument,     0, _OPT_MAC_AUDIO_LINEAR },
		{ "mac-audio-l1-protection", no_argument, 0, _OPT_MAC_AUDIO_L1_PROTECTION },
		{ "mac-audio-l2-protection", no_argument, 0, _OPT_MAC_AUDIO_L2_PROTECTION },
		{ "sis",            required_argument, 0, _OPT_SIS },
		{ "swap-iq",        no_argument,       0, _OPT_SWAP_IQ },
		{ "offset",         required_argument, 0, _OPT_OFFSET },
		{ "passthru",       required_argument, 0, _OPT_PASSTHRU },
		{ "invert-video",   no_argument,       0, _OPT_INVERT_VIDEO },
		{ "raw-bb-file",    required_argument, 0, _OPT_RAW_BB_FILE },
		{ "raw-bb-blanking", required_argument, 0, _OPT_RAW_BB_BLANKING },
		{ "raw-bb-white",   required_argument, 0, _OPT_RAW_BB_WHITE },
		{ "secam-field-id", no_argument,       0, _OPT_SECAM_FIELD_ID },
		{ "secam-field-id-lines", required_argument, 0, _OPT_SECAM_FIELD_ID_LINES },
		{ "json",           no_argument,       0, _OPT_JSON },
		{ "ffmt",           required_argument, 0, _OPT_FFMT },
		{ "fopts",          required_argument, 0, _OPT_FOPTS },
		{ "frequency",      required_argument, 0, 'f' },
		{ "amp",            no_argument,       0, 'a' },
		{ "gain",           required_argument, 0, 'g' },
		{ "antenna",        required_argument, 0, 'A' },
		{ "type",           required_argument, 0, 't' },
		{ "fl2k-audio",     required_argument, 0, _OPT_FL2K_AUDIO },
		{ "showecm",        no_argument,       0, _OPT_SHOW_ECM },
		{ "threads",        no_argument,       0, _OPT_THREADS },
		{ "version",        no_argument,       0, _OPT_VERSION },
		{ 0,                0,                 0,  0  }
	};
	static hacktv_t s;
	const vid_configs_t *vid_confs;
	vid_config_t vid_conf;
	char *pre, *sub;
	int l;
	int r;
	r64_t rn;
	
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
	s.fit_mode = AV_FIT_STRETCH;
	s.repeat = 0;
	s.shuffle = 0;
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
	s.vitc = 0;
	s.cc608 = 0;
	s.filter = 0;
	s.nocolour = 0;
	s.volume = 1.0;
	s.noaudio = 0;
	s.nonicam = 0;
	s.a2stereo = 0;
	s.scramble_video = 0;
	s.scramble_audio = 0;
	s.chid = -1;
	s.mac_audio_stereo = MAC_STEREO;
	s.mac_audio_quality = MAC_HIGH_QUALITY;
	s.mac_audio_companded = MAC_COMPANDED;
	s.mac_audio_protection = MAC_FIRST_LEVEL_PROTECTION;
	s.frequency = 0;
	s.amp = 0;
	s.gain = 0;
	s.antenna = NULL;
	s.showecm = 0;
	s.ec_ppv = NULL;
	s.nodate = 0;
	s.file_type = RF_INT16;
	s.raw_bb_blanking_level = 0;
	s.raw_bb_white_level = INT16_MAX;
	s.fl2k_audio = FL2K_AUDIO_NONE;
	
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
				s.output_type = "soapysdr";
				s.output = sub;
			}
			else if(strcmp(pre, "fl2k") == 0)
			{
				s.output_type = "fl2k";
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
		
		case _OPT_LIST_MODES: /* --list-modes */
			s.list_modes = 1;
			break;
		
		case 's': /* -s, --samplerate <value> */
			
			rn = r64_parse(optarg, NULL);
			if(rn.den == 0)
			{
				fprintf(stderr, "Invalid sample rate\n");
				return(-1);
			}
			
			s.samplerate = (rn.num + rn.den / 2) / rn.den;
			
			break;
		
		case _OPT_PIXELRATE: /* --pixelrate <value> */
			
			rn = r64_parse(optarg, NULL);
			if(rn.den == 0)
			{
				fprintf(stderr, "Invalid pixel rate\n");
				return(-1);
			}
			
			s.pixelrate = (rn.num + rn.den / 2) / rn.den;
			
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
		
		case _OPT_FIT: /* --fit <mode> */
			
			if(strcmp(optarg, "stretch") == 0) s.fit_mode = AV_FIT_STRETCH;
			else if(strcmp(optarg, "fill") == 0) s.fit_mode = AV_FIT_FILL;
			else if(strcmp(optarg, "fit") == 0) s.fit_mode = AV_FIT_FIT;
			else if(strcmp(optarg, "none") == 0) s.fit_mode = AV_FIT_NONE;
			else
			{
				fprintf(stderr, "Unrecognised fit mode '%s'.\n", optarg);
				return(-1);
			}
			
			break;
		
		case _OPT_MIN_ASPECT: /* --min-aspect <value> */
			
			s.min_aspect = r64_parse(optarg, NULL);
			if(s.min_aspect.den == 0)
			{
				fprintf(stderr, "Invalid minimum aspect\n");
				return(-1);
			}
			
			break;
		
		case _OPT_MAX_ASPECT: /* --max-aspect <value> */
			
			s.max_aspect = r64_parse(optarg, NULL);
			if(s.max_aspect.den == 0)
			{
				fprintf(stderr, "Invalid maximum aspect\n");
				return(-1);
			}
			
			break;
		
		case _OPT_LETTERBOX: /* --letterbox */
			
			/* For compatiblity with CJ fork */
			s.fit_mode = AV_FIT_FIT;
			
			break;
		
		case _OPT_PILLARBOX: /* --pillarbox */
			
			/* For compatiblity with CJ fork */
			s.fit_mode = AV_FIT_FILL;
			
			break;
		
		case 'r': /* -r, --repeat */
			s.repeat = 1;
			break;
		
		case _OPT_SHUFFLE: /* --shuffle */
			s.shuffle = 1;
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
		
		case _OPT_SHOW_ECM: /* --showecm */
			s.showecm = 1;
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
		
		case _OPT_VITC: /* --vitc */
			s.vitc = 1;
			break;
		
		case _OPT_CC608: /* --cc608 */
			s.cc608 = 1;
			break;
		
		case _OPT_FILTER: /* --filter */
			s.filter = 1;
			break;
		
		case _OPT_NOCOLOUR: /* --nocolour / --nocolor */
			s.nocolour = 1;
			break;
		
		case _OPT_S_VIDEO: /* --s-video */
			s.s_video = 1;
			break;
		
		case _OPT_VOLUME: /* --volume <value> */
			s.volume = atof(optarg);
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
		
		case _OPT_EC_MAT_RATING: /* --ec-mat-rating */
			s.ec_mat_rating = atoi(optarg);
			break;
		
		case _OPT_EC_PPV: /* --ec-ppv */
			s.ec_ppv = "0,0";
			if(!optarg && NULL != argv[optind] && '-' != argv[optind][0])
			{
				s.ec_ppv = argv[optind++];
			}
			break;
			
		case _OPT_SCRAMBLE_AUDIO: /* --scramble-audio */
			s.scramble_audio = 1;
			break;
		
		case _OPT_CHID: /* --chid <id> */
			s.chid = strtol(optarg, NULL, 0);
			break;
		
		case _OPT_MAC_AUDIO_STEREO: /* --mac-audio-stereo */
			s.mac_audio_stereo = MAC_STEREO;
			break;
		
		case _OPT_MAC_AUDIO_MONO: /* --mac-audio-mono */
			s.mac_audio_stereo = MAC_MONO;
			break;
		
		case _OPT_MAC_AUDIO_HIGH_QUALITY: /* --mac-audio-high-quality */
			s.mac_audio_quality = MAC_HIGH_QUALITY;
			break;
		
		case _OPT_MAC_AUDIO_MEDIUM_QUALITY: /* --mac-audio-medium-quality */
			s.mac_audio_quality = MAC_MEDIUM_QUALITY;
			break;
		
		case _OPT_MAC_AUDIO_COMPANDED: /* --mac-audio-companded */
			s.mac_audio_companded = MAC_COMPANDED;
			break;
		
		case _OPT_MAC_AUDIO_LINEAR: /* --mac-audio-linear */
			s.mac_audio_companded = MAC_LINEAR;
			break;
		
		case _OPT_MAC_AUDIO_L1_PROTECTION: /* --mac-audio-l1-protection */
			s.mac_audio_protection = MAC_FIRST_LEVEL_PROTECTION;
			break;
		
		case _OPT_MAC_AUDIO_L2_PROTECTION: /* --mac-audio-l2-protection */
			s.mac_audio_protection = MAC_SECOND_LEVEL_PROTECTION;
			break;
		
		case _OPT_SIS: /* --sis <mode> */
			s.sis = optarg;
			break;
		
		case _OPT_SWAP_IQ: /* --swap-iq */
			s.swap_iq = 1;
			break;
		
		case _OPT_OFFSET: /* --offset <value Hz> */
			s.offset = (int64_t) strtod(optarg, NULL);
			break;
		
		case _OPT_PASSTHRU: /* --passthru <path> */
			s.passthru = optarg;
			break;
		
		case _OPT_INVERT_VIDEO: /* --invert-video */
			s.invert_video = 1;
			break;
		
		case _OPT_RAW_BB_FILE: /* --raw-bb-file <file> */
			s.raw_bb_file = optarg;
			break;
		
		case _OPT_RAW_BB_BLANKING: /* --raw-bb-blanking <value> */
			s.raw_bb_blanking_level = strtol(optarg, NULL, 0);
			break;
		
		case _OPT_RAW_BB_WHITE: /* --raw-bb-white <value> */
			s.raw_bb_white_level = strtol(optarg, NULL, 0);
			break;
		
		case _OPT_SECAM_FIELD_ID: /* --secam-field-id */
			s.secam_field_id = 1;
			break;
		
		case _OPT_SECAM_FIELD_ID_LINES: /* --secam-field-id-lines <value> */
			s.secam_field_id_lines = strtol(optarg, NULL, 0);
			break;
		
		case _OPT_JSON: /* --json */
			s.json = 1;
			break;
		
		case _OPT_FFMT: /* --ffmt <format> */
			s.ffmt = optarg;
			break;
		
		case _OPT_FOPTS: /* --fopts <option=value:[option2=value...]> */
			s.fopts = optarg;
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
				s.file_type = RF_UINT8;
			}
			else if(strcmp(optarg, "int8") == 0)
			{
				s.file_type = RF_INT8;
			}
			else if(strcmp(optarg, "uint16") == 0)
			{
				s.file_type = RF_UINT16;
			}
			else if(strcmp(optarg, "int16") == 0)
			{
				s.file_type = RF_INT16;
			}
			else if(strcmp(optarg, "int32") == 0)
			{
				s.file_type = RF_INT32;
			}
			else if(strcmp(optarg, "float") == 0)
			{
				s.file_type = RF_FLOAT;
			}
			else
			{
				fprintf(stderr, "Unrecognised file data type.\n");
				return(-1);
			}
			
			break;
		
		case _OPT_FL2K_AUDIO: /* --fl2k-audio <mode> */
			
			if(strcmp(optarg, "none") == 0)
			{
				s.fl2k_audio = FL2K_AUDIO_NONE;
			}
			else if(strcmp(optarg, "stereo") == 0)
			{
				s.fl2k_audio = FL2K_AUDIO_STEREO;
			}
			else if(strcmp(optarg, "spdif") == 0)
			{
				s.fl2k_audio = FL2K_AUDIO_SPDIF;
			}
			else
			{
				fprintf(stderr, "Unrecognised FL2K audio mode.\n");
				return(-1);
			}
			
			break;
		
		case _OPT_THREADS: /* --threads */
			s.threads_test = 1;
			break;
		
		case _OPT_VERSION: /* --version */
			print_version();
			return(0);
		
		case '?':
			print_usage();
			return(0);
		}
	}
	
	if(s.list_modes)
	{
		_list_modes(s.json);
		return(-1);
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
#ifndef _WIN32
	struct sigaction action = { .sa_handler = _sigint_callback_handler };
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGILL, &action, NULL);
	sigaction(SIGFPE, &action, NULL);
	sigaction(SIGSEGV, &action, NULL);
	sigaction(SIGTERM, &action, NULL);
	sigaction(SIGABRT, &action, NULL);
#else
	signal(SIGINT, &_sigint_callback_handler);
	signal(SIGILL, &_sigint_callback_handler);
	signal(SIGFPE, &_sigint_callback_handler);
	signal(SIGSEGV, &_sigint_callback_handler);
	signal(SIGTERM, &_sigint_callback_handler);
	signal(SIGABRT, &_sigint_callback_handler);
#endif
	
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
	
	if(s.s_video)
	{
		if((vid_conf.colour_mode != VID_PAL &&
		   vid_conf.colour_mode != VID_SECAM &&
		   vid_conf.colour_mode != VID_NTSC) ||
		   vid_conf.output_type != RF_INT16_REAL)
		{
			fprintf(stderr, "S-Video is only available with PAL, SECAM, or NTSC baseband modes.\n");
			return(-1);
		}
		
		vid_conf.s_video = 1;
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
		if(vid_conf.type != VID_RASTER_625)
		{
			fprintf(stderr, "WSS is only supported for 625 line raster modes.\n");
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
	
	if(s.eurocrypt)
	{
		if(vid_conf.lines != 625 && vid_conf.colour_mode != VID_MAC)
		{
			fprintf(stderr, "Eurocrypt is only compatible with MAC modes.\n");
			return(-1);
		}
		vid_conf.eurocrypt = s.eurocrypt;
	}
	
	if(s.ec_mat_rating)
	{
		if(!s.eurocrypt)
		{
			fprintf(stderr, "Maturing rating option is only used in conjunction with Eurocrypt.\n");
			return(-1);
		}
		vid_conf.ec_mat_rating = s.ec_mat_rating;
	}
	
	if(s.ec_ppv)
	{
		if(!s.eurocrypt)
		{
			fprintf(stderr, "PPV option is only used in conjunction with Eurocrypt.\n");
			return(-1);
		}
		vid_conf.ec_ppv = s.ec_ppv;
	}
	
	if(s.showecm)
	{
		vid_conf.showecm = s.showecm;
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
	
	if(s.nodate)
	{
		vid_conf.nodate = s.nodate;
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
	
	if(s.vitc)
	{
		if(vid_conf.type != VID_RASTER_625 &&
		   vid_conf.type != VID_RASTER_525)
		{
			fprintf(stderr, "VITC is only currently supported for 625 and 525 line raster modes.\n");
			return(-1);
		}
		
		vid_conf.vitc = 1;
	}
	
	if(s.cc608)
	{
		if(vid_conf.type != VID_RASTER_625 &&
		   vid_conf.type != VID_RASTER_525)
		{
			fprintf(stderr, "CEA/EIA-608 is only currently supported for 625 and 525 line raster modes.\n");
			return(-1);
		}
		
		vid_conf.cc608 = 1;
	}
	
	if(vid_conf.type == VID_MAC)
	{
		if(s.chid >= 0)
		{
			vid_conf.chid = (uint16_t) s.chid;
		}
		
		vid_conf.mac_audio_stereo = s.mac_audio_stereo;
		vid_conf.mac_audio_quality = s.mac_audio_quality;
		vid_conf.mac_audio_protection = s.mac_audio_protection;
		vid_conf.mac_audio_companded = s.mac_audio_companded;
	}
	
	if(s.filter)
	{
		vid_conf.vfilter = 1;
	}
	
	if(s.sis)
	{
		if(vid_conf.lines != 625)
		{
			fprintf(stderr, "SiS is only available with 625 line modes.\n");
			return(-1);
		}
		
		vid_conf.sis = s.sis;
	}
	
	vid_conf.swap_iq = s.swap_iq;
	vid_conf.offset = s.offset;
	vid_conf.passthru = s.passthru;
	vid_conf.volume = s.volume * 256 + 0.5;
	vid_conf.invert_video = s.invert_video;
	vid_conf.raw_bb_file = s.raw_bb_file;
	vid_conf.raw_bb_blanking_level = s.raw_bb_blanking_level;
	vid_conf.raw_bb_white_level = s.raw_bb_white_level;
	vid_conf.secam_field_id = s.secam_field_id;
	vid_conf.secam_field_id_lines = s.secam_field_id_lines;
	
	if(s.threads_test)
	{
		vid_conf.threads_test = 1;
	}
	
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
#ifdef HAVE_HACKRF
		if(rf_hackrf_open(&s.rf, s.output, s.vid.sample_rate, s.frequency, s.gain, s.amp, s.vid.conf.output_type == RF_INT16_REAL) != RF_OK)
		{
			vid_free(&s.vid);
			return(-1);
		}
#else
		fprintf(stderr, "HackRF support is not available in this build of hacktv.\n");
		vid_free(&s.vid);
		return(-1);
#endif
	}
	else if(strcmp(s.output_type, "soapysdr") == 0)
	{
#ifdef HAVE_SOAPYSDR
		if(rf_soapysdr_open(&s.rf, s.output, s.vid.sample_rate, s.frequency, s.gain, s.antenna) != RF_OK)
		{
			vid_free(&s.vid);
			return(-1);
		}
#else
		fprintf(stderr, "SoapySDR support is not available in this build of hacktv.\n");
		vid_free(&s.vid);
		return(-1);
#endif
	}
	else if(strcmp(s.output_type, "fl2k") == 0)
	{
#ifdef HAVE_FL2K
		if(rf_fl2k_open(&s.rf, s.output, s.vid.sample_rate, s.vid.conf.output_type == RF_INT16_REAL && s.vid.conf.s_video == 0, s.fl2k_audio) != RF_OK)
		{
			vid_free(&s.vid);
			return(-1);
		}
#else
		fprintf(stderr, "FL2K support is not available in this build of hacktv.\n");
		vid_free(&s.vid);
		return(-1);
#endif
	}
	else if(strcmp(s.output_type, "file") == 0)
	{
		if(rf_file_open(&s.rf, s.output, s.file_type, s.vid.conf.output_type == RF_INT16_COMPLEX || s.vid.conf.s_video) != RF_OK)
		{
			vid_free(&s.vid);
			return(-1);
		}
	}
	
	av_ffmpeg_init();
	
	/* Configure AV source settings */
	s.vid.av = (av_t) {
		.frame_rate = (r64_t) {
			.num = s.vid.conf.frame_rate.num * (s.vid.conf.interlace ? 2 : 1),
			.den = s.vid.conf.frame_rate.den,
		},
		.display_aspect_ratios = {
			s.vid.conf.frame_aspects[0],
			s.vid.conf.frame_aspects[1]
		},
		.fit_mode = s.fit_mode,
		.min_display_aspect_ratio = s.min_aspect,
		.max_display_aspect_ratio = s.max_aspect,
		.width = s.vid.active_width,
		.height = s.vid.conf.active_lines,
		.sample_rate = (r64_t) { HACKTV_AUDIO_SAMPLE_RATE, 1 },
	};
	
	if((s.vid.conf.frame_orientation & 3) == VID_ROTATE_90 ||
	   (s.vid.conf.frame_orientation & 3) == VID_ROTATE_270)
	{
		/* Flip dimensions if the lines are scanned vertically */
		s.vid.av.width = s.vid.conf.active_lines;
		s.vid.av.height = s.vid.active_width;
	}
	
	do
	{
		if(s.shuffle)
		{
			/* Shuffle the input source list */
			/* Avoids moving the last entry to the start
			 * to prevent it repeating immediately */
			for(c = optind; c < argc - 1; c++)
			{
				l = c + (rand() % (argc - c - (c == optind ? 1 : 0)));
				pre = argv[c];
				argv[c] = argv[l];
				argv[l] = pre;
			}
		}
		
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
				r = av_test_open(&s.vid.av);
			}
			else if(strncmp(pre, "ffmpeg", l) == 0)
			{
				r = av_ffmpeg_open(&s.vid.av, sub, s.ffmt, s.fopts);
			}
			else
			{
				r = av_ffmpeg_open(&s.vid.av, pre, s.ffmt, s.fopts);
			}
			
			if(r != AV_OK)
			{
				/* Error opening this source. Move to the next */
				continue;
			}
			
			while(!_abort)
			{
				vid_line_t *line = vid_next_line(&s.vid);
				
				if(line == NULL) break;
				
				if(rf_write(&s.rf, line->output, line->width) != RF_OK) break;
				if(line->audio_len && rf_write_audio(&s.rf, line->audio, line->audio_len) != RF_OK) break;
			}
			
			if(_signal)
			{
				fprintf(stderr, "Caught signal %d\n", _signal);
				_signal = 0;
			}
			
			av_close(&s.vid.av);
		}
	}
	while(s.repeat && !_abort);
	
	rf_close(&s.rf);
	vid_free(&s.vid);
	
	av_ffmpeg_deinit();
	
	fprintf(stderr, "\n");
	
	return(0);
}

