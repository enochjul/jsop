//This Source Code Form is subject to the terms of the Mozilla Public
//License, v. 2.0. If a copy of the MPL was not distributed with this
//file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef JSOP_STRING_BUFFER_H
#define JSOP_STRING_BUFFER_H

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "JsopDefines.h"

//! Represents a temporary automatically resized string buffer
class JsopStringBuffer final {
	char *Start;
	char *End;
	char *AllocEnd;

	char *resize() noexcept;

public:
	//! Creates a string buffer with the given size
	explicit JsopStringBuffer(size_t n) noexcept {
		assert(n >= 16 && n % 16 == 0);

		Start = static_cast<char *>(malloc(n * sizeof(char)));
		End = Start;
		AllocEnd = Start + n;
	}
	~JsopStringBuffer() {
		free(Start);
	}

	JsopStringBuffer(const JsopStringBuffer &) = delete;
	JsopStringBuffer &operator =(const JsopStringBuffer &) = delete;

	bool initialized() const noexcept {
		return Start != nullptr;
	}

	void clear() noexcept {
		End = Start;
	}

	const char *getStart() const noexcept {
		return Start;
	}

	const char *getEnd() const noexcept {
		return End;
	}

	//! Appends the given character to the string
	JSOP_INLINE bool append(char ch) noexcept {
		if (End == AllocEnd) {
			if (resize() == nullptr) {
				return false;
			}
		}
		*End = ch;
		++End;
		return true;
	}

	//! Converts the 32-bit character into the equivalent UTF-8 character(s) and appends to the string
	bool appendUtf32(uint32_t code) noexcept;
};

#endif
