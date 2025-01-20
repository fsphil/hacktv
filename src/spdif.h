/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2025 Philip Heron <phil@sanslogic.co.uk>                    */
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

#ifndef _SPDIF_H
#define _SPDIF_H

#include <stdint.h>

#define SPDIF_BLOCK_SAMPLES (192 * 2)
#define SPDIF_BLOCK_BYTES (SPDIF_BLOCK_SAMPLES * 8)
#define SPDIF_BLOCK_BITS (SPDIF_BLOCK_BYTES * 8)

extern uint32_t spdif_bitrate(uint32_t sample_rate);
extern void spdif_block(uint8_t b[SPDIF_BLOCK_BYTES], const int16_t pcm[SPDIF_BLOCK_SAMPLES]);

#endif

