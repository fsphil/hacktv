/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2020 Alex L. James                                          */
/* Copyright 2020 Philip Heron <phil@sanslogic.co.uk>                    */
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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "video.h"
#include <time.h>

#define ECM  0
#define HASH 1

#define EC_M    0x20
#define EC_S    0x01
#define EC_S2   0x30
#define EC_3DES 0x31

#define ENCRYPT  1
#define DECRYPT 2

enum {
	THEME_ARTS = 0x01,
	THEME_CHILDREN,
	THEME_CLUB,
	THEME_ENTERTAINMENT,
	THEME_FILM,
	THEME_LIFESTYLE,
	THEME_MUSIC,
	THEME_NEWS,
	THEME_SERIES,
	THEME_SPORTS,
	THEME_SPECIAL,
	THEME_NATURE,
	THEME_DOCUMENTARY,
	THEME_MINISERIES,
	THEME_SCIENCE,
	THEME_ALL = 0xFF
};

/* Data for EC controlled-access decoding */
const static ec_mode_t _ec_modes[] = {
/*  |- Mode name -|- Mode -|- Algo -| |------------------- OP KEY --------------------|  |-- PPID/Key num --|  |--- Date ----|   |------- Theme -----| Level |- "Channel name" -| */
	{ "bbcprime",     EC_M,    EC_M, { 0x99, 0x01, 0x00, 0x5C, 0x63, 0xF8, 0x50, 0x00 }, { 0x00, 0x28, 0x18 }, { "TODAY"      }, { THEME_ENTERTAINMENT, 0x04 }, "BBC Prime (M)" },
	{ "cplusfr",      EC_M,    EC_M, { 0xEC, 0xA6, 0xE8, 0x4E, 0x10, 0x41, 0x6F, 0x00 }, { 0x10, 0x00, 0x18 }, { "TODAY"      }, { THEME_FILM,          0x00 }, "Canal+ 4/3 (M)" },
	{ "cplusfr169",   EC_M,    EC_M, { 0x34, 0x94, 0x2B, 0x9B, 0xE5, 0xC1, 0xA2, 0x00 }, { 0x10, 0x00, 0x28 }, { "TODAY"      }, { THEME_FILM,          0x00 }, "Canal+ 16/9 (M)" },
	{ "ctv",          EC_M,    EC_M, { 0x84, 0x66, 0x30, 0xE4, 0xDA, 0xFA, 0x23, 0x00 }, { 0x00, 0x04, 0x38 }, { "02/04/1996" }, { THEME_ENTERTAINMENT, 0x00 }, "CTV (M)" },
	{ "filmnet",      EC_M,    EC_M, { 0x21, 0x12, 0x31, 0x35, 0x8A, 0xC3, 0x4F, 0x00 }, { 0x00, 0x28, 0x08 }, { "TODAY"      }, { THEME_FILM,          0x00 }, "FilmNet (M)" },
	{ "multivisio",   EC_M,    EC_M, { 0xA3, 0x42, 0xC3, 0x9F, 0xED, 0xA4, 0x53, 0x00 }, { 0x00, 0x44, 0x08 }, { "TODAY"      }, { THEME_ENTERTAINMENT, 0x00 }, "Multivisio (M)" },
	{ "tv3",          EC_M,    EC_M, { 0xE9, 0xF3, 0x34, 0x36, 0xB0, 0xBB, 0xF8, 0x00 }, { 0x00, 0x04, 0x0C }, { "TODAY"      }, { THEME_ENTERTAINMENT, 0x00 }, "TV3 (M)" },
	{ "tv1000",       EC_M,    EC_M, { 0x48, 0x63, 0xC5, 0xB3, 0xDA, 0xE3, 0x29, 0x00 }, { 0x00, 0x04, 0x18 }, { "TODAY"      }, { THEME_FILM,          0x00 }, "TV 1000 (M)" },
	{ "tvcable",      EC_M,    EC_M, { 0xDA, 0xCF, 0xEB, 0x94, 0x44, 0x55, 0x56, 0x00 }, { 0x00, 0x0C, 0x09 }, { "TODAY"      }, { THEME_ENTERTAINMENT, 0x00 }, "TV Cable (M)" },
	{ "tvplus",       EC_M,    EC_M, { 0x12, 0x06, 0x28, 0x3A, 0x4B, 0x1D, 0xE2, 0x00 }, { 0x00, 0x2C, 0x08 }, { "TODAY"      }, { THEME_ENTERTAINMENT, 0x00 }, "TVPlus (M)" },
	{ "visiopass",    EC_M,    EC_M, { 0x68, 0x67, 0x24, 0x50, 0xF1, 0x98, 0x72, 0x00 }, { 0x00, 0x24, 0x08 }, { "TODAY"      }, { THEME_ENTERTAINMENT, 0x00 }, "Visiopass (M)" },
	{ "teletv",       EC_S,    EC_S, { 0x72, 0xEE, 0xD1, 0xFA, 0xE5, 0x0E, 0x84, 0xEE }, { 0x00, 0x60, 0x47 }, { "TODAY"      }, { THEME_ENTERTAINMENT, 0x00 }, "Tele-TV (S)" },
	{ "nrk",         EC_S2,    EC_M, { 0xE7, 0x19, 0x5B, 0x7C, 0x47, 0xF4, 0x66, 0x00 }, { 0x47, 0x52, 0x00 }, { "06/02/1999" }, { THEME_ENTERTAINMENT, 0x00 }, "NRK (S2)" },
	{ "tv2",         EC_S2,    EC_M, { 0x70, 0xBF, 0x6E, 0x51, 0x9F, 0xB8, 0xA6, 0x00 }, { 0x47, 0x51, 0x00 }, { "06/02/1999" }, { THEME_ENTERTAINMENT, 0x00 }, "TV2 Norway (S2)" },
	{ "ctvs",        EC_S2,   EC_S2, { 0x17, 0x38, 0xFA, 0x8A, 0x84, 0x5A, 0x5E, 0x00 }, { 0x00, 0x2B, 0x20 }, { "TODAY"      }, { THEME_ENTERTAINMENT, 0x00 }, "CTV (S2)" },
	{ "eros",        EC_S2,   EC_S2, { 0x3E, 0xC5, 0x54, 0x92, 0x61, 0x7D, 0x24, 0x00 }, { 0x00, 0x2E, 0x10 }, { "01/01/2019" }, { THEME_FILM,          0x00 }, "Eros (S2)" },
	{ "rdv",         EC_S2,   EC_S2, { 0x22, 0xC5, 0xC9, 0x22, 0x8D, 0x45, 0x35, 0x00 }, { 0x00, 0x2D, 0x10 }, { "TODAY"      }, { THEME_ENTERTAINMENT, 0x00 }, "RDV (S2)" },
	{ "tvs",         EC_S2,   EC_S2, { 0x5C, 0x8B, 0x11, 0x2F, 0x99, 0xA8, 0x2C, 0x00 }, { 0x00, 0x2B, 0x50 }, { "06/02/1999" }, { THEME_ENTERTAINMENT, 0x00 }, "TV-S (S2)" },
	{ "cplus",     EC_3DES, EC_3DES, { 0x62, 0xA7, 0x01, 0xA0, 0x5E, 0x8B, 0xB9, 0x00,  /* Index key E = Key 02 and key 03 */
	                                   0xCB, 0x86, 0x67, 0x27, 0x5C, 0x53, 0x17, 0x00 }, { 0x00, 0x2B, 0x1E }, { "19/11/1998"  }, { THEME_FILM,         0x00 }, "Canal+ DK (3DES)" },
	{ NULL } 
};

