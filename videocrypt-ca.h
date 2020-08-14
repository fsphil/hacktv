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

#ifndef _VIDEOCRYPT_CA_H
#define _VIDEOCRYPT_CA_H

#include <stdint.h>

#define VC_TAC1 3001
#define VC_TAC2 3002
#define VC_SKY7 3003
#define VC2_MC  3004

typedef struct {
	uint8_t mode;
	uint64_t codeword;
	uint8_t messages[7][32];
} _vc_block_t;

typedef struct {
	uint8_t mode;
	uint64_t codeword;
	uint8_t messages[8][32];
	/* Random bytes */
	uint8_t b1, b2, b3;
} _vc2_block_t;

/* Videocrypt 1 */
extern void _vc_seed_p03(_vc_block_t *s);
extern void _vc_seed_p07(_vc_block_t *s, int ca);
extern void _vc_seed_p09(_vc_block_t *s);

extern void _vc_emm_p07(_vc_block_t *s, int cmd, uint32_t cardserial);
extern void _vc_emm_p09(_vc_block_t *s, int cmd, uint32_t cardserial);

extern void _vc_seed_xtea(_vc_block_t *s);
extern void  _vc_seed_ppv(_vc_block_t *s, uint8_t _ppv_card_data[7]);

/* Videocrypt 2 */
extern void _vc_seed_vc2(_vc2_block_t *s, int ca);
extern void _vc2_emm(_vc2_block_t *s, int cmd, uint32_t cardserial, int ca);


#endif