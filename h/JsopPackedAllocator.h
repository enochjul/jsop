//This Source Code Form is subject to the terms of the Mozilla Public
//License, v. 2.0. If a copy of the MPL was not distributed with this
//file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef JSOP_PACKED_ALLOCATOR_H
#define JSOP_PACKED_ALLOCATOR_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <limits>

#include "JsopPackedDocument.h"

template <
	class ValueType,
	bool MinimumAlignmentOnly = false,
	bool PadWithZero = false,
	size_t DefaultSize = 65536,
	bool RootFirst = true>
class JsopPackedAllocator {
public:
	typedef ValueType value_type;
	typedef typename value_type::size_type size_type;
	typedef typename value_type::SmallString SmallString;
	typedef typename value_type::String String;
	typedef typename value_type::Array Array;
	typedef typename value_type::Object Object;

private:
	void *Start = nullptr;
	void *End = nullptr;
	size_t FreeSize = 0;

	enum : size_t {
		MINIMUM_ALIGNMENT = value_type::MINIMUM_ALIGNMENT,
		MAX_ALLOC_SIZE = MINIMUM_ALIGNMENT < (1 << value_type::VALUE_TYPE_NUMBER_OF_BITS) ?
			(static_cast<size_t>(1) << ((sizeof(size_t) <= sizeof(size_type) ? sizeof(size_t) : sizeof(size_type)) * CHAR_BIT - value_type::VALUE_TYPE_NUMBER_OF_BITS)) * MINIMUM_ALIGNMENT :
			((sizeof(size_t) <= sizeof(size_type) ? SIZE_MAX : std::numeric_limits<size_type>::max()) - (alignof(max_align_t) <= MINIMUM_ALIGNMENT ? MINIMUM_ALIGNMENT : alignof(max_align_t)) + 1) & ~((alignof(max_align_t) <= MINIMUM_ALIGNMENT ? MINIMUM_ALIGNMENT : alignof(max_align_t)) - 1),
	};
	static_assert(MAX_ALLOC_SIZE % alignof(max_align_t) == 0, "MAX_ALLOC_SIZE % alignof(max_align_t) == 0");
	static_assert(MAX_ALLOC_SIZE % MINIMUM_ALIGNMENT == 0, "MAX_ALLOC_SIZE % MINIMUM_ALIGNMENT == 0");

	template <size_t TypeAlignment>
	void *resize_and_allocate(size_t n) noexcept;

	template <size_t TypeAlignment>
	JSOP_INLINE void *allocate(size_t n) noexcept {
		static_assert(TypeAlignment <= alignof(max_align_t), "TypeAlignment <= alignof(max_align_t)");

		auto free_size = FreeSize;
		auto aligned_free_size = (free_size / TypeAlignment) * TypeAlignment;
		if (JSOP_LIKELY(n <= aligned_free_size)) {
			auto padding_size = free_size % TypeAlignment;
			auto padding = static_cast<uint8_t *>(End);
			if (PadWithZero) {
				if (padding_size > 0) {
					auto padding_end = padding + padding_size;
					do {
						*padding = 0;
						++padding;
					} while (padding < padding_end);
				}
			} else {
				padding += padding_size;
			}
			End = padding + n;
			FreeSize = aligned_free_size - n;
			return padding;
		} else {
			return resize_and_allocate<TypeAlignment>(n);
		}
	}

	template <typename T>
	JSOP_INLINE T *try_alloc() noexcept {
		constexpr size_t alignment = (MinimumAlignmentOnly || alignof(T) <= MINIMUM_ALIGNMENT) ? MINIMUM_ALIGNMENT : alignof(T);
		return static_cast<T *>(allocate<alignment>(sizeof(T)));
	}

	template <typename T, size_t sentinel_n = 0>
	JSOP_INLINE T *try_alloc_flexible_array(size_t n) noexcept {
		constexpr size_t alignment = (MinimumAlignmentOnly || alignof(T) <= MINIMUM_ALIGNMENT) ? MINIMUM_ALIGNMENT : alignof(T);

		if (n <= (((MAX_ALLOC_SIZE - T::sizeofHeader()) / sizeof(typename T::value_type)) - sentinel_n)) {
			size_t size = n * sizeof(typename T::value_type) + (sentinel_n * sizeof(typename T::value_type) + T::sizeofHeader());
			return static_cast<T *>(allocate<alignment>(size));
		}
		return nullptr;
	}

public:
	JsopPackedAllocator() = default;
	~JsopPackedAllocator() noexcept {
		free(Start);
	}