/* Data for EC controlled-access EMMs */
const static em_mode_t _em_modes[] = {
/*  |- Mode name -|- Mode -|- Algo -| |----------------- MGMT KEY --------------------|  |-- PPID/Key num --|  |-- Shared Addr. --|  |--------- Unique Addr. -------|- Type -| */
	{ "bbcprime",     EC_M,    EC_M, { 0x89, 0x6D, 0xAA, 0x83, 0x03, 0x57, 0x16, 0x00 }, { 0x00, 0x28, 0x12 }, { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x00, 0x00, 0x00 }, EMMG },
	{ "cplusfr",	  EC_M,    EC_M, { 0xB2, 0x26, 0xF7, 0x98, 0x36, 0xEB, 0xC8, 0x00 }, { 0x10, 0x00, 0x13 }, { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x00, 0x00, 0x00 }, EMMG },
	{ "cplusfr169",   EC_M,    EC_M, { 0x6B, 0xB7, 0x78, 0x65, 0xA8, 0xC7, 0xF2, 0x00 }, { 0x10, 0x00, 0x23 }, { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x00, 0x00, 0x00 }, EMMG },
	{ "ctv",       	  EC_M,    EC_M, { 0x5E, 0xC9, 0xD7, 0x19, 0x89, 0x64, 0xE6, 0x00 }, { 0x00, 0x04, 0x34 }, { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x00, 0x00, 0x00 }, EMMG },
	{ "filmnet",   	  EC_M,    EC_M, { 0x13, 0x39, 0x6F, 0xDB, 0x3A, 0x88, 0x60, 0x00 }, { 0x00, 0x28, 0x06 }, { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x00, 0x00, 0x00 }, EMMG },
	{ "tv3",       	  EC_M,    EC_M, { 0x31, 0xD4, 0x65, 0x64, 0x15, 0xC8, 0x7B, 0x00 }, { 0x00, 0x04, 0x03 }, { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x00, 0x00, 0x00 }, EMMG },
	{ "tv1000",   	  EC_M,    EC_M, { 0xFA, 0x9B, 0xBF, 0x2C, 0x22, 0x5C, 0x22, 0x00 }, { 0x00, 0x04, 0x13 }, { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x00, 0x00, 0x00 }, EMMG },
	{ "tvcable",	  EC_M,    EC_M, { 0x16, 0x81, 0x15, 0x93, 0xD8, 0xDD, 0x68, 0x00 }, { 0x00, 0x0C, 0x02 }, { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x00, 0x00, 0x00 }, EMMG },
	{ "tvplus",       EC_M,    EC_M, { 0x21, 0xF5, 0x50, 0xAC, 0x0E, 0xF4, 0xA7, 0x00 }, { 0x00, 0x2C, 0x03 }, { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x00, 0x00, 0x00 }, EMMG },
	{ "visiopass",    EC_M,    EC_M, { 0x3B, 0xDD, 0x2C, 0xF3, 0xC1, 0xA0, 0x03, 0x00 }, { 0x00, 0x24, 0x03 }, { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x00, 0x00, 0x00 }, EMMG },
	{ "teletv",       EC_S,    EC_S, { 0x7A, 0x88, 0x42, 0xD3, 0xFC, 0x66, 0xF8, 0x2A }, { 0x00, 0x60, 0x47 }, { 0x00, 0x00, 0x00 }, { 0xCD, 0x1A, 0xFB, 0x0B, 0x00 }, EMMU },
	{ "tv2",         EC_S2,    EC_M, { 0x5B, 0xF7, 0xBF, 0xCF, 0xF7, 0x59, 0xB7, 0x00 }, { 0x47, 0x52, 0x00 }, { 0x00, 0x00, 0x00 }, { 0x3E, 0xE3, 0x8E, 0x06, 0x00 }, EMMU },
	{ "ctvs",      	 EC_S2,   EC_S2, { 0xC2, 0xAC, 0x87, 0xC1, 0xFD, 0x6D, 0x4B, 0x00 }, { 0x00, 0x2B, 0x20 }, { 0xD9, 0x6F, 0x28 }, { 0xBC, 0x61, 0x97, 0x1F, 0x00 }, EMMU },
	{ "rdv",         EC_S2,   EC_S2, { 0xD3, 0x4E, 0xD3, 0x85, 0xC2, 0x0E, 0x13, 0x00 }, { 0x00, 0x2D, 0x80 }, { 0xA6, 0x68, 0x35 }, { 0x05, 0xC2, 0xB9, 0x29, 0x00 }, EMMU },
	{ "cplus",     EC_3DES, EC_3DES, { 0x00, 0x20, 0x20, 0x02, 0x00, 0x02, 0x00, 0x00,
	                                   0x8B, 0xBE, 0xD4, 0x7C, 0xF8, 0x8A, 0x7A, 0x00 }, { 0x00, 0x2B, 0x10 }, { 0x9B, 0x54, 0x3F }, { 0x4D, 0x19, 0x7A, 0x31, 0x00 }, EMMU },
	{ NULL } 
};

/* Initial permutation for Eurocrypt-S2/3DES */
static const uint8_t _ip[] = {
	58, 50, 42, 34, 26, 18, 10, 2,
	60, 52, 44, 36, 28, 20, 12, 4,
	62, 54, 46, 38, 30, 22, 14, 6,
	64, 56, 48, 40, 32, 24, 16, 8,
	57, 49, 41, 33, 25, 17,  9, 1,
	59, 51, 43, 35, 27, 19, 11, 3,
	61, 53, 45, 37, 29, 21, 13, 5,
	63, 55, 47, 39, 31, 23, 15, 7,
};

/* Inverse/final permutation for Eurocrypt-S2/3DES */
static const uint8_t _ipp[] = {
	40, 8, 48, 16, 56, 24, 64, 32,
	39, 7, 47, 15, 55, 23, 63, 31,
	38, 6, 46, 14, 54, 22, 62, 30,
	37, 5, 45, 13, 53, 21, 61, 29,
	36, 4, 44, 12, 52, 20, 60, 28,
	35, 3, 43, 11, 51, 19, 59, 27,
	34, 2, 42, 10, 50, 18, 58, 26,
	33, 1, 41,  9, 49, 17, 57, 25,
};

static const uint8_t _exp[] = {
	32,  1,  2,  3,  4,  5,
	 4,  5,  6,  7,  8,  9,
	 8,  9, 10, 11, 12, 13,
	12, 13, 14, 15, 16, 17,
	16, 17, 18, 19, 20, 21,
	20, 21, 22, 23, 24, 25,
	24, 25, 26, 27, 28, 29,
	28, 29, 30, 31, 32,  1
};

static const uint8_t _sb[][64] = {
	{ 0xE, 0x0, 0x4, 0xF, 0xD, 0x7, 0x1, 0x4,
	  0x2, 0xE, 0xF, 0x2, 0xB, 0xD, 0x8, 0x1,
	  0x3, 0xA, 0xA, 0x6, 0x6, 0xC, 0xC, 0xB,
	  0x5, 0x9, 0x9, 0x5, 0x0, 0x3, 0x7, 0x8,
	  0x4, 0xF, 0x1, 0xC, 0xE, 0x8, 0x8, 0x2,
	  0xD, 0x4, 0x6, 0x9, 0x2, 0x1, 0xB, 0x7,
	  0xF, 0x5, 0xC, 0xB, 0x9, 0x3, 0x7, 0xE,
	  0x3, 0xA, 0xA, 0x0, 0x5, 0x6, 0x0, 0xD
	},
	{ 0xF, 0x3, 0x1, 0xD, 0x8, 0x4, 0xE, 0x7,
	  0x6, 0xF, 0xB, 0x2, 0x3, 0x8, 0x4, 0xE,
	  0x9, 0xC, 0x7, 0x0, 0x2, 0x1, 0xD, 0xA,
	  0xC, 0x6, 0x0, 0x9, 0x5, 0xB, 0xA, 0x5,
	  0x0, 0xD, 0xE, 0x8, 0x7, 0xA, 0xB, 0x1,
	  0xA, 0x3, 0x4, 0xF, 0xD, 0x4, 0x1, 0x2,
	  0x5, 0xB, 0x8, 0x6, 0xC, 0x7, 0x6, 0xC,
	  0x9, 0x0, 0x3, 0x5, 0x2, 0xE, 0xF, 0x9
	},
	{ 0xA, 0xD, 0x0, 0x7, 0x9, 0x0, 0xE, 0x9,
	  0x6, 0x3, 0x3, 0x4, 0xF, 0x6, 0x5, 0xA,
	  0x1, 0x2, 0xD, 0x8, 0xC, 0x5, 0x7, 0xE,
	  0xB, 0xC, 0x4, 0xB, 0x2, 0xF, 0x8, 0x1,
	  0xD, 0x1, 0x6, 0xA, 0x4, 0xD, 0x9, 0x0,
	  0x8, 0x6, 0xF, 0x9, 0x3, 0x8, 0x0, 0x7,
	  0xB, 0x4, 0x1, 0xF, 0x2, 0xE, 0xC, 0x3,
	  0x5, 0xB, 0xA, 0x5, 0xE, 0x2, 0x7, 0xC
	},
	{ 0x7, 0xD, 0xD, 0x8, 0xE, 0xB, 0x3, 0x5,
	  0x0, 0x6, 0x6, 0xF, 0x9, 0x0, 0xA, 0x3,
	  0x1, 0x4, 0x2, 0x7, 0x8, 0x2, 0x5, 0xC,
	  0xB, 0x1, 0xC, 0xA, 0x4, 0xE, 0xF, 0x9,
	  0xA, 0x3, 0x6, 0xF, 0x9, 0x0, 0x0, 0x6,
	  0xC, 0xA, 0xB, 0x1, 0x7, 0xD, 0xD, 0x8,
	  0xF, 0x9, 0x1, 0x4, 0x3, 0x5, 0xE, 0xB,
	  0x5, 0xC, 0x2, 0x7, 0x8, 0x2, 0x4, 0xE
	},
	{ 0x2, 0xE, 0xC, 0xB, 0x4, 0x2, 0x1, 0xC,
	  0x7, 0x4, 0xA, 0x7, 0xB, 0xD, 0x6, 0x1,
	  0x8, 0x5, 0x5, 0x0, 0x3, 0xF, 0xF, 0xA,
	  0xD, 0x3, 0x0, 0x9, 0xE, 0x8, 0x9, 0x6,
	  0x4, 0xB, 0x2, 0x8, 0x1, 0xC, 0xB, 0x7,
	  0xA, 0x1, 0xD, 0xE, 0x7, 0x2, 0x8, 0xD,
	  0xF, 0x6, 0x9, 0xF, 0xC, 0x0, 0x5, 0x9,
	  0x6, 0xA, 0x3, 0x4, 0x0, 0x5, 0xE, 0x3
	},
	{ 0xC, 0xA, 0x1, 0xF, 0xA, 0x4, 0xF, 0x2,
	  0x9, 0x7, 0x2, 0xC, 0x6, 0x9, 0x8, 0x5,
	  0x0, 0x6, 0xD, 0x1, 0x3, 0xD, 0x4, 0xE,
	  0xE, 0x0, 0x7, 0xB, 0x5, 0x3, 0xB, 0x8,
	  0x9, 0x4, 0xE, 0x3, 0xF, 0x2, 0x5, 0xC,
	  0x2, 0x9, 0x8, 0x5, 0xC, 0xF, 0x3, 0xA,
	  0x7, 0xB, 0x0, 0xE, 0x4, 0x1, 0xA, 0x7,
	  0x1, 0x6, 0xD, 0x0, 0xB, 0x8, 0x6, 0xD
	},
	{ 0x4, 0xD, 0xB, 0x0, 0x2, 0xB, 0xE, 0x7,
	  0xF, 0x4, 0x0, 0x9, 0x8, 0x1, 0xD, 0xA,
	  0x3, 0xE, 0xC, 0x3, 0x9, 0x5, 0x7, 0xC,
	  0x5, 0x2, 0xA, 0xF, 0x6, 0x8, 0x1, 0x6,
	  0x1, 0x6, 0x4, 0xB, 0xB, 0xD, 0xD, 0x8,
	  0xC, 0x1, 0x3, 0x4, 0x7, 0xA, 0xE, 0x7,
	  0xA, 0x9, 0xF, 0x5, 0x6, 0x0, 0x8, 0xF,
	  0x0, 0xE, 0x5, 0x2, 0x9, 0x3, 0x2, 0xC
	},
	{ 0xD, 0x1, 0x2, 0xF, 0x8, 0xD, 0x4, 0x8,
	  0x6, 0xA, 0xF, 0x3, 0xB, 0x7, 0x1, 0x4,
	  0xA, 0xC, 0x9, 0x5, 0x3, 0x6, 0xE, 0xB,
	  0x5, 0x0, 0x0, 0xE, 0xC, 0x9, 0x7, 0x2,
	  0x7, 0x2, 0xB, 0x1, 0x4, 0xE, 0x1, 0x7,
	  0x9, 0x4, 0xC, 0xA, 0xE, 0x8, 0x2, 0xD,
	  0x0, 0xF, 0x6, 0xC, 0xA, 0x9, 0xD, 0x0,
	  0xF, 0x3, 0x3, 0x5, 0x5, 0x6, 0x8, 0xB
	}
};

