/*
  wiring_private.h - Internal header file.
  Part of Arduino - http://www.arduino.cc/

  Copyright (c) 2005-2006 David A. Mellis

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General
  Public License along with this library; if not, write to the
  Free Software Foundation, Inc., 59 Temple Place, Suite 330,
  Boston, MA  02111-1307  USA
*/

#ifndef WiringPrivate_h
#define WiringPrivate_h

#include <stdio.h>
#include <stdarg.h>

#include "wiring.h"

#ifndef cbi
#define cbi(sfr, bit) ((sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) ((sfr) |= _BV(bit))
#endif

typedef void (*voidFuncPtr)(void);

#endif


