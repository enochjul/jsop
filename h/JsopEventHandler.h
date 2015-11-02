//This Source Code Form is subject to the terms of the Mozilla Public
//License, v. 2.0. If a copy of the MPL was not distributed with this
//file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef JSOP_EVENT_HANDLER_H
#define JSOP_EVENT_HANDLER_H

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "JsopDefines.h"

//! Base class for event driven parser handler that use a simple context stack for array/object
class JsopEventHandler {
	size_t *Start;
	size_t *End;
	size_t *AllocEnd;
	size_t Mask;

	static_assert(JSOP_EVENT_HANDLER_STACK_MIN_SIZE % sizeof(*Start) == 0, "JSOP_EVENT_HANDLER_STACK_MIN_SIZE % sizeof(*Start) == 0");

	size_t *resize() noexcept;

	JSOP_INLINE bool push() noexcept {
		Mask <<= 1;
		if (Mask == 0) {
			if (End == AllocEnd) {
				if (resize() == nullptr) {
					return false;
				}
			}
			++End;
			Mask = 1;
		}
		return true;
	}

	JSOP_INLINE void pop() noexcept {
		assert(End > Start);

		Mask >>= 1;
		if (Mask == 0) {
			--End;
			Mask = static_cast<size_t>(1) << (sizeof(size_t) * CHAR_BIT - 1);
		}
	}

public:
	enum : bool {
		NoExceptions = true
	};

	JsopEventHandler() noexcept {
		Start = static_cast<size_t *>(malloc(JSOP_EVENT_HANDLER_STACK_MIN_SIZE));
		End = Start;
		AllocEnd = Start + JSOP_EVENT_HANDLER_STACK_MIN_SIZE / sizeof(*Start);
		Mask = 0;
	}
	~JsopEventHandler() noexcept {
		free(Start);
	}

	JsopEventHandler(const JsopEventHandler &) = delete;
	JsopEventHandler &operator =(const JsopEventHandler &) = delete;

	//! Check if strings need a null terminator character at the end
	bool requireNullTerminator() const noexcept {
		return false;
	}

	//! Initialize the parsing
	bool start() noexcept {
		End = Start;
		Mask = 0;
		return Start != nullptr;
	}
	//! Finish the parsing
	bool finish() noexcept {
		return true;
	}
	//! Clean up on a parse error
	void cleanup() noexcept {
	}

	//! Checks if it is parsing a value at the top level
	bool inTop() const noexcept {
		return Start == End;
	}

	//! Checks if it is parsing a value inside an array
	bool inArray() const noexcept {
		assert(End > Start && Mask != 0);
		return (*(End - 1) & Mask) == 0;
	}

	//! Checks if it is parsing a value inside an object
	bool inObject() const noexcept {
		assert(End > Start && Mask != 0);
		return (*(End - 1) & Mask) != 0;
	}

	bool makeNull() noexcept {
		return true;
	}

	bool makeBool(bool value) noexcept {
		return true;
	}

	bool makeInteger(uint64_t value, bool negative) noexcept {
		return true;
	}

	bool makeDouble(double value) noexcept {
		return true;
	}

	bool makeString(const char *start, const char *end, bool key) noexcept {
		return true;
	}

	//! Push an array to the context stack
	JSOP_INLINE bool pushArray() noexcept {
		if (push()) {
			*(End - 1) &= ~Mask;
			return true;
		}
		return false;
	}

	//! Checks if the top of the context stack refers to an array and pops the stack
	JSOP_INLINE bool popArray() noexcept {
		assert(End > Start && Mask != 0);

		//Checks if the top of the stack is an array
		if ((*(End - 1) & Mask) == 0) {
			pop();
			return true;
		}
		return false;
	}

	//! Push an object to the context stack
	JSOP_INLINE bool pushObject() noexcept {
		if (push()) {
			*(End - 1) |= Mask;
			return true;
		}
		return false;
	}

	//! Checks if the top of the context stack refers to an object and pops the stack
	JSOP_INLINE bool popObject() noexcept {
		assert(End > Start && Mask != 0);

		//Checks if the top of the stack is an object
		if ((*(End - 1) & Mask) != 0) {
			pop();
			return true;
		}
		return false;
	}
};

#endif