/* System S seems to use a different S-box table */
static const uint8_t _ss_sb[] = {
	0xEC,0x16,0x6E,0x46,0x3B,0x96,0x70,0x32,0x54,0x20,0x4F,0x78,0x5A,0x4D,0x01,0xC1,
	0x9E,0xD9,0x35,0xEF,0xBA,0x5F,0xA5,0x7F,0x19,0x72,0xE2,0x31,0xA0,0x3E,0xEC,0x3A,
	0xE1,0x73,0x8D,0x13,0x52,0x1F,0xF3,0xE0,0x90,0x28,0xD0,0xD3,0x30,0x09,0x6B,0x8F,
	0x33,0x9D,0xA7,0xEB,0x90,0x7D,0x3D,0xBF,0x26,0x20,0xBB,0x2B,0xAE,0x84,0xB0,0x77,
	0xDB,0x1C,0xB0,0xEF,0x6A,0x91,0xD8,0x36,0x3F,0x65,0x81,0x0C,0x82,0xC5,0xD4,0x1B,
	0x04,0x15,0xB2,0x0D,0x0E,0x1A,0x2B,0xC0,0xCA,0x67,0x1B,0xF7,0x8F,0x31,0x0D,0x05,
	0x2C,0x92,0xDF,0xD3,0xFA,0xB9,0xB7,0xE3,0x55,0x03,0x12,0x29,0x93,0xD7,0x43,0x87,
	0xFF,0xA1,0x4C,0x9B,0xB0,0xC4,0x11,0x59,0xE8,0xC6,0xF2,0x57,0x14,0x63,0x42,0xE0,
	0xAB,0xB4,0xC2,0xF0,0x34,0x02,0x11,0x59,0x8E,0x86,0x6B,0xCE,0xAF,0xF1,0xA2,0x95,
	0x79,0x5A,0x66,0x8F,0x88,0x4B,0x4E,0x0B,0xC9,0xCC,0x7A,0x89,0xAA,0x41,0x64,0xAB,
	0xB6,0xB8,0x51,0x10,0x1D,0x68,0x2A,0x65,0xF5,0xF4,0x43,0xA9,0x2F,0x5D,0x65,0x4A,
	0x7B,0xE9,0x40,0x6D,0x45,0x97,0xF3,0xC3,0x69,0xD1,0xFE,0xE6,0xD0,0x64,0x08,0x83,
	0x5B,0xF6,0xED,0x2E,0x99,0xE7,0x0F,0x74,0x37,0x24,0xD8,0x07,0x23,0x71,0xA4,0x5C,
	0x9A,0xA3,0x05,0x53,0xFB,0xBE,0x5E,0x1E,0x17,0xB3,0x88,0x0F,0xD2,0x7C,0xCB,0x59,
	0x0A,0x22,0xCD,0x61,0x6C,0xEE,0xAC,0x7E,0x75,0x8A,0x76,0x94,0x27,0xFC,0x47,0xBD,
	0x60,0x3B,0xDD,0x56,0x4D,0x58,0x44,0xEA,0x67,0x3C,0x46,0xAD,0x62,0xD5,0x46,0x21
};

static const uint8_t _ss_data[] = {
	0x89, 0xAB, 0xCD, 0xEF, 0xFE, 0xDC, 0xBA, 0x98
};

static const uint8_t _perm[] = {
	16,  7, 20, 21,
	29, 12, 28, 17,
	 1, 15, 23, 26,
	 5, 18, 31, 10,
	 2,  8, 24, 14,
	32, 27,  3,  9,
	19, 13, 30,  6,
	22, 11,  4, 25
};

/* Inverse PC1 table */
static const uint8_t _ipc1[] = {
	8, 16, 24, 56, 52, 44, 36, 57, 
	7, 15, 23, 55, 51, 43, 35, 58, 
	6, 14, 22, 54, 50, 42, 34, 59, 
	5, 13, 21, 53, 49, 41, 33, 60, 
	4, 12, 20, 28, 48, 40, 32, 61, 
	3, 11, 19, 27, 47, 39, 31, 62, 
	2, 10, 18, 26, 46, 38, 30, 63, 
	1,  9, 17, 25, 45, 37, 29, 64
};

static const uint8_t _pc2[] = {
	14, 17, 11, 24,  1,  5,
	 3, 28, 15,  6, 21, 10,
	23, 19, 12,  4, 26,  8,
	16,  7, 27, 20, 13,  2,
	41, 52, 31, 37, 47, 55,
	30, 40, 51, 45, 33, 48,
	44, 49, 39, 56, 34, 53,
	46, 42, 50, 36, 29, 32
};

/* Triple DES key map table */
static const uint8_t _tdesmap[4][2] = {
	{ 0x00, 0x01 }, /* Index C */
	{ 0x01, 0x02 }, /* Index D */
	{ 0x02, 0x03 }, /* Index E */
	{ 0x03, 0x00 }  /* Index F */
};

static const uint8_t _lshift[] = {
	1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1
};

static uint8_t flag = 0;

static void _permute_ec(uint8_t *data, const uint8_t *pr, int n)
{
	uint8_t pin[8];
	int i, j, k, t;
	
	for(i = 0, k = 0; k < n; i++)
	{
		int p = 0;
		
		for(j = 7; j >= 0; j--, k++)
		{
			t = pr[k] - 1;
			p = (p << 1) + ((data[t >> 3]) >> (7 - (t & 7)) & 1);
		}
		
		pin[i] = p;
	}
	
	memcpy(data, pin, 8);
}

uint16_t _get_ec_date(const char *dtm, int mode)
{
	int day, mon, year;
	
	uint16_t date = 0;
	
	sscanf(dtm, "%d/%d/%d", &day, &mon, &year);
	
	/* EC-M, EC_S and EC-S2/3DES have different date byte structures */
	if(mode == EC_M)
	{
		date  = (year - 1980) << 9; /* Year - first 7 bits */
		date |= mon << 5;           /* Month - next 4 bits */
		date |= day << 0;           /* Day - next 5 bits */
	}
	else if(mode == EC_S)
	{
		date  = (year % 10) << 12;	 /* Year - first 4 bits (max 1999 in EC-S) */
		date |= mon << 8;            /* Month - next 4 bits */
		date |= day << 0;            /* Day - next 8 bits */
	}
	else if(mode == EC_S2)
	{
		uint8_t y;
	
		if(year > 2029)
		{
			y  = 3 << 5;
			y |= (year - 1990 - (10 * 3));
		}
		else
		{
			uint8_t ydiff;

			ydiff = (year - 1990) / 10;
			y  = ydiff << 5;
			y |= (year - 1990 - (10 * ydiff));
		}

		date  = y << 12;  	/* Year - first 4 bits */
		date |= mon << 8;   /* Month - next 4 bits */
		date |= y & 0xE0;	/* Year adjustment - 2 bits */
		date |= day << 0;   /* Day - 5 bits */
	}

	return (date);
}

static uint64_t _ec_des_f(uint64_t r, uint8_t *k2)
{
	int i, k;
	uint64_t s = 0, result = 0;
	
	for(i = 0, k = 0; i < 8; i++)
	{
		int j;
		uint8_t v = 0;
		
		/* The expansion E (R1) */
		for(j = 0; j < 6; j++, k++)
		{
			v |= (r >> (32 - _exp[k]) & 1) << (5 - j);
		}
		
		/* Create R2 */
		v ^= k2[i];
		
		/* The S-boxes */
		s |= (uint64_t) _sb[i][v] << (28 - 4 * i);
	}
	
	/* The permutation P (R3) */
	for(i = 0; i < 32; i++)
	{
		result |= (s >> (32 - _perm[i]) & 1) << (31 - i);
	}
	
	return(result);
}

