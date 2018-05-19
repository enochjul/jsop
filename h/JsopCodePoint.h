//This Source Code Form is subject to the terms of the Mozilla Public
//License, v. 2.0. If a copy of the MPL was not distributed with this
//file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef JSOP_CODE_POINT_H
#define JSOP_CODE_POINT_H

#include <assert.h>
#include <stdint.h>

#include "JsopDefines.h"

#define JSOP_CODE_POINT_IDC_ASCII_BITSET_MASK 0x1
#define JSOP_CODE_POINT_IDS_ASCII_BITSET_MASK 0x2

extern const uint8_t JsopCodePointASCIIBitset[];
extern const uint8_t JsopCodePointHexDigitValue[];

//! Checks if the given code point is allowed as a part of an identifier
bool jsop_code_point_is_id_continue(uint32_t codepoint) noexcept;
//! Checks if the given code point is allowed as the first character of an identifier
bool jsop_code_point_is_id_start(uint32_t codepoint) noexcept;

//! Checks if the given code point is a 7-bit ASCII character and allowed as a part of an identifier
JSOP_INLINE bool jsop_code_point_is_ascii_id_continue(unsigned char codepoint) noexcept {
	return JsopCodePointASCIIBitset[codepoint] & JSOP_CODE_POINT_IDC_ASCII_BITSET_MASK;
}

//! Checks if the given code point is a 7-bit ASCII character and allowed as the first character of an identifier
JSOP_INLINE bool jsop_code_point_is_ascii_id_start(unsigned char codepoint) noexcept {
	return JsopCodePointASCIIBitset[codepoint] & JSOP_CODE_POINT_IDS_ASCII_BITSET_MASK;
}

//! Checks if the given char is a single ASCII char that is copied directly to the output when used in a string
JSOP_INLINE bool jsop_code_point_is_normal_ascii_string_char(unsigned char codepoint) noexcept {
	//Since the ASCII IDC and IDS sets are a strict subset of this, just load and test the value without masking
	return JsopCodePointASCIIBitset[codepoint];
}

#endif
