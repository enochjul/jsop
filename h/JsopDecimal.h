//This Source Code Form is subject to the terms of the Mozilla Public
//License, v. 2.0. If a copy of the MPL was not distributed with this
//file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef JSOP_DECIMAL_H
#define JSOP_DECIMAL_H

#include <stdint.h>
#include "JsopDefines.h"

//! Converts the specified integer significand and exponent (of 10) to a double precision number
double jsop_decimal_to_double(uint64_t significand, int exponent, bool negative) noexcept;

//! Converts the specified integer significand and exponent (of 2) to a double precision number
double jsop_hexadecimal_to_double(uint64_t significand, int exponent, bool negative) noexcept;

#endif