static void _key_rotate_ec(uint64_t *c, uint64_t *d, int dir, int iter)
{
	int i;
	
	/* Rotate left (decryption) */
	if(dir == ENCRYPT)
	{
		for(i = 0; i < _lshift[iter]; i++)
		{
			*c = ((*c << 1) ^ (*c >> 27)) & 0xFFFFFFFL;
			*d = ((*d << 1) ^ (*d >> 27)) & 0xFFFFFFFL;
		}
	}
	/* Rotate right (encryption) */
	else
	{
		for(i = 0; i < _lshift[15 - iter]; i++)
		{
			*c = ((*c >> 1) ^ (*c << 27)) & 0xFFFFFFFL;
			*d = ((*d >> 1) ^ (*d << 27)) & 0xFFFFFFFL;
		}
	}
}

static void _key_exp(uint64_t *c, uint64_t *d, uint8_t *k2)
{
	int j, k;
	
	/* Key expansion */
	for(j = 0, k = 0; j < 8; j++)
	{
		int t;
		
		k2[j] = 0;
		
		for(t = 0; t < 6; t++, k++)
		{
			if(_pc2[k] < 29)
			{
				k2[j] |= (*c >> (28 - _pc2[k]) & 1) << (5 - t);
			}
			else
			{
				k2[j] |= (*d >> (56 - _pc2[k]) & 1) << (5 - t);
			}
		}
	}
}

static void _eurocrypt_system_s(uint8_t *in, const uint8_t *k)
{
	int d, i, round, pl_byte, y;

	uint8_t b, sl, sr, c, xor, tmp[8], key[8];

	uint8_t data[39], ss_buffer[16];

	memcpy(data, in, 39);

	for(d = round = 0; round < 8; round++)
	{
		/* Copy keys to local buffers */
		for(i = 0; i < 8; i++)
		{
			key[i] = k[i];
			tmp[i] = _ss_data[i];
		}

		for(sl = sr = pl_byte = 0; pl_byte < 0x27; pl_byte++)
		{

			for(int i = 0; i < 8; i++)
			{
			ss_buffer[i] = _ss_sb[(uint8_t) (key[i] + tmp[i])];
			}

			/* Straight permutation(?) */
			for(i = 8; i > 0; i--)
			{
			for(y = 7; y >= 0 ; y--)
			{
				c = (ss_buffer[y] >> 7) & 1;
				ss_buffer[y] <<= 1;
				
				ss_buffer[7 + i] <<= 1;
				ss_buffer[7 + i]  |= c;
			}
			}

			for(xor = i = 0; i < 8; i++)
			{
			ss_buffer[i] = _ss_sb[ss_buffer[8 + i]];
			xor |= ss_buffer[i] & (1 << i);
			}

			xor = _ss_sb[(uint8_t) (sl + sr + xor)];

			sl = _ss_sb[(ss_buffer[2] & 0x03) | (ss_buffer[1] & 0xFC)];
			sr = _ss_sb[(ss_buffer[6] & 0x3F) | (ss_buffer[5] & 0xC0)];

			/* Rotate key/buffer */
			for(i = 6; i >= 0; i--)
			{
			tmp[i + 1] = tmp[i];
			}

			if((ss_buffer[6] >> 7) & 1)
			{
			b = key[0];

			for(i = 0; i < 7; i++)
			{
				key[i] = key[i + 1];
			}

			key[7] = b;
			}

			data[d] ^= xor;
			tmp[0] = data[d];

			d = (round & 1 ? d - 1 : d + 1);
		}
		d = (round & 1 ? d + 1 : d - 1);
	}

	/* Rearrange encrypted payload back-to-front */
	for(i = 0; i < 0x13; i++)
	{
		b = data[i];
		data[i] = data[0x26 - i];
		data[0x26 - i] = b;
	}

	memcpy(in, data, 39);
}

static void _eurocrypt(uint8_t *data, const uint8_t *key, int desmode, int des_algo, int rnd)
{
	int i;
	uint64_t r, l, c, d, s;
	
	/* Key preparation. Split key into two halves */
	c = ((uint64_t) key[0] << 20)
	  ^ ((uint64_t) key[1] << 12)
	  ^ ((uint64_t) key[2] << 4)
	  ^ ((uint64_t) key[3] >> 4);
	
	d = ((uint64_t) (key[3] & 0x0F) << 24)
	  ^ ((uint64_t) key[4] << 16)
	  ^ ((uint64_t) key[5] << 8)
	  ^ ((uint64_t) key[6] << 0);
	
	/* Initial permutation for Eurocrypt S2/3DES  - always do this */
	if(des_algo != EC_M)
	{
		_permute_ec(data, _ip, 64);
	}
	
	/* Control word preparation. Split CW into two halves. */
	for(i = 3, l = 0, r = 0; i >= 0; i--)
	{
		l ^= (uint64_t) data[3 - i] << (8 * i);
		r ^= (uint64_t) data[7 - i] << (8 * i);
	}
	
	/* 16 iterations */
	for(i = 0; i < 16; i++)
	{
		uint64_t r3;
		uint8_t k2[8];
		switch (des_algo) {
			/* If mode is not valid, abort -- this is a bug! */
			default:
				fprintf(stderr, "_eurocrypt: BUG: invalid encryption mode!!!\n");
				assert(0);
				break;

			/* EC-M */
			case EC_M:
			case EC_S:
				{
					if(desmode == HASH)
					{
						_key_rotate_ec(&c, &d, ENCRYPT, i);
					}
					
					/* Key expansion */
					_key_exp(&c, &d, k2);
					
					/* One DES round */
					s = _ec_des_f(r, k2);
					
					if(desmode != HASH)
					{
						_key_rotate_ec(&c, &d, DECRYPT, i);
					}
					
					/* Swap first two bytes if it's a hash routine */
					if(desmode == HASH)
					{
						s = ((s >> 8) & 0xFF0000L) | ((s << 8) & 0xFF000000L) | (s & 0x0000FFFFL);
					}
				}
				break;

			/* EC-S2 */
			case EC_S2:
				{
					/* Key rotation */
					_key_rotate_ec(&c, &d, ENCRYPT, i);
					
					/* Key expansion */
					_key_exp(&c, &d, k2);
					
					/* One DES round */
					s = _ec_des_f(r, k2);
				}
				break;

			/* EC-3DES */
			case EC_3DES:
				{
					/* Key rotation */
					if(rnd != 2)
					{
						_key_rotate_ec(&c, &d, ENCRYPT, i);
					}
					
					/* Key expansion */
					_key_exp(&c, &d, k2);
					
					/* One DES round */
					s = _ec_des_f(r, k2);
					
					if(rnd == 2)
					{
						_key_rotate_ec(&c, &d, DECRYPT, i);
					}
				}
				break;
		}
		
		/* Rotate halves around */
		r3 = l ^ s;
		l = r;
		r = r3;
	}
	
	/* Put everything together */
	for(i = 3; i >= 0; i--)
	{
		data[3 - i] = r >> (8 * i);
		data[7 - i] = l >> (8 * i);
	}
	
	/* Final permutation for Eurocrypt S2/3DES */
	if(des_algo != EC_M)
	{
		_permute_ec(data, _ipp, 64);
	}
}

static void _calc_ec_hash(uint8_t *hash, uint8_t *msg, int mode, int msglen, const uint8_t *key)
{
	int i, r;
	
	/* Iterate through data */
	for(i = 0; i < msglen; i++)
	{
		hash[(i % 8)] ^= msg[i];
		
		if(i % 8 == 7)
		{
			/* Three rounds for 3DES mode, one round for others */
			for(r = 0; r < (mode != EC_3DES ? 1 : 3); r++) 
			{
				/* Use second key on second round in 3DES */
				_eurocrypt(hash, key + (r != 1 ? 0 : 8), HASH, mode, r + 1);
			}
		}
	}
	
	/* Final interation - EC-M only */
	if(mode == EC_M)
	{
		_eurocrypt(hash, key, HASH, mode, 1);
	}
}

static void _build_ecm_hash_data(uint8_t *hash, const eurocrypt_t *e, int x)
{
	uint8_t msg[MAC_PAYLOAD_BYTES];
	int msglen;
	
	/* Clear hash memory */
	memset(hash, 0, 8);
	
	/* Build the hash message */
	msglen = 0;
	
	/* EC-S2 and EC-3DES */
	if(e->mode->des_algo != EC_M)
	{
		/* Copy PPID */
		memcpy(msg, e->ecm_pkt + 5, 3);
		msglen += 3;
		
		/* Third byte of PPUA contains key index, which needs to be masked for hashing */
		msg[2] &= 0xF0;
		
		/* Copy E1 04 data + 0xEA */
		memcpy(msg + msglen, e->ecm_pkt + (x - 24), 5);
		msglen += 5;
		
		/* Copy CWs */
		memcpy(msg + msglen, e->ecw[0], 8); msglen += 8;
		memcpy(msg + msglen, e->ecw[1], 8); msglen += 8;
	}
	else
	{
		/* EC-M */
		memcpy(msg, e->ecm_pkt + 8, x - 10);
		msglen += x - 10;
	}
	
	/* Calculate hash */
	_calc_ec_hash(hash, msg, e->mode->des_algo, msglen, e->mode->key);
}

