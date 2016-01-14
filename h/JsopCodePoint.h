//This Source Code Form is subject to the terms of the Mozilla Public
//License, v. 2.0. If a copy of the MPL was not distributed with this
//file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef JSOP_CODE_POINT_H
#define JSOP_CODE_POINT_H

#include <stdint.h>

//! Checks if the given code point is allowed as a part of an identifier
bool jsop_code_point_is_id_continue(uint32_t codepoint);
//! Checks if the given code point is allowed as the first character of an identifier
bool jsop_code_point_is_id_start(uint32_t codepoint);

#endif
