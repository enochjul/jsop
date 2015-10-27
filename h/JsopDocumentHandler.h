//This Source Code Form is subject to the terms of the Mozilla Public
//License, v. 2.0. If a copy of the MPL was not distributed with this
//file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef JSOP_DOCUMENT_HANDLER_H
#define JSOP_DOCUMENT_HANDLER_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "JsopDefines.h"
#include "JsopMemoryPools.h"
#include "JsopValue.h"

class JsopDocument;

//! Parser handler to generate the results as a document
class JsopDocumentHandler {
	JsopMemoryPools Pools;
	JsopValue *StackStart = nullptr;
	JsopValue *StackEnd = nullptr;
	JsopValue *StackAllocEnd = nullptr;
	size_t PrevStackSize = 0;

	JsopValue *resizeStack() noexcept;

	//! Creates a value and push it to the stack
	//! The caller must initialize the value returned
	JSOP_INLINE JsopValue *makeValue() noexcept {
		auto new_value = StackEnd;
		if (new_value == StackAllocEnd) {
			new_value = resizeStack();
			if (new_value == nullptr) {
				return nullptr;
			}
		}
		StackEnd = new_value + 1;
		return new_value;
	}

public:
	enum : bool {
		NoExceptions = true
	};

	JsopDocumentHandler() = default;
	JSOP_INLINE ~JsopDocumentHandler() noexcept {
		free(StackStart);
	}

	JsopDocumentHandler(const JsopDocumentHandler &) = delete;
	JsopDocumentHandler &operator =(const JsopDocumentHandler &) = delete;

	//! Initializes the parsing
	bool start() noexcept;
	//! Finish the parsing by moving the parsed values into the given document
	bool finish(JsopDocument *doc) noexcept;
	//! Frees memory allocated for the parsed values
	//! Does not clear the stack as it can be used for subsequent parses
	void cleanup()  noexcept {
		Pools.clear();
	}

	//! Checks if it is parsing a value at the top level
	bool inTop() const noexcept {
		return PrevStackSize == 0;
	}

	//! Checks if it is parsing a value inside an array
	bool inArray() const noexcept {
		assert(PrevStackSize > 0);
		return StackStart[PrevStackSize - 1].getType() == JsopValue::PartialArrayType;
	}

	//! Checks if it is parsing a value inside an object
	bool inObject() const noexcept {
		assert(PrevStackSize > 0);
		return StackStart[PrevStackSize - 1].getType() == JsopValue::PartialObjectType;
	}

	JSOP_INLINE bool makeNull() noexcept {
		auto new_value = makeValue();
		if (new_value != nullptr) {
			new_value->setNull();
			return true;
		} else {
			return false;
		}
	}

	JSOP_INLINE bool makeBool(bool value) noexcept {
		auto new_value = makeValue();
		if (new_value != nullptr) {
			new_value->setBool(value);
			return true;
		} else {
			return false;
		}
	}

#if JSOP_WORD_SIZE == 64
	//! Makes an integer if possible, by checking if it stays within the range of
	//! a 64-bit signed/unsigned value
	//! If the value is outside the range, makes a double precision value instead
	JSOP_INLINE bool makeInteger(uint64_t value, bool negative) noexcept {
		auto new_value = makeValue();
		if (new_value != nullptr) {
			if (!negative) {
				if (value <= INT64_MAX) {
					new_value->setInt64(static_cast<int64_t>(value));
				} else {
					new_value->setUint64(value);
				}
			} else {
				if (value <= (UINT64_C(1) << 63)) {
					new_value->setInt64(-static_cast<int64_t>(value));
				} else {
					new_value->setDouble(-static_cast<double>(value));
				}
			}
			return true;
		}
		return false;
	}
#else
	//! Makes an integer if possible, by checking if it stays within the range of
	//! a 32-bit/64-bit signed/unsigned value
	//! If the value is outside the range, makes a double precision value instead
	bool makeInteger(uint64_t value, bool negative) noexcept;
#endif

	JSOP_INLINE bool makeDouble(double value) noexcept {
		auto new_value = makeValue();
		if (new_value != nullptr) {
#if JSOP_WORD_SIZE == 64
			new_value->setDouble(value);
			return true;
#else
			auto new_double = Pools.alloc<double>();
			if (new_double != nullptr) {
				*new_double = value;
				new_value->setDouble(new_double);
				return true;
			}
			new_value->setNull();
#endif
		}
		return false;
	}

	//! Makes a null-terminated string indicated by the (start, end) pair
	bool makeString(const char *start, const char *end) noexcept;

	//! Makes a new array, and push the context to add subsequent values to the array
	JSOP_INLINE bool pushArray() noexcept {
		auto new_value = makeValue();
		if (new_value != nullptr) {
			new_value->setPartialArray(PrevStackSize);
			PrevStackSize = StackEnd - StackStart;
			return true;
		}
		return false;
	}

	//! Finish parsing of an array and return to the previous context
	bool popArray() noexcept;

	//! Makes a new array, and push the context to add subsequent (key, value) pairs to the array
	JSOP_INLINE bool pushObject() noexcept {
		auto new_value = makeValue();
		if (new_value != nullptr) {
			new_value->setPartialObject(PrevStackSize);
			PrevStackSize = StackEnd - StackStart;
			return true;
		}
		return false;
	}

	//! Finish parsing of an object and return to the previous context
	bool popObject() noexcept;
};

#endif
