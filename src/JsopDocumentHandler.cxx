//This Source Code Form is subject to the terms of the Mozilla Public
//License, v. 2.0. If a copy of the MPL was not distributed with this
//file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <string.h>

#include "JsopDocument.h"
#include "JsopDocumentHandler.h"

JsopValue *JsopDocumentHandler::resizeStack() noexcept {
	JsopValue *new_start;
	size_t capacity, n;

	capacity = StackAllocEnd - StackStart;
	if (capacity <= SIZE_MAX / (sizeof(JsopValue) * 2)) {
		capacity *= 2;
		new_start = static_cast<JsopValue *>(realloc(StackStart, capacity * sizeof(JsopValue)));
		if (new_start != nullptr) {
			n = StackEnd - StackStart;
			StackStart = new_start;
			StackEnd = new_start + n;
			StackAllocEnd = new_start + capacity;
			return StackEnd;
		}
	}
	return nullptr;
}

bool JsopDocumentHandler::start() noexcept {
	if (StackStart == nullptr) {
		static_assert(JSOP_VALUE_STACK_MIN_SIZE % sizeof(JsopValue) == 0, "JSOP_VALUE_STACK_MIN_SIZE % sizeof(JsopValue) == 0");
		StackStart = static_cast<JsopValue *>(malloc(JSOP_VALUE_STACK_MIN_SIZE));
		if (StackStart == nullptr) {
			return false;
		}
		StackAllocEnd = StackStart + JSOP_VALUE_STACK_MIN_SIZE / sizeof(JsopValue);
	}
	StackEnd = StackStart;
	PrevStackSize = 0;
	return true;
}

bool JsopDocumentHandler::finish(JsopDocument *doc) noexcept {
	doc->Pools.move(&Pools);

	free(doc->Value);
	//Resize the stack to hold only the exact number of items
	auto stack_start = StackStart;
	if (StackAllocEnd > StackEnd) {
		stack_start = static_cast<JsopValue *>(realloc(StackStart, (StackEnd - StackStart) * sizeof(JsopValue)));
		if (stack_start != nullptr && stack_start != StackStart) {
			if ((stack_start->getType() == JsopValue::ArrayType ||
				stack_start->getType() == JsopValue::ObjectType) &&
				stack_start->getValues() != nullptr) {
				assert(stack_start->getValues() == StackStart + 1);
				stack_start->setValues(stack_start + 1);
			}
		}
	}
	doc->Value = stack_start;
	StackStart = nullptr;
	return true;
}

#if JSOP_WORD_SIZE != 64
bool JsopDocumentHandler::makeInteger(uint64_t value, bool negative) noexcept {
	auto new_value = makeValue();
	if (new_value != nullptr) {
		if (!negative) {
			if (value <= INT32_MAX) {
				new_value->setInt32(static_cast<int32_t>(value));
				return true;
			} else if (value <= UINT32_MAX) {
				new_value->setUint32(static_cast<uint32_t>(value));
				return true;
			} else if (value <= INT64_MAX) {
				auto new_int64 = Pools.alloc<int64_t>();
				if (new_int64 != nullptr) {
					*new_int64 = static_cast<int64_t>(value);
					new_value->setInt64(new_int64);
					return true;
				}
			} else {
				auto new_uint64 = Pools.alloc<uint64_t>();
				if (new_uint64 != nullptr) {
					*new_uint64 = value;
					new_value->setUint64(new_uint64);
					return true;
				}
			}
		} else {
			if (value <= (1U << 31)) {
				new_value->setInt32(-static_cast<int32_t>(value));
				return true;
			} else if (value <= (UINT64_C(1) << 63)) {
				auto new_int64 = Pools.alloc<int64_t>();
				if (new_int64 != nullptr) {
					*new_int64 = -static_cast<int64_t>(value);
					new_value->setInt64(new_int64);
					return true;
				}
			} else {
				auto new_double = Pools.alloc<double>();
				if (new_double != nullptr) {
					*new_double = -static_cast<double>(value);
					new_value->setDouble(new_double);
					return true;
				}
			}
		}
		new_value->setNull();
	}
	return false;
}
#endif

bool JsopDocumentHandler::makeString(const char *start, const char *end) noexcept {
	JsopValue *new_value;
	char *new_string;
	size_t n;

	new_value = makeValue();
	if (new_value != nullptr) {
		//Check the length of the string (including the null terminator) to determine
		//the storage mechanism
		n = end - start;
		if (n < (sizeof(size_t) / sizeof(char))) {
			//Small strings are store within the value itself
			new_value->setSmallString(n, start);
			return true;
		} else if (n <= JsopValue::MAX_SIZE) {
			//Allocate the string and store a pointer to the allocated value
			new_string = Pools.alloc<char>(n + 1);
			if (new_string != nullptr) {
				new_value->setString(n, new_string);
				//Copy the string and terminate it with the null character
				memcpy(new_string, start, n);
				new_string[n] = '\0';
				return true;
			}
		}
		//Either the length is too large or it cannot allocate the memory
		new_value->setNull();
	}
	return false;
}

bool JsopDocumentHandler::popArray() noexcept {
	JsopValue *new_values;
	size_t n;

	assert((StackEnd - StackStart) > 0 && (StackEnd - StackStart) >= PrevStackSize && PrevStackSize > 0);

	if (StackStart[PrevStackSize - 1].getType() == JsopValue::PartialArrayType) {
		n = static_cast<size_t>(StackEnd - StackStart) - PrevStackSize;
		if (n <= JsopValue::MAX_SIZE) {
			auto values_start = StackStart + PrevStackSize;
			new_values = nullptr;
			if (n > 0) {
				if (values_start[-1].getStackSize() > 0) {
					new_values = Pools.alloc<JsopValue>(n);
					if (new_values != nullptr) {
						memcpy(new_values, values_start, sizeof(JsopValue) * n);
					} else {
						return false;
					}
					StackEnd = values_start;
				} else {
					new_values = values_start;
				}
			}
			PrevStackSize = values_start[-1].getStackSize();
			values_start[-1].setArray(new_values, n);
			return true;
		}
	}
	return false;
}

bool JsopDocumentHandler::popObject() noexcept {
	JsopValue *new_values;
	size_t n, new_object_size;

	assert((StackEnd - StackStart) > 0 && (StackEnd - StackStart) >= PrevStackSize && PrevStackSize > 0);

	if (StackStart[PrevStackSize - 1].getType() == JsopValue::PartialObjectType) {
		n = static_cast<size_t>(StackEnd - StackStart) - PrevStackSize;
		assert((n % 2) == 0);
		new_object_size = n / 2;
		if (new_object_size <= JsopValue::MAX_SIZE) {
			auto values_start = StackStart + PrevStackSize;
			new_values = nullptr;
			if (n > 0) {
				if (values_start[-1].getStackSize() > 0) {
					new_values = Pools.alloc<JsopValue>(n);
					if (new_values != nullptr) {
						memcpy(new_values, values_start, sizeof(JsopValue) * n);
					} else {
						return false;
					}
					StackEnd = values_start;
				} else {
					new_values = values_start;
				}
			}
			PrevStackSize = values_start[-1].getStackSize();
			values_start[-1].setObject(new_values, new_object_size);
			return true;
		}
	}
	return false;
}