static void _build_emmg_hash_data(uint8_t *hash, eurocrypt_t *e, int x)
{
	uint8_t msg[MAC_PAYLOAD_BYTES];
	int msglen;
	
	/* Clear hash memory */
	memset(hash, 0, 8);
	
	/* Build the hash data */
	msglen = 0;
	
	/* Copy entitlements into data buffer */
	memcpy(msg, e->emmg_pkt + 8, x); msglen += x - 10;
	_calc_ec_hash(hash, msg, e->mode->des_algo, msglen, e->emmode->key);
}

static void _build_emms_hash_data(uint8_t *hash, eurocrypt_t *e)
{
	uint8_t msg[MAC_PAYLOAD_BYTES];
	int msglen;
	
	/* Clear hash memory */
	memset(hash, 0, 8);
	
	/* Build the hash data */
	msglen = 0;
	
	if(e->emmode->des_algo == EC_M)
	{
		/* Copy card's Shared Address into hash buffer */
		hash[5] = e->emmode->sa[2];
		hash[6] = e->emmode->sa[1];
		hash[7] = e->emmode->sa[0];
		
		/* Do the initial hashing of the buffer */
		_eurocrypt(hash, e->emmode->key, HASH, e->mode->des_algo, 1);
		
		/* Copy ADF into data buffer */
		msg[msglen++] = 0x9e;
		msg[msglen++] = 0x20;
		memcpy(msg + msglen, e->emms_pkt + 6, 32); msglen += 32;
		
		/* Hash it */
		_calc_ec_hash(hash, msg, e->mode->des_algo, msglen, e->emmode->key);
		
		msglen = 0;
		
		/* Copy entitlements into data buffer */
		memcpy(msg + msglen, e->emmg_pkt + 8, 15); msglen += 15;
	}
	else
	{
		/* Copy ADF into data buffer */
		memcpy(msg + msglen, e->emms_pkt + 6, 35); msglen += 35;
		memset(msg + msglen, 0xFF, 5); msglen += 5;
	}
	
	/* Final hash */
	_calc_ec_hash(hash, msg, e->emmode->des_algo, msglen, e->emmode->key);
}

