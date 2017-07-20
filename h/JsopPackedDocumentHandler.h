//This Source Code Form is subject to the terms of the Mozilla Public
//License, v. 2.0. If a copy of the MPL was not distributed with this
//file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef JSOP_PACKED_DOCUMENT_HANDLER_H
#define JSOP_PACKED_DOCUMENT_HANDLER_H

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <limits>
#include <utility>

#include "JsopDefines.h"
#include "JsopPackedValue.h"

template <class IO>
class JsopPackedDocumentHandler : public IO {
	typedef typename IO::value_type value_type;
	typedef typename IO::size_type size_type;
	typedef typename value_type::TinyString TinyString;
	typedef typename value_type::SmallString SmallString;
	typedef typename value_type::String String;
	typedef typename value_type::Array Array;
	typedef typename value_type::Object Object;

	value_type *StackStart = nullptr;
	value_type *StackEnd = nullptr;
	value_type *StackAllocEnd = nullptr;
	size_type PrevStackSize = 0;

	value_type *resizeStack() noexcept;

	//! Creates a value and push it to the stack
	//! The caller must initialize the value returned
	JSOP_INLINE value_type *makeValue() noexcept {
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

	JsopPackedDocumentHandler() = default;
	~JsopPackedDocumentHandler() noexcept {
		free(StackStart);
	}

	JsopPackedDocumentHandler(const JsopPackedDocumentHandler &) = delete;
	JsopPackedDocumentHandler &operator =(const JsopPackedDocumentHandler &) = delete;

	//! Check if strings need a null terminator character at the end
	constexpr static bool requireNullTerminator() noexcept {
		//Do not null terminate strings as it copies the string
		return false;
	}

	//! Initializes the parsing
	template <typename ... A>
	bool start(A && ... args) noexcept {
		if (IO::start(std::forward<A>(args) ...)) {
			auto stack_start = StackStart;
			if (stack_start == nullptr) {
				static_assert(JSOP_VALUE_STACK_MIN_SIZE % sizeof(value_type) == 0, "JSOP_VALUE_STACK_MIN_SIZE % sizeof(value_type) == 0");
				stack_start = static_cast<value_type *>(malloc(JSOP_VALUE_STACK_MIN_SIZE));
				if (stack_start != nullptr) {
					StackStart = stack_start;
					StackAllocEnd = stack_start + JSOP_VALUE_STACK_MIN_SIZE / sizeof(value_type);
				} else {
					return false;
				}
			}
			StackEnd = stack_start;
			PrevStackSize = 0;
			return true;
		} else {
			return false;
		}
	}

	//! Finish the parsing by copying the parsed values into the given document
	template <typename ... A>
	bool finish(A && ... args) noexcept {
		return IO::finish(*StackStart, std::forward<A>(args) ...);
	}

	//! Frees memory allocated for the parsed values
	void cleanup() noexcept {
	}

	//! Checks if it is parsing a value at the top level
	bool inTop() const noexcept {
		return PrevStackSize == 0;
	}

	//! Checks if it is parsing a value inside an array
	bool inArray() const noexcept {
		assert(PrevStackSize > 0);
		return StackStart[PrevStackSize - 1].isPartialArray();
	}

	//! Checks if it is parsing a value inside an object
	bool inObject() const noexcept {
		assert(PrevStackSize > 0);
		return StackStart[PrevStackSize - 1].isPartialObject();
	}

	bool makeNull() noexcept {
		auto new_value = makeValue();
		if (new_value != nullptr) {
			new_value->setNull();
			return true;
		} else {
			return false;
		}
	}

	bool makeBool(bool value) noexcept {
		auto new_value = makeValue();
		if (new_value != nullptr) {
			new_value->setBool(value);
			return true;
		} else {
			return false;
		}
	}

	bool makeInteger(uint64_t value, bool negative) noexcept;

	bool makeDouble(value_type *new_value, double value) noexcept {
		if (sizeof(size_type) >= sizeof(double)) {
			union {
				uint64_t Uint64Value;
				double DoubleValue;
			} u;
			u.DoubleValue = value;
			auto e = (u.Uint64Value >> (DBL_MANT_DIG - 1)) & ((1 << (sizeof(double) * CHAR_BIT - DBL_MANT_DIG)) - 1);
			if (e >= ((2 - DBL_MIN_EXP) - value_type::PACKED_DOUBLE_EXPONENT_BIAS) && e <= ((2 - DBL_MIN_EXP) + (value_type::PACKED_DOUBLE_EXPONENT_BIAS + 1))) {
				new_value->setPackedDouble(value);
				return true;
			}
		}
		*new_value = IO::writeDouble(value);
		return !(new_value->isNull());
	}

	bool makeDouble(double value) noexcept {
		auto new_value = makeValue();
		if (new_value != nullptr) {
			return makeDouble(new_value, value);
		}
		return false;
	}

	//! Makes a null-terminated string indicated by the (start, end) pair
	bool makeString(const char *start, const char *end) noexcept;
	bool makeString(const char *start, const char *end, bool) noexcept {
		return makeString(start, end);
	}

	//! Makes a new array, and push the context to add subsequent values to the array
	bool pushArray() noexcept {
		auto new_value = makeValue();
		if (new_value != nullptr) {
			new_value->setPartialArray(PrevStackSize);
			size_t new_stack_size = static_cast<size_t>(StackEnd - StackStart);
			PrevStackSize = static_cast<size_type>(new_stack_size);
			return ((SIZE_MAX / sizeof(value_type)) < value_type::MAX_SIZE) || (new_stack_size < value_type::MAX_SIZE);
		} else {
			return false;
		}
	}

	//! Finish parsing of an array and return to the previous context
	bool popArray() noexcept {
		assert((StackEnd - StackStart) >= PrevStackSize && PrevStackSize > 0);

		auto values_start = StackStart + PrevStackSize;
		if (JSOP_LIKELY(values_start[-1].isPartialArray())) {
			size_t n = static_cast<size_t>(StackEnd - values_start);
			auto new_value = IO::writeArray(n, values_start);
			if (!(new_value.isNull())) {
				StackEnd = values_start;

				PrevStackSize = values_start[-1].getOffset();
				values_start[-1] = new_value;
				return true;
			}
		}
		return false;
	}

	//! Makes a new object, and push the context to add subsequent (key, value) pairs to the object
	bool pushObject() noexcept {
		auto new_value = makeValue();
		if (new_value != nullptr) {
			new_value->setPartialObject(PrevStackSize);
			size_t new_stack_size = static_cast<size_t>(StackEnd - StackStart);
			PrevStackSize = static_cast<size_type>(new_stack_size);
			return ((SIZE_MAX / sizeof(value_type)) < value_type::MAX_SIZE) || (new_stack_size < value_type::MAX_SIZE);
		} else {
			return false;
		}
	}

	//! Finish parsing of an object and return to the previous context
	bool popObject() noexcept {
		assert((StackEnd - StackStart) >= PrevStackSize && PrevStackSize > 0);

		auto values_start = StackStart + PrevStackSize;
		if (JSOP_LIKELY(values_start[-1].isPartialObject())) {
			size_t n = static_cast<size_t>(StackEnd - values_start);
			assert((n % 2) == 0);
			size_t new_object_size = n / 2;
			auto new_value = IO::writeObject(new_object_size, values_start);
			if (!(new_value.isNull())) {
				StackEnd = values_start;

				PrevStackSize = values_start[-1].getOffset();
				values_start[-1] = new_value;
				return true;
			}
		}
		return false;
	}
};

template <class IO>
auto JsopPackedDocumentHandler<IO>::resizeStack() noexcept -> value_type * {
	auto *stack_start = StackStart;
	size_t capacity = StackAllocEnd - stack_start;
	if (capacity < (SIZE_MAX / (sizeof(value_type) * 2))) {
		capacity *= 2;
		auto *new_start = static_cast<value_type *>(realloc(stack_start, capacity * sizeof(value_type)));
		if (new_start != nullptr) {
			StackStart = new_start;
			auto *new_stack_end = new_start + (StackEnd - stack_start);
			StackEnd = new_stack_end;
			StackAllocEnd = new_start + capacity;
			return new_stack_end;
		}
	}
	return nullptr;
}

template <class IO>
bool JsopPackedDocumentHandler<IO>::makeInteger(uint64_t value, bool negative) noexcept {
	auto new_value = makeValue();
	if (new_value != nullptr) {
		if (!negative) {
			if (value < (static_cast<size_type>(1) << (sizeof(size_type) * CHAR_BIT - value_type::VALUE_TYPE_NUMBER_OF_BITS - 1))) {
				new_value->setPackedInt(static_cast<int64_t>(value));
				return true;
			} else if (value < (static_cast<size_type>(1) << (sizeof(size_type) * CHAR_BIT - value_type::VALUE_TYPE_NUMBER_OF_BITS))) {
				new_value->setPackedUint(value);
				return true;
			} else if (value <= INT64_MAX) {
				*new_value = IO::writeInt64(value);
				return !(new_value->isNull());
			} else {
				*new_value = IO::writeUint64(value);
				return !(new_value->isNull());
			}
		} else {
			if (value <= (static_cast<size_type>(1) << (sizeof(size_type) * CHAR_BIT - value_type::VALUE_TYPE_NUMBER_OF_BITS - 1))) {
				new_value->setPackedInt(-static_cast<typename value_type::ssize_type>(value));
				return true;
			} else if (value <= (static_cast<size_type>(1) << (sizeof(size_type) * CHAR_BIT - 1))) {
				*new_value = IO::writeInt64(-static_cast<int64_t>(value));
				return !(new_value->isNull());
			} else {
				return makeDouble(new_value, -static_cast<double>(value));
			}
		}
	}
	return false;
}

template <class IO>
bool JsopPackedDocumentHandler<IO>::makeString(const char *start, const char *end) noexcept {
	auto new_value = makeValue();
	if (new_value != nullptr) {
		size_t n = static_cast<size_t>(end - start);
		if (JSOP_LIKELY(n <= std::numeric_limits<typename SmallString::size_type>::max())) {
			if (JSOP_LIKELY(n > (sizeof(size_type) - sizeof(typename TinyString::size_type) - sizeof(char)) / sizeof(char))) {
				*new_value = IO::writeSmallString(n, start);
				return !(new_value->isNull());
			} else {
				*new_value = value_type::makeTinyString(n, start);
				return true;
			}
		} else {
			*new_value = IO::writeString(n, start);
			return !(new_value->isNull());
		}
	}
	return false;
}

#endif
