/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
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

#ifndef EUROCRYPT_H_
#define EUROCRYPT_H_

#include "video.h"

#define ECM 0
#define HASH 1

#define EC_M 0
#define EC_S 3

#define IP_DIM 64
#define IPP_DIM 64
#define E_DIM 48
#define S_BOXES 8
#define S_DIM 64
#define P_DIM 32
#define PC2_DIM 48
#define LS_DIM 16

extern void eurocrypt_init(mac_t *mac, char *mode);
extern void generate_ecm(ec_t *e, int cafcnt);

#endif