	JsopPackedAllocator(const JsopPackedAllocator &) = delete;
	JsopPackedAllocator &operator =(const JsopPackedAllocator &) = delete;

	bool start() noexcept {
		auto *start = Start;
		if (start == nullptr) {
			//Allocate a new memory block with the default size
			start = malloc(DefaultSize);
			if (start != nullptr) {
				Start = start;
				if (RootFirst) {
					End = static_cast<uint8_t *>(start) + sizeof(value_type);
					FreeSize = DefaultSize - sizeof(value_type);
				} else {
					End = start;
					FreeSize = DefaultSize;
				}
				return true;
			}
		} else {
			//Calculate the current capacity of the memory block
			size_t capacity = reinterpret_cast<uintptr_t>(End) - reinterpret_cast<uintptr_t>(start) + FreeSize;
			if (RootFirst) {
				End = static_cast<uint8_t *>(start) + sizeof(value_type);
				FreeSize = capacity - sizeof(value_type);
			} else {
				End = start;
				FreeSize = capacity;
			}
			return true;
		}
		return false;
	}
	bool finish(value_type value, JsopPackedDocument<value_type, RootFirst> *doc) noexcept {
		if (!RootFirst) {
			auto *new_value = try_alloc<value_type>();
			if (new_value != nullptr) {
				*new_value = value;
			} else {
				return false;
			}
		}

		//Shrink the allocated memory block to the exact size
		auto *start = static_cast<value_type *>(Start);
		uintptr_t n = reinterpret_cast<uintptr_t>(End) - reinterpret_cast<uintptr_t>(start);
		auto *new_start = static_cast<value_type *>(realloc(start, n));
		if (new_start != nullptr) {
			start = new_start;
		}

		//Set the root and transfer ownership to the document
		if (RootFirst) {
			*start = value;
		}
		doc->set(start, reinterpret_cast<char *>(start) + n);

		//Reset to allocate a new memory block on next parse
		Start = nullptr;
		End = nullptr;
		FreeSize = 0;
		return true;
	}
	void cleanup() noexcept {
		//Don't do anything here; next parse will reuse the memory block
	}

	template <JsopPackedValueType type, typename T>
	JSOP_INLINE value_type writeValue(T value) noexcept {
		auto *new_value = try_alloc<T>();
		if (new_value != nullptr) {
			*new_value = value;
			return value_type::make(
				type,
				(reinterpret_cast<uintptr_t>(new_value) - reinterpret_cast<uintptr_t>(Start)) / MINIMUM_ALIGNMENT);
		} else {
			return value_type::makeNull();
		}
	}

	JSOP_INLINE value_type writeInt64(int64_t value) noexcept {
		return writeValue<JsopPackedValueType::FullInt64, int64_t>(value);
	}
	JSOP_INLINE value_type writeUint64(uint64_t value) noexcept {
		return writeValue<JsopPackedValueType::FullUint64, uint64_t>(value);
	}
	JSOP_INLINE value_type writeDouble(double value) noexcept {
		return writeValue<JsopPackedValueType::FullDouble, double>(value);
	}

