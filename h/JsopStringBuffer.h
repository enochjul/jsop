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

	char *getEnd() noexcept {
		return End;
	}

	void setEnd(char *ptr) noexcept {
		assert(ptr >= Start && ptr < AllocEnd);
		End = ptr;
	}

	//! Appends the given character to the string
	JSOP_INLINE bool append(char ch) noexcept {
		auto end = End;
		if (JSOP_UNLIKELY(end == AllocEnd)) {
			if (resize() == nullptr) {
				return false;
			}
			end = End;
		}
		*end = ch;
		End = end + 1;
		return true;
	}

	//! Appends the given 2 characters to the string
	JSOP_INLINE bool append(char ch0, char ch1) noexcept {
		auto end = End;
		size_t remaining_capacity = AllocEnd - end;
		if (JSOP_UNLIKELY(remaining_capacity < 2)) {
			if (resize() == nullptr) {
				return false;
			}
			end = End;
		}
		*end = ch0;
		*(end + 1) = ch1;
		End = end + 2;
		return true;
	}

	//! Appends the given 3 characters to the string
	JSOP_INLINE bool append(char ch0, char ch1, char ch2) noexcept {
		auto end = End;
		size_t remaining_capacity = AllocEnd - end;
		if (JSOP_UNLIKELY(remaining_capacity < 3)) {
			if (resize() == nullptr) {
				return false;
			}
			end = End;
		}
		*end = ch0;
		*(end + 1) = ch1;
		*(end + 2) = ch2;
		End = end + 3;
		return true;
	}

	//! Appends the given 3 characters to the string
	JSOP_INLINE bool append(char ch0, char ch1, char ch2, char ch3) noexcept {
		auto end = End;
		size_t remaining_capacity = AllocEnd - End;
		if (JSOP_UNLIKELY(remaining_capacity < 4)) {
			if (resize() == nullptr) {
				return false;
			}
			end = End;
		}
		*end = ch0;
		*(end + 1) = ch1;
		*(end + 2) = ch2;
		*(end + 3) = ch3;
		End = end + 4;
		return true;
	}

	//! Resize the buffer if the remaining capacity in the buffer is less than the given size
	JSOP_INLINE bool resize_if(size_t n) noexcept {
		assert(static_cast<size_t>(AllocEnd - Start) >= n);
		size_t remaining_capacity = AllocEnd - End;
		if (JSOP_UNLIKELY(remaining_capacity < n)) {
			if (resize() == nullptr) {
				return false;
			}
		}
		return true;
	}

	//! Converts the 32-bit character into the equivalent UTF-8 character(s) and appends to the string
	bool appendUtf32(uint32_t code) noexcept;
};

#endif
