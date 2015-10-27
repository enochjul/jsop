//This Source Code Form is subject to the terms of the Mozilla Public
//License, v. 2.0. If a copy of the MPL was not distributed with this
//file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <limits.h>

#include "JsopStringBuffer.h"

char *JsopStringBuffer::resize() noexcept {
	char *new_start;
	size_t n, new_capacity;

	n = AllocEnd - Start;
	assert(n >= 16);

	if (n <= (SIZE_MAX / (2 * sizeof(char)))) {
		new_capacity = n * 2;

		new_start = static_cast<char *>(realloc(Start, new_capacity * sizeof(char)));
		if (new_start != nullptr) {
			Start = new_start;
			End = new_start + n;
			AllocEnd = new_start + new_capacity;
		}
		return new_start;
	}
	return nullptr;
}

bool JsopStringBuffer::appendUtf32(uint32_t code) noexcept {
	size_t remaining_capacity;

	if (code <= 0x7F) {
		if (End == AllocEnd) {
			if (resize() == nullptr) {
				return false;
			}
		}
		*End = code;
		End += 1;
	} else if (code <= 0x7FF) {
		remaining_capacity = AllocEnd - End;
		if (remaining_capacity < 2) {
			if (resize() == nullptr) {
				return false;
			}
		}
		*End = (code >> 6) + 0xC0;
		*(End + 1) = (code & 0x3F) + 0x80;
		End += 2;
	} else if (code <= 0xFFFF) {
		remaining_capacity = AllocEnd - End;
		if (remaining_capacity < 3) {
			if (resize() == nullptr) {
				return false;
			}
		}
		*End = (code >> 12) + 0xE0;
		*(End + 1) = ((code & 0xFFF) >> 6) + 0x80;
		*(End + 2) = (code & 0x3F) + 0x80;
		End += 3;
	} else {
		assert(code <= 0x10FFFF);

		remaining_capacity = AllocEnd - End;
		if (remaining_capacity < 4) {
			if (resize() == nullptr) {
				return false;
			}
		}
		*End = (code >> 18) + 0xF0;
		*(End + 1) = ((code & 0x3FFFF) >> 12) + 0x80;
		*(End + 2) = ((code & 0xFFF) >> 6) + 0x80;
		*(End + 3) = (code & 0x3F) + 0x80;
		End += 4;
	}
	return true;
}