	JSOP_INLINE value_type writeSmallString(size_t n, const char *s) noexcept {
		auto *new_value = try_alloc_flexible_array<SmallString, 1>(n);
		if (new_value != nullptr) {
			new_value->Size = static_cast<typename SmallString::size_type>(n);
			memcpy(new_value->Data, s, n * sizeof(char));
			new_value->Data[n] = '\0';
			return value_type::make(
				JsopPackedValueType::SmallString,
				(reinterpret_cast<uintptr_t>(new_value) - reinterpret_cast<uintptr_t>(Start)) / MINIMUM_ALIGNMENT);
		} else {
			return value_type::makeNull();
		}
	}
	JSOP_INLINE value_type writeString(size_t n, const char *s) noexcept {
		auto *new_value = try_alloc_flexible_array<String, 1>(n);
		if (new_value != nullptr) {
			new_value->Size = static_cast<typename String::size_type>(n);
			memcpy(new_value->Data, s, n * sizeof(char));
			new_value->Data[n] = '\0';
			return value_type::make(
				JsopPackedValueType::String,
				(reinterpret_cast<uintptr_t>(new_value) - reinterpret_cast<uintptr_t>(Start)) / MINIMUM_ALIGNMENT);
		} else {
			return value_type::makeNull();
		}
	}
	JSOP_INLINE value_type writeArray(size_t n, const value_type *values) noexcept {
		auto *new_value = try_alloc_flexible_array<Array>(n);
		if (new_value != nullptr) {
			new_value->Size = static_cast<typename Array::size_type>(n);
			memcpy(new_value->Data, values, n * sizeof(typename Array::value_type));
			return value_type::make(
				JsopPackedValueType::Array,
				(reinterpret_cast<uintptr_t>(new_value) - reinterpret_cast<uintptr_t>(Start)) / MINIMUM_ALIGNMENT);
		} else {
			return value_type::makeNull();
		}
	}
	JSOP_INLINE value_type writeObject(size_t n, const value_type *key_values) noexcept {
		auto *new_value = try_alloc_flexible_array<Object>(n);
		if (new_value != nullptr) {
			new_value->Size = static_cast<typename Object::size_type>(n);
			memcpy(new_value->Data, key_values, n * sizeof(typename Object::value_type));
			return value_type::make(
				JsopPackedValueType::Object,
				(reinterpret_cast<uintptr_t>(new_value) - reinterpret_cast<uintptr_t>(Start)) / MINIMUM_ALIGNMENT);
		} else {
			return value_type::makeNull();
		}
	}
};

template <class ValueType, bool MinimumAlignmentOnly, bool PadWithZero, size_t DefaultSize, bool RootFirst>
template <size_t TypeAlignment>
void *JsopPackedAllocator<ValueType, MinimumAlignmentOnly, PadWithZero, DefaultSize, RootFirst>::resize_and_allocate(size_t n) noexcept {
	static_assert(TypeAlignment <= alignof(max_align_t), "TypeAlignment <= alignof(max_align_t)");

	auto free_size = FreeSize;
	size_t alloc_size = static_cast<uint8_t *>(End) - static_cast<uint8_t *>(Start);
	auto aligned_alloc_size = alloc_size + (free_size % TypeAlignment);
	if (n <= MAX_ALLOC_SIZE - aligned_alloc_size) {
		auto new_alloc_size = aligned_alloc_size + n;
		auto new_capacity = alloc_size + free_size;
		if (JSOP_LIKELY(new_capacity <= MAX_ALLOC_SIZE / 2)) {
			new_capacity *= 2;
			if (JSOP_UNLIKELY(new_capacity < new_alloc_size)) {
				new_capacity = ((new_alloc_size + alignof(max_align_t) - 1) / alignof(max_align_t)) * alignof(max_align_t);
			}
		} else {
			if (sizeof(size_t) > sizeof(size_type) || MINIMUM_ALIGNMENT < (1 << value_type::VALUE_TYPE_NUMBER_OF_BITS)) {
				new_capacity = MAX_ALLOC_SIZE;
			} else {
				new_capacity = ((new_alloc_size + alignof(max_align_t) - 1) / alignof(max_align_t)) * alignof(max_align_t);
			}
		}

		auto new_start = realloc(Start, new_capacity);
		if (new_start != nullptr) {
			auto *new_aligned_end = static_cast<uint8_t *>(new_start) + aligned_alloc_size;
			if (PadWithZero) {
				for (auto *new_end = static_cast<uint8_t *>(new_start) + alloc_size; new_end < new_aligned_end; ++new_end) {
					*new_end = 0;
				}
			}
			Start = new_start;
			End = static_cast<uint8_t *>(new_start) + new_alloc_size;
			FreeSize = new_capacity - new_alloc_size;
			return new_aligned_end;
		}
	}
	return nullptr;
}

#endif