char *_get_sub_date(int b, const char *date)
{
	const int months[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	int d, m, y;
	char *dtm;
	
	dtm = malloc(sizeof(char) * 24);
	
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	
	m = tm.tm_mon + 1;
	y = tm.tm_year + 1900;
	
	if(strcmp(date, "TODAY") != 0)
	{
		sscanf(date, "%d/%d/%d", &d, &m, &y);
	}
	
	switch(b)
	{
		/* Today's day */
		case 0:
			d = tm.tm_mday;
			break;
			
		/* Last day in a month */
		case 31:
			d = months[m - 1];
			break;
		
		/* Default to passed value */
		default:
			d = b > 0 && b <= 31 ? b : 1;
			break;
	}
	
	sprintf(dtm, "\n%02d/%02d/%02d\n", d, m, y);
	
	return (dtm);
}

static void _encrypt_opkey(uint8_t *data, eurocrypt_t *e, int t)
{
	int r;
	uint8_t *emm = malloc(sizeof(e->mode->key) / sizeof(uint8_t));
	
	memset(emm, 0, 8);

	/* Pick the right key */
	if(e->mode->des_algo == EC_3DES)
	{
		memcpy(emm, e->mode->key + (t ? 8 : 0), 8);
	}
	else
	{
		memcpy(emm, e->mode->key, 8);
	}
	
	/* Do inverse permuted choice permutation for EC-S2/3DES keys */
	if(e->emmode->des_algo != EC_M) 
	{
		_permute_ec(emm, _ipc1, 64);
	}

	
	/* Three rounds for 3DES mode, one round for others */
	for(r = 0; r < (e->emmode->des_algo != EC_3DES ? 1 : 3); r++)
	{
		/* Use second key on second round in 3DES */
		_eurocrypt(emm, e->emmode->key + (r != 1 ? 0 : 8), ECM, e->emmode->des_algo, r + 1);
	}
	
	memcpy(data, emm, 8);
}

static void _encrypt_date(uint8_t *outdata, eurocrypt_t *e, uint8_t indata[8])
{
	int r;

	if(e->emmode->des_algo == EC_3DES)
	{
		/* Three rounds for 3DES mode, one round for others */
		for(r = 0; r < (e->emmode->des_algo != EC_3DES ? 1 : 3); r++)
		{
			/* Use second key on second round in 3DES */
			_eurocrypt(indata, e->emmode->key + (r != 1 ? 0 : 8), ECM, e->emmode->des_algo, r + 1);
		}
	}
	
	memcpy(outdata, indata, 8);
}

static void _reverse_byte(uint8_t *out, uint8_t *in)
{
	int i;
	for(i = 0; i < 8; i++)
	{
		out[i] = in[7 - i];
	}

}

static uint8_t _update_ecm_packet_ec_s(eurocrypt_t *e)
{
	int x;
	uint8_t *pkt = e->ecm_pkt;
	
	memset(pkt, 0, MAC_PAYLOAD_BYTES * 2);
	
	x = 0;
	pkt[x++] = 0x00; /* ?? */
	pkt[x++] = 0x00; /* ?? */
	pkt[x++] = 0x60; /* Provider ID */
	pkt[x++] = 0x47; /* Provider ID */
	pkt[x++] = 0x00; /* ?? */

	uint16_t d = _get_ec_date(strcmp(e->mode->date, "TODAY") == 0 ? _get_sub_date(0, e->mode->date) : e->mode->date, e->mode->des_algo);
	pkt[x++] = (d & 0xFF00) >> 8;
	pkt[x++] = (d & 0x00FF) >> 0;

	/* Plain text packet - always 39 bytes long */
	pkt[x++] = 0x00;	/* Always 0 */
	pkt[x++] = 0x00;	/* No idea what these are */
	pkt[x++] = 0x00;	/* .. */
	pkt[x++] = 0x00;	/* .. */
	pkt[x++] = 0x00;	/* .. */
	pkt[x++] = 0x00;	/* .. */
	pkt[x++] = 0xFF;	/* Tiers */
	pkt[x++] = 0xFF;    /* .. */
	pkt[x++] = 0xFF;	/* .. */
	pkt[x++] = 0xFF;	/* .. */
	pkt[x++] = 0xFF;	/* .. */
	pkt[x++] = 0x00;	/* No idea what these are */
	pkt[x++] = 0x00;	/* .. */
	pkt[x++] = 0x00;	/* .. */
	pkt[x++] = 0x00;	/* .. */
	
	/* Even/Odd CW */

	/* CW bytes are sent in reverse order */
	_reverse_byte(e->cw[0], e->ecw[0]);
	_reverse_byte(e->cw[1], e->ecw[1]);

	memcpy(&pkt[x], e->ecw[1], 8); x += 8; /* OCW */
	memcpy(&pkt[x], e->ecw[0], 8); x += 8; /* ECW */

	pkt[x++] = 0xAE;

	pkt[x++] = pkt[1];	/* Must match the packet header */
	pkt[x++] = pkt[2];	/* .. */
	pkt[x++] = pkt[3];	/* .. */
	pkt[x++] = pkt[4];	/* .. */
	pkt[x++] = pkt[5];	/* .. */
	pkt[x++] = pkt[6];	/* .. */
	pkt[x++] = pkt[7];	/* .. */

	_eurocrypt_system_s(&pkt[x - 39], e->mode->key);
	
	return (x / ECM_PAYLOAD_BYTES);
}


static uint8_t _update_ecm_packet(eurocrypt_t *e, int t, int m, char *ppv, int nd)
{
	int x;
	uint16_t b;
	uint8_t *pkt = e->ecm_pkt;
	
	memset(pkt, 0, MAC_PAYLOAD_BYTES * 2);
	
	/* PT - always 0x00 for ECM */
	x = 0;
	pkt[x++] = ECM;
	
	/* Command Identifier, CI */
	b  = (e->mode->packet_type & 0x30) << 2; /* Crypto-algo type */
	b |= 1 << 1;                       /* Format bit - always 1 */
	b |= t << 0;                       /* Toggle bit */
	pkt[x++] = b;
	
	/* Command Length Indicator, CLI -- updated later */
	pkt[x++] = 0;
	
	/* PPID */
	pkt[x++] = 0x90; /* PI */
	pkt[x++] = 0x03; /* LI */
	memcpy(&pkt[x], e->mode->ppid, 3); x += 3;
	
	/* Undocumented meaning of this byte but appears to be padding */
	pkt[x++] = 0xDF; /* PI */
	pkt[x++] = 0x00; /* LI */
	
	if(e->mode->des_algo == EC_M)
	{
		/* CTRL */
		pkt[x++] = 0xE0;
		pkt[x++] = 0x01;
		b  = m ? 1 : 0 << 6;	/* Enable maturity rating */
		b = (nd ? 1 : 0) << 5;	/* Date verification */
		b |= m ? m : 0;			/* Maturity rating (4 bits) */
		pkt[x++] = b;
	}
	
	if(ppv != NULL)
	{
		uint32_t ppvi[2] = { 0, 0 };
		int i = 0;
		
		char tokens[0x7F];
		strcpy(tokens, ppv);
		
		char *ptr = strtok(tokens, ",");
		
		while(ptr != NULL)
		{
			ppvi[i++] = (uint32_t) atof(ptr);
			ptr = strtok(NULL, ",");
		}
		
		pkt[x++] = 0xE4;
		pkt[x++] = 0x05;
		pkt[x++] = (ppvi[0] >> 16) & 0xFF;
		pkt[x++] = (ppvi[0] >>  8) & 0xFF;
		pkt[x++] = (ppvi[0] >>  0) & 0xFF;
		pkt[x++] = ppvi[1] & 0xFF;
		pkt[x++] = 0x00;
	}
	else
	{
		/* CDATE + THEME/LEVEL */
		pkt[x++] = 0xE1; /* PI */
		pkt[x++] = 0x04; /* LI */
		uint16_t d = _get_ec_date(strcmp(e->mode->date, "TODAY") == 0 ? _get_sub_date(0, e->mode->date) : e->mode->date, e->mode->des_algo);
		pkt[x++] = (d & 0xFF00) >> 8;
		pkt[x++] = (d & 0x00FF) >> 0;
		memcpy(&pkt[x], e->mode->theme, 2); x += 2;
	}
	
	/* ECW/OCW */
	pkt[x++] = 0xEA; /* PI */
	pkt[x++] = 0x10; /* LI */
	memcpy(&pkt[x], e->ecw[0], 8); x += 8; /* ECW */
	memcpy(&pkt[x], e->ecw[1], 8); x += 8; /* OCW */
	
	/* HASH */
	pkt[x++] = 0xF0; /* PI */
	pkt[x++] = 0x08; /* LI */
	_build_ecm_hash_data(&pkt[x], e, x); x += 8;
	memcpy(e->ecm_hash, &pkt[x - 8], 8);
	
	/* Update the CI command length */
	pkt[2] = x - 3;
	
	return (x / ECM_PAYLOAD_BYTES);
}

static void _build_emmu_hash_data(uint8_t *hash, eurocrypt_t *e, int x)
{
	uint8_t msg[MAC_PAYLOAD_BYTES];
	int msglen;
	
	/* Clear hash memory */
	memset(hash, 0, 8);
	memset(msg, 0, MAC_PAYLOAD_BYTES);
	
	/* Build the hash data */
	msglen = 0;
	
	/* Copy provider ID into data buffer */
	memcpy(msg + msglen, e->emmode->ppid, 3); msglen += 3;
	
	/* Copy LABEL + 1 byte into data buffer */
	memcpy(msg + msglen, e->emmu_pkt + 15, 0x0C); msglen += 0x0C;
	
	/* Copy "what to do" byte */
	memcpy(msg + msglen, e->emmu_pkt + 40, 0x01); msglen += 0x01;
	
	/* Copy key or date data */
	memcpy(msg + msglen, e->emmu_pkt + 28, 0x06); msglen += 0x06;
	memcpy(msg + msglen, e->emmu_pkt + 38, 0x02); msglen += 0x02;
	
	_calc_ec_hash(hash, msg, e->emmode->des_algo, msglen, e->emmode->key);
}

static uint8_t _update_emmu_packet_system_s(eurocrypt_t *e, int t)
{
	int i, x;
	uint8_t *pkt = e->emmu_pkt;
	
	memset(pkt, 0, MAC_PAYLOAD_BYTES * 2);
	
	/* Packet Type */
	x = 0;
	pkt[x++] = EMMU;

	/* Unique Address - reversed */
	memcpy(&pkt[x], e->emmode->ua, 5); x += 5;

	pkt[x++] = 0x00;
	pkt[x++] = 0xA0;

	pkt[x++] = e->emmode->ppid[1];
	pkt[x++] = e->emmode->ppid[2];

	/* Set channel label */	
	memset(&pkt[x], 0x20, 0x0D);
	i = (int) (strchr(e->mode->channame, '(') - e->mode->channame);
	strncpy((char *) &pkt[x], e->mode->channame, i > 1 ? i - 1 : 0x0D);
	x += 0x0D;

	/* Start/end date */
	uint16_t d;
	d = _get_ec_date(_get_sub_date(1, e->mode->date), e->emmode->des_algo);
	pkt[x++] = (d & 0xFF00) >> 8;
	pkt[x++] = (d & 0x00FF) >> 0;
	d = _get_ec_date(_get_sub_date(31, e->mode->date), e->emmode->des_algo);
	pkt[x++] = (d & 0xFF00) >> 8;
	pkt[x++] = (d & 0x00FF) >> 0;

	pkt[x++] = 0x0A;
	pkt[x++] = 0x01;
	pkt[x++] = 0x10;
	pkt[x++] = 0x01;

	memcpy(&pkt[x], e->mode->key, 8);
	x += 8;

	pkt[x++] = pkt[1];	/* Must match the packet header */
	pkt[x++] = pkt[2];	/* .. */
	pkt[x++] = pkt[3];	/* .. */
	pkt[x++] = pkt[4];	/* .. */
	pkt[x++] = pkt[5];	/* .. */
	pkt[x++] = pkt[6];	/* .. */
	pkt[x++] = pkt[7];	/* .. */

	_eurocrypt_system_s(&pkt[x - 39], e->emmode->key);

	return(x / ECM_PAYLOAD_BYTES);
}

static uint8_t _update_emmu_packet(eurocrypt_t *e, int t)
{
	int i, x;
	uint16_t b;
	uint8_t *pkt = e->emmu_pkt;
	
	memset(pkt, 0, MAC_PAYLOAD_BYTES * 2);
	
	/* Packet Type */
	x = 0;
	pkt[x++] = EMMU;

	/* Unique Address - reversed */
	memcpy(&pkt[x], e->emmode->ua, 5); x += 5;
	
	/* Command Identifier, CI */
	b  = (e->emmode->packet_type & 0x30) << 2; /* Crypto-algo type */
	b |= 1 << 1;                         /* Format bit - 0: fixed, 1: variable */
	b |= 1 << 0;                         /* 1 */
	pkt[x++] = b;
	
	/* Command Length Indicator, CLI -- updated later */
	pkt[x++] = 0;
	
	/* PPID */
	pkt[x++] = 0x90; /* PI */
	pkt[x++] = 0x03; /* LI */
	/* Provider ID and op-key to use */
	memcpy(&pkt[x], e->emmode->ppid, 3); x += 3;
	
	/* LABEL string - always included */
	pkt[x++] = 0xA7;
	pkt[x++] = 0x0B;
	memset(&pkt[x], 0x20, 0x0B);

	i = (int) (strchr(e->mode->channame, '(') - e->mode->channame);

	strncpy((char *) &pkt[x], e->mode->channame, i > 1 ? i - 1 : 0x0B);
	x += 0x0B;
	
	if(++flag % 3 == 0)
	{
		uint8_t data[8];
		uint16_t d;

		/* Date header */
		pkt[x++] = 0xA8;
		pkt[x++] = 0x06;

		/* Date/theme */
		d = _get_ec_date(_get_sub_date(1, e->mode->date), e->emmode->des_algo);
		data[0] = (d & 0xFF00) >> 8;
		data[1] = (d & 0x00FF) >> 0;
		d = _get_ec_date(_get_sub_date(31, e->mode->date), e->emmode->des_algo);
		data[2] = (d & 0xFF00) >> 8;
		data[3] = (d & 0x00FF) >> 0;

		/* Theme/level */
		memcpy(&data[4], e->mode->theme, 2);

		/* Padding */
		data[6] = 0x00;
		data[7] = 0x00;

		/* Date is encrypted in S2/3DES mode */
		_encrypt_date(e->enc_data, e, data);
	}
	else
	{
		/* CLE - key */
		pkt[x++] = 0xEF;
		pkt[x++] = 0x06;

		_encrypt_opkey(e->enc_data, e, t);
	}

	memcpy(&pkt[x], e->enc_data, 6); x += 6;	
	
	/* Padding */
	pkt[x++] = 0xDF;
	pkt[x++] = 0x00;
	
	/* EMM hash */
	pkt[x++] = 0xF0;
	pkt[x++] = 0x08;
	memcpy(&pkt[x], e->enc_data + 6, 2); x += 2;
	
	/* Action byte */
	/*
		01/02: dates (also writes label)
		10: first 8-bytes for label
		11: last 3 bytes for label
		2x: update key, where x is key number
		30: update issuer key 0
		31: update issuer key 1
		40: write "CA" byte in 10 02 CA >20<
		50: write prov-id
	*/

	/* ID to use and update */
	if(flag % 3 == 0)
	{
		b = 0x02; /* Update date */
	}
	else
	{
		b = 0x20; /* Update key */
	
		/* Index to update */
		if(e->emmode->des_algo == EC_3DES && e->emmode->packet_type == EC_3DES)
		{
			b |= _tdesmap[(e->mode->ppid[2] & 0x0F) - 0x0C][t];
		}
		else
		{
			b |= e->mode->ppid[2] & 0x0F;
		}
	}

	pkt[x++] = b;

	_build_emmu_hash_data(&pkt[x], e, x);
	memcpy(e->emm_hash, &pkt[x], 8);
	memcpy(&pkt[x], e->emm_hash + 3, 5); x += 5;

	/* Update the CI command length */
	pkt[7] = x - 8;

	return(x / ECM_PAYLOAD_BYTES);
}

static void _update_emms_packet(eurocrypt_t *e, int t)
{
	int x;
	uint16_t b;
	uint8_t *pkt = e->emms_pkt;
	
	memset(pkt, 0, MAC_PAYLOAD_BYTES);
	
	x = 0;
	
	/* Packet Type */
	pkt[x++] = EMMS;
	
	/* Shared Address - reversed */
	memcpy(&pkt[x], e->emmode->sa, 3); x += 3;
	
	/* Command Identifier, CI */
	b  = (e->emmode->packet_type & 0x30) << 2;  /* Crypto-algo type */
	b |= 0 << 1;                         		/* Format bit - 0: fixed, 1: variable */
	b |= 0 << 0;                         		/* ADF - clear */
	pkt[x++] = b;
	
	/* Command Length Indicator, CLI */
	pkt[x++] = 0x28;
	
	/* ADF */
	memset(&pkt[x], 0xFF, 32); x += 32;
	
	if(e->emmode->des_algo == EC_M)
	{
		/* EMM hash */
		_build_emms_hash_data(&pkt[x], e); x += 8;
		memcpy(e->emm_hash, &pkt[x - 8], 8);
	}
	else
	{
		x -= 7;
		
		/* ID to use and update */
		if(++flag % 3 == 0)
		{
			b = 0x02; /* Update date */
		}
		else
		{
			b = 0x20; /* Update key */
		}
		
		/* Index to update */
		if(e->emmode->des_algo == EC_3DES && e->emmode->packet_type == EC_3DES)
		{
			b |= _tdesmap[(e->mode->ppid[2] & 0x0F) - 0x0C][t];
		}
		else
		{
			b |= e->mode->ppid[2] & 0x0F;
		}
		
		pkt[x++] = b;
		
		/* Update key index */
		b  = (e->emmode->ppid[2] & 0x0F) << 4;
		
		/* PPID to update */
		if(e->emmode->packet_type == EC_M)
		{
			b |= (e->mode->ppid[1] & 0x0F);
		}
		else
		{
			b |= (e->mode->ppid[2] & 0xF0) >> 4;
		}
		
		pkt[x++] = b;
				
		if(flag % 3 == 0)
		{
			uint8_t data[8];
			uint16_t d;

			/* Date */
			d = _get_ec_date(_get_sub_date(1, e->mode->date), e->emmode->des_algo);
			data[0] = (d & 0xFF00) >> 8;
			data[1] = (d & 0x00FF) >> 0;
			d = _get_ec_date(_get_sub_date(31, e->mode->date), e->emmode->des_algo);
			data[2] = (d & 0xFF00) >> 8;
			data[3] = (d & 0x00FF) >> 0;

			/* Theme/level */
			memcpy(&data[4], e->mode->theme, 2);

			/* Padding */
			data[6] = 0x00;
			data[7] = 0x00;

			/* Date is encrypted in S2 mode */
			_encrypt_date(e->enc_data, e, data);
		}
		else
		{
			_encrypt_opkey(e->enc_data, e, t);
		}

		memcpy(&pkt[x], e->enc_data, 8); x += 8;
		
		/* EMM hash */
		_build_emms_hash_data(e->emm_hash, e);
		memcpy(&pkt[x], e->emm_hash + 3, 5);
	}
	
	mac_golay_encode(pkt + 1, 30);
}

/* Global EMM - only valid in Eurocrypt-M mode */
static uint8_t _update_emmg_packet(eurocrypt_t *e, int t, char *ppv)
{
	int x;
	uint16_t b, d;
	uint8_t *pkt = e->emmg_pkt;
	
	memset(pkt, 0, MAC_PAYLOAD_BYTES * 2);
	
	/* Packet Type */
	x = 0;
	pkt[x++] = EMMG;
	
	/* Command Identifier, CI */
	b  = (e->emmode->packet_type & 0x30) << 2; /* Crypto-algo type */
	b |= 1 << 1;                         /* Format bit - 0: fixed, 1: variable */
	b |= t << 0;                         /* Toggle */
	pkt[x++] = b;
	
	/* Command Length Indicator, CLI -- updated later */
	pkt[x++] = 0;
	
	/* PPID */
	pkt[x++] = 0x90; /* PI */
	pkt[x++] = 0x03; /* LI */
	/* Provider ID and M-key to use for decryption of op-key */
	memcpy(&pkt[x], e->emmode->ppid, 3); x += 3;
	
	/* CTRL - Global EMM */
	pkt[x++] = 0xA0;
	pkt[x++] = 0x01;
	pkt[x++] = 0x00;
	
	if(ppv && t)
	{
		d = _get_ec_date(_get_sub_date(0, e->mode->date), e->mode->des_algo);
		pkt[x++] = 0xAB;
		pkt[x++] = 0x04;
		pkt[x++] = (d & 0xFF00) >> 8;
		pkt[x++] = (d & 0x00FF) >> 0;
		pkt[x++] = 0x0F;
		pkt[x++] = 0xFF;
	}
	else
	{
		/* Date/theme */
		pkt[x++] = 0xA8;
		pkt[x++] = 0x06;
		d = _get_ec_date(_get_sub_date(1, e->mode->date), e->emmode->des_algo);
		pkt[x++] = (d & 0xFF00) >> 8;
		pkt[x++] = (d & 0x00FF) >> 0;
		d = _get_ec_date(_get_sub_date(31, e->mode->date), e->emmode->des_algo);
		pkt[x++] = (d & 0xFF00) >> 8;
		pkt[x++] = (d & 0x00FF) >> 0;
		memcpy(&pkt[x], e->mode->theme, 2); x += 2;
		
		/* IDUP */
		pkt[x++] = 0xA1;
		pkt[x++] = 0x03;
		/* Provider ID and op-key to update */
		memcpy(&pkt[x], e->mode->ppid, 3); x += 3;
		
		/* Encrypted op-key */
		pkt[x++] = 0xEF;
		pkt[x++] = 0x08;
		_encrypt_opkey(e->enc_data, e, t);
		memcpy(&pkt[x], e->enc_data, 8); x += 8;
	}
	
	/* EMM hash */
	pkt[x++] = 0xF0;
	pkt[x++] = 0x08;
	_build_emmg_hash_data(&pkt[x], e, x); x += 8;
	
	memcpy(e->emm_hash, &pkt[x - 8], 8);
	
	/* Update the CI command length */
	pkt[2] = x - 3;
	
	return(x / ECM_PAYLOAD_BYTES);
}

static uint8_t _update_emmgs_packet(eurocrypt_t *e, int t)
{
	int x;
	uint16_t b;
	uint8_t *pkt = e->emmg_pkt;
	
	memset(pkt, 0, MAC_PAYLOAD_BYTES * 2);
	
	/* Packet Type */
	x = 0;
	pkt[x++] = EMMG;
	
	/* Command Identifier, CI */
	b  = (e->emmode->packet_type & 0x30) << 2; /* Crypto-algo type */
	b |= 1 << 1;                         /* Format bit - 0: fixed, 1: variable */
	b |= t << 0;                         /* Toggle */
	pkt[x++] = b;
	
	/* Command Length Indicator, CLI -- updated later */
	pkt[x++] = 0;
	
	/* PPID */
	pkt[x++] = 0x90; /* PI */
	pkt[x++] = 0x03; /* LI */
	/* Provider ID and M-key to use for decryption of op-key */
	memcpy(&pkt[x], e->emmode->ppid, 3); x += 3;
	
	if(e->emmode->des_algo == EC_M)
	{
		/* IDUP */
		pkt[x++] = 0xA1;
		pkt[x++] = 0x03;
		/* Provider ID and op-key to update */
		memcpy(&pkt[x], e->mode->ppid, 3); x += 3;
		
		pkt[x++] = 0xEF;
		pkt[x++] = 0x08;
		_encrypt_opkey(e->enc_data, e, t);
		memcpy(&pkt[x], e->enc_data, 8); x += 8;
	}
	else
	{
		int i;

		/* LABEL string - always included */
		pkt[x++] = 0xA7;
		pkt[x++] = 0x0B;
		memset(&pkt[x], 0x20, 0x0B);

		i = (int) (strchr(e->mode->channame, '(') - e->mode->channame);

		strncpy((char *) &pkt[x], e->mode->channame, i > 1 ? i - 1 : 0x0B);
		x += 0x0B;

		/* Padding */
		pkt[x++] = 0xDF;
		pkt[x++] = 0x00;
	}
	
	/* Update the CI command length */
	pkt[2] = x - 3;
	
	return(x / ECM_PAYLOAD_BYTES);
}

static uint64_t _update_cw(eurocrypt_t *e, int t)
{
	uint64_t cw;
	int i, r;
	
	/* Fetch the next active CW */
	for(cw = i = 0; i < 8; i++)
	{
		cw = (cw << 8) | e->cw[t][i];
	}
	
	/* Generate a new CW */
	t ^= 1;
	
	for(i = 0; i < 8; i++)
	{
		e->cw[t][i] = e->ecw[t][i] = rand() & 0xFF;
	}

	/* EC-S uses a home-brew encryption */
	if(e->mode->des_algo != EC_S)
	{
		/* Three rounds for 3DES mode, one round for others */
		for(r = 0; r < (e->mode->des_algo != EC_3DES ? 1 : 3); r++)
		{
			/* Use second key on second round in 3DES */
			_eurocrypt(e->ecw[t], e->mode->key + (r != 1 ? 0 : 8), ECM, e->mode->des_algo, r + 1);
		}
	}
		
	return(cw);
}

void eurocrypt_next_frame(vid_t *vid, int frame)
{
	eurocrypt_t *e = &vid->mac.ec;
	
	/* Update the CW at the beginning of frames FCNT == 1 */
	if((frame & 0xFF) == 1)
	{
		int t = (frame >> 8) & 1;
		
		/* Fetch and update next CW */
		vid->mac.cw = _update_cw(e, t);
		
		/* Update the ECM packet */
		if(e->mode->packet_type == EC_S)
		{
			e->ecm_cont = _update_ecm_packet_ec_s(e);
		}
		else
		{
			e->ecm_cont = _update_ecm_packet(e, t, vid->mac.ec_mat_rating, vid->conf.ec_ppv, vid->conf.nodate);
		}
		
		/* Print ECM */
		if(vid->conf.showecm)
		{
			int i;
			if(frame == 1)
			{
				fprintf(stderr, "\n+----+----------------------+-------------------------+-------------------------+");
				fprintf(stderr, "----------------------------+----------------------------+");
				fprintf(stderr, "-------------------------+");
				fprintf(stderr, "\n| ## |   Operational Key    |   Encrypted CW (even)   |    Encrypted CW (odd)   |");
				fprintf(stderr, "    Decrypted CW (even)     |     Decrypted CW (odd)     |");
				fprintf(stderr, "           Hash          |");
			}
			fprintf(stderr, "\n+----+----------------------+-------------------------+-------------------------+");
			fprintf(stderr, "----------------------------+----------------------------+");
			fprintf(stderr, "-------------------------+");
			fprintf(stderr, "\n| %02X | ", e->mode->ppid[2] & 0x0F);
			for(i = 0; i < 7; i++) fprintf(stderr, "%02X ", e->mode->key[i]);
			fprintf(stderr, "| ");
			for(i = 0; i < 8; i++) fprintf(stderr, "%02X ", e->ecw[0][i]);
			fprintf(stderr, "| ");
			for(i = 0; i < 8; i++) fprintf(stderr, "%02X ", e->ecw[1][i]);
			fprintf(stderr, "| %s", t ? "  " : "->");
			for(i = 0; i < 8; i++) fprintf(stderr, "%02X ", e->cw[0][i]);
			fprintf(stderr, " | %s", t ? "->" : "  ");
			for(i = 0; i < 8; i++) fprintf(stderr, "%02X ", e->cw[1][i]);
			fprintf(stderr, " | ");
			for(i = 0; i < 8; i++) fprintf(stderr, "%02X ", (uint8_t) e->ecm_hash[i]);
			fprintf(stderr, "|");
		}
	}
	
	/* Send an ECM packet every 64 frames - ~2.5s */
	if((frame % 64) == 1)
	{
		uint8_t pkt[MAC_PAYLOAD_BYTES];
		memset(pkt, 0, MAC_PAYLOAD_BYTES);
		int i;
		
		/* Break up the ECM packet, if required */
		for(i = 0; i <= e->ecm_cont; i++)
		{
			memcpy(pkt, e->ecm_pkt + (i * ECM_PAYLOAD_BYTES), ECM_PAYLOAD_BYTES + 1);
			
			pkt[0] = ECM;
			
			/* Golay encode the payload */
			mac_golay_encode(pkt + 1, 30);
			mac_write_packet(vid, 0, e->ecm_addr, i, pkt, 0);
		}	
	}

	/* Send EMMs every ~10 seconds, if available */
	if(e->emmode->id != NULL)
	{
		if((vid->frame & 0xFF) == 0x7F)
		{
			if(e->emmode->packet_type == EC_S)
			{
				/* Generate EMM-Unique packet */
				if(e->emmode->emmtype == EMMU)
				{
					uint8_t pkt[MAC_PAYLOAD_BYTES];
					memset(pkt, 0, MAC_PAYLOAD_BYTES);
					
					int i;
					
					int t = (vid->frame >> 8) & 1;
					
					e->emm_cont = _update_emmu_packet_system_s(e, t);
					
					/* Break up the EMM-U packet, if required */
					for(i = 0; i <= e->emm_cont; i++)
					{
						memcpy(pkt, e->emmu_pkt + (i * ECM_PAYLOAD_BYTES), ECM_PAYLOAD_BYTES + 1);
						
						pkt[0] = EMMU;

						/* Golay encode the payload */
						mac_golay_encode(pkt + 1, 30);
						
						/* Write the packet */
						mac_write_packet(vid, 0, e->emm_addr, i, pkt, 0);
					}
				}
			}
			else
			{
				/* Generate EMM-Global packet */
				if(e->emmode->emmtype == EMMG)
				{
					uint8_t pkt[MAC_PAYLOAD_BYTES];
					memset(pkt, 0, MAC_PAYLOAD_BYTES);
					
					int i;
					
					int t = (vid->frame >> 8) & 1;
					
					e->emm_cont = _update_emmg_packet(e, t, vid->conf.ec_ppv);
					
					/* Break up the EMM-G packet, if required */
					for(i = 0; i <= e->emm_cont; i++)
					{
						memcpy(pkt, e->emmg_pkt + (i * ECM_PAYLOAD_BYTES), ECM_PAYLOAD_BYTES + 1);
						
						pkt[0] = e->emmode->emmtype;
						
						/* Golay encode the payload */
						mac_golay_encode(pkt + 1, 30);
						
						/* Write the packet */
						mac_write_packet(vid, 0, e->emm_addr, i, pkt, 0);
					}
				}
				
				/* Generate EMM-Unique packet */
				if(e->emmode->emmtype == EMMU)
				{
					uint8_t pkt[MAC_PAYLOAD_BYTES];
					memset(pkt, 0, MAC_PAYLOAD_BYTES);
					
					int i;
					
					int t = (vid->frame >> 8) & 1;
					
					e->emm_cont = _update_emmu_packet(e, t);
					
					/* Break up the EMM-U packet, if required */
					for(i = 0; i <= e->emm_cont; i++)
					{
						memcpy(pkt, e->emmu_pkt + (i * ECM_PAYLOAD_BYTES), ECM_PAYLOAD_BYTES + 1);
						
						pkt[0] = EMMU;

						/* Golay encode the payload */
						mac_golay_encode(pkt + 1, 30);
						
						/* Write the packet */
						mac_write_packet(vid, 0, e->emm_addr, i, pkt, 0);
					}
				}
				
				/* Generate EMM-Shared packet */
				if(e->emmode->emmtype == EMMS)
				{
					uint8_t pkt[MAC_PAYLOAD_BYTES];
					memset(pkt, 0, MAC_PAYLOAD_BYTES);
					int i;
					
					int t = (vid->frame >> 8) & 1;
					
					/* Shared EMM packet requires EMM-Global packet before it */
					e->emm_cont = _update_emmgs_packet(e, t);
					
					/* Break up the EMM-G packet, if required */
					for(i = 0; i <= e->emm_cont; i++)
					{
						memcpy(pkt, e->emmg_pkt + (i * ECM_PAYLOAD_BYTES), ECM_PAYLOAD_BYTES + 1);
						
						pkt[0] = EMMG;

						/* Golay encode the payload */
						mac_golay_encode(pkt + 1, 30);
						
						mac_write_packet(vid, 0, e->emm_addr, i, pkt, 0);
					}
					
					/* Generate the EMM-S packet (always fixed length) */
					_update_emms_packet(e, t);
					
					mac_write_packet(vid, 0, e->emm_addr, 0, e->emms_pkt, 0);
				}
			}
			
			/* Print EMM to console */
			if(vid->conf.showecm)
			{
				int i;
				fprintf(stderr, "\n\n ***** EMM *****");
				fprintf(stderr, "\nUnique address:\t\t");
				for(i = 0; i < 4; i++) fprintf(stderr, "%02X ", e->emmode->ua[3 - i]);
				fprintf(stderr, "\nShared address:\t\t");
				for(i = 0; i < 3; i++) fprintf(stderr, "%02X ", e->emmode->sa[2 - i]);
				fprintf(stderr, "\nManagement key   [%02X]:\t", e->emmode->ppid[2] & 0x0F);
				for(i = 0; i < 7; i++) fprintf(stderr, "%02X ", e->emmode->key[i]);
				fprintf(stderr, "\nDecrypted op key [%02X]:\t", e->mode->ppid[2] & 0x0F);
				for(i = 0; i < 7; i++) fprintf(stderr, "%02X ", e->mode->key[i]);
				fprintf(stderr, "\nEncrypted op key [%02X]:\t", e->mode->ppid[2] & 0x0F);
				for(i = 0; i < 8; i++) fprintf(stderr, "%02X ", e->enc_data[i]);
				fprintf(stderr,"\nHash:\t\t\t");
				for(i = 0; i < 8; i++) fprintf(stderr, "%02X ", (uint8_t) e->emm_hash[i]);
				fprintf(stderr,"\n");
			}
		}
	}
}

int eurocrypt_init(vid_t *vid, const char *mode)
{
	eurocrypt_t *e = &vid->mac.ec;
	
	memset(e, 0, sizeof(eurocrypt_t));
	
	/* Find the ECM mode */
	for(e->mode = _ec_modes; e->mode->id != NULL; e->mode++)
	{
		if(strcmp(mode, e->mode->id) == 0) break;
	}
	
	if(e->mode->id == NULL)
	{
		fprintf(stderr, "Unrecognised Eurocrypt mode.\n");
		return(VID_ERROR);
	}
	
	/* Find the EMM mode */
	for(e->emmode = _em_modes; e->emmode->id != NULL; e->emmode++)
	{
		if(strcmp(mode, e->emmode->id) == 0) break;
	}
	
	if(e->emmode->id == NULL)
	{
		fprintf(stderr, "Cannot find a matching EMM mode.\n");
	}
	
	/* ECM/EMM address */
	e->ecm_addr = 346;
	e->emm_addr = 347;
	
	/* Generate initial even and odd encrypted CWs */
	_update_cw(e, 0);
	_update_cw(e, 1);
	
	/* Generate initial packet */
	if(e->mode->packet_type == EC_S)
	{
		e->ecm_cont = _update_ecm_packet_ec_s(e);
	}
	else
	{
		e->ecm_cont = _update_ecm_packet(e, 0, vid->mac.ec_mat_rating, vid->conf.ec_ppv, vid->conf.nodate);
	}
	
	return(VID_OK);
}

