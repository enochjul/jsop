//This Source Code Form is subject to the terms of the Mozilla Public
//License, v. 2.0. If a copy of the MPL was not distributed with this
//file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef JSOP_PACKED_FILE_H
#define JSOP_PACKED_FILE_H

#include <stddef.h>
#include <string.h>

#include <limits>
#include <type_traits>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

template <
	class ValueType,
	bool MinimumAlignmentOnly = false,
	bool PadWithZero = true,
	size_t BufferSize = 65536,
	size_t MaxWriteSize = 4194304>
class JsopPackedFile {
public:
	typedef ValueType value_type;
	typedef typename value_type::size_type size_type;
	typedef typename value_type::SmallString SmallString;
	typedef typename value_type::String String;
	typedef typename value_type::Array Array;
	typedef typename value_type::Object Object;

private:
	enum : size_t {
		MINIMUM_ALIGNMENT = value_type::MINIMUM_ALIGNMENT,
		MAX_ALIGNMENT = MINIMUM_ALIGNMENT < (1 << value_type::VALUE_TYPE_NUMBER_OF_BITS) ?
			1 << value_type::VALUE_TYPE_NUMBER_OF_BITS :
			MINIMUM_ALIGNMENT,
	};

	enum : size_type {
		TOTAL_SIZE = MINIMUM_ALIGNMENT < (1 << value_type::VALUE_TYPE_NUMBER_OF_BITS) ?
			(static_cast<size_type>(1) << (sizeof(size_type) * CHAR_BIT - value_type::VALUE_TYPE_NUMBER_OF_BITS)) * MINIMUM_ALIGNMENT :
			(std::numeric_limits<size_type>::max() / MAX_ALIGNMENT) * MAX_ALIGNMENT,
	};

	size_type WrittenSize;
	size_type WrittenBufferSize;
	size_t FreeBufferSize;
	int Handle;
	union {
		char Buffer[BufferSize];
		value_type ValueBuffer[BufferSize / sizeof(value_type)];
	};

	static_assert(BufferSize >= MaxWriteSize || ((BufferSize & (BufferSize - 1)) == 0), "BufferSize >= MaxWriteSize || ((BufferSize & (BufferSize - 1)) == 0)");
	static_assert(MaxWriteSize <= SSIZE_MAX, "MaxWriteSize <= SSIZE_MAX");

	static_assert(sizeof(double) == sizeof(int64_t), "sizeof(double) == sizeof(int64_t)");
	static_assert(BufferSize > 0, "BufferSize > 0");
	static_assert((BufferSize % MINIMUM_ALIGNMENT) == 0, "(BufferSize % MINIMUM_ALIGNMENT) == 0");
	static_assert((BufferSize % sizeof(int64_t)) == 0, "(BufferSize % sizeof(int64_t)) == 0");
	static_assert((BufferSize % sizeof(uint64_t)) == 0, "(BufferSize % sizeof(uint64_t)) == 0");
	static_assert((BufferSize % sizeof(double)) == 0, "(BufferSize % sizeof(double)) == 0");
	static_assert((BufferSize % alignof(SmallString)) == 0, "(BufferSize % alignof(SmallString)) == 0");
	static_assert((BufferSize % alignof(String)) == 0, "(BufferSize % alignof(String)) == 0");
	static_assert((BufferSize % alignof(Array)) == 0, "(BufferSize % alignof(Array)) == 0");
	static_assert((BufferSize % alignof(Object)) == 0, "(BufferSize % alignof(Object)) == 0");

	static ssize_t writeAll(int fd, const void *buffer, size_t n) noexcept {
		assert(n > 0);

		auto remaining_size = n;
		do {
			auto written_size = write(fd, buffer, remaining_size);
			if (written_size >= 0) {
				buffer = static_cast<const char *>(buffer) + static_cast<size_t>(written_size);
				remaining_size -= static_cast<size_t>(written_size);
			} else {
				return written_size;
			}
		} while (remaining_size > 0);
		assert(remaining_size == 0);
		return n;
	}

	template <typename T, size_t Alignment>
	bool writeSmallData(T value) noexcept {
		auto n = sizeof(T);
		auto free_buffer_size = FreeBufferSize;
		if (free_buffer_size >= n) {
			memcpy(Buffer + (BufferSize - free_buffer_size), &value, n);
			FreeBufferSize = free_buffer_size - n;
			return true;
		} else {
			if (Alignment >= sizeof(T) || free_buffer_size == 0) {
				if (JSOP_UNLIKELY(writeAll(Handle, Buffer, BufferSize) < 0)) {
					return false;
				}
				if (JSOP_LIKELY(WrittenBufferSize <= TOTAL_SIZE - BufferSize)) {
					WrittenBufferSize += BufferSize;
					FreeBufferSize = BufferSize - n;
					memcpy(Buffer, &value, n);
					return true;
				} else {
					return false;
				}
			} else {
				const void *src = &value;
				n -= free_buffer_size;
				memcpy(Buffer + (BufferSize - free_buffer_size), src, free_buffer_size);
				src = static_cast<const char *>(src) + free_buffer_size;
				if (JSOP_UNLIKELY(writeAll(Handle, Buffer, BufferSize) < 0)) {
					return false;
				}
				if (JSOP_LIKELY(WrittenBufferSize <= TOTAL_SIZE - BufferSize)) {
					WrittenBufferSize += BufferSize;
					FreeBufferSize = BufferSize - n;
					memcpy(Buffer, src, n);
					return true;
				} else {
					return false;
				}
			}
		}
	}
	bool writeData(const void *src, size_t n) noexcept;

public:
	JsopPackedFile() noexcept : WrittenSize(0), WrittenBufferSize(0), FreeBufferSize(BufferSize), Handle(-1) {
	}

	JsopPackedFile(const JsopPackedFile &) = delete;
	JsopPackedFile &operator =(const JsopPackedFile &) = delete;

	bool start(int handle) noexcept {
		if (handle >= 0) {
			WrittenSize = sizeof(value_type);
			WrittenBufferSize = 0;
			FreeBufferSize = BufferSize - sizeof(value_type);
			Handle = handle;
			ValueBuffer[0].setNull();
			return true;
		} else {
			return false;
		}
	}
	bool finish(value_type value) noexcept;
	void cleanup() noexcept {
		Handle = -1;
	}

	template <JsopPackedValueType type, typename T>
	value_type writeValue(T value) noexcept;
	value_type writeInt64(int64_t value) noexcept {
		return writeValue<JsopPackedValueType::FullInt64, int64_t>(value);
	}
	value_type writeUint64(uint64_t value) noexcept {
		return writeValue<JsopPackedValueType::FullUint64, uint64_t>(value);
	}
	value_type writeDouble(double value) noexcept {
		return writeValue<JsopPackedValueType::FullDouble, double>(value);
	}

	value_type writeSmallString(size_t n, const char *s) noexcept;
	value_type writeString(size_t n, const char *s) noexcept;

	value_type writeSizeData(JsopPackedValueType type, size_t n, size_t number_of_bytes, const void *data) noexcept;
	value_type writeArray(size_t n, const value_type *values) noexcept {
		static_assert(Array::sizeofHeader() == sizeof(size_type), "Array::sizeofHeader() == sizeof(size_type)");
		return writeSizeData(JsopPackedValueType::Array, n, n * sizeof(value_type), values);
	}
	value_type writeObject(size_t n, const value_type *key_values) noexcept {
		static_assert(Object::sizeofHeader() == sizeof(size_type), "Object::sizeofHeader() == sizeof(size_type)");
		return writeSizeData(JsopPackedValueType::Object, n, n * 2 * sizeof(value_type), key_values);
	}
};

template <class ValueType, bool MinimumAlignmentOnly, bool PadWithZero, size_t BufferSize, size_t MaxWriteSize>
bool JsopPackedFile<ValueType, MinimumAlignmentOnly, PadWithZero, BufferSize, MaxWriteSize>::writeData(const void *src, size_t n) noexcept {
	auto free_buffer_size = FreeBufferSize;
	if (free_buffer_size >= n) {
		memcpy(Buffer + (BufferSize - free_buffer_size), src, n);
		FreeBufferSize = free_buffer_size - n;
		return true;
	} else {
		auto handle = Handle;
		if (free_buffer_size < BufferSize) {
			if (free_buffer_size > 0) {
				n -= free_buffer_size;
				memcpy(Buffer + (BufferSize - free_buffer_size), src, free_buffer_size);
				src = static_cast<const char *>(src) + free_buffer_size;
			}
			if (JSOP_UNLIKELY(writeAll(handle, Buffer, BufferSize) < 0)) {
				return false;
			}
			if (JSOP_LIKELY(WrittenBufferSize <= TOTAL_SIZE - BufferSize)) {
				WrittenBufferSize += BufferSize;
			} else {
				return false;
			}
		}

		if (JSOP_LIKELY(((n / BufferSize) * BufferSize) <= TOTAL_SIZE - WrittenBufferSize)) {
			WrittenBufferSize += (n / BufferSize) * BufferSize;
		} else {
			return false;
		}

		if (BufferSize < MaxWriteSize) {
			while (n >= MaxWriteSize) {
				n -= MaxWriteSize;
				auto write_rv = writeAll(handle, src, MaxWriteSize);
				src = static_cast<const char *>(src) + MaxWriteSize;
				if (JSOP_UNLIKELY(write_rv < 0)) {
					return false;
				}
			}
			auto last_write_size = (n / BufferSize) * BufferSize;
			if (last_write_size > 0) {
				n -= last_write_size;
				auto write_rv = writeAll(handle, src, last_write_size);
				src = static_cast<const char *>(src) + last_write_size;
				if (JSOP_UNLIKELY(write_rv < 0)) {
					return false;
				}
			}
		} else {
			while (n >= BufferSize) {
				n -= BufferSize;
				auto write_rv = writeAll(handle, src, BufferSize);
				src = static_cast<const char *>(src) + BufferSize;
				if (JSOP_UNLIKELY(write_rv < 0)) {
					return false;
				}
			}
		}

		FreeBufferSize = BufferSize - n;
		memcpy(Buffer, src, n);
		return true;
	}
}

template <class ValueType, bool MinimumAlignmentOnly, bool PadWithZero, size_t BufferSize, size_t MaxWriteSize>
bool JsopPackedFile<ValueType, MinimumAlignmentOnly, PadWithZero, BufferSize, MaxWriteSize>::finish(value_type value) noexcept {
	auto handle = Handle;
	bool ok;

	//Write the remaining bytes in the buffer
	assert(FreeBufferSize <= BufferSize);
	size_t remaining_size = BufferSize - FreeBufferSize;
	if (remaining_size > 0) {
		ok = writeAll(handle, Buffer, remaining_size) >= 0;
		if (ok) {
			ok = WrittenBufferSize <= TOTAL_SIZE - remaining_size;
		}
	} else {
		ok = true;
	}
	//Write the root value
	if (ok) {
		ok = lseek64(handle, 0, SEEK_SET) == 0;
		if (ok) {
			ok = writeAll(handle, &value, sizeof(value)) >= 0;
		}
	}
	//Close the file
	Handle = -1;
	return ok;
}

template <class ValueType, bool MinimumAlignmentOnly, bool PadWithZero, size_t BufferSize, size_t MaxWriteSize>
template <JsopPackedValueType type, typename T>
auto JsopPackedFile<ValueType, MinimumAlignmentOnly, PadWithZero, BufferSize, MaxWriteSize>::writeValue(T value) noexcept -> value_type {
	constexpr size_t alignment = (MinimumAlignmentOnly || sizeof(T) <= MINIMUM_ALIGNMENT) ? MINIMUM_ALIGNMENT : sizeof(T);

	auto free_buffer_size = FreeBufferSize;
	size_t padding_size = free_buffer_size % alignment;
	size_type byte_offset = WrittenSize + padding_size;
	size_t aligned_free_buffer_size = (free_buffer_size / alignment) * alignment;

	//Adds zero padding if necessary
	if (alignment > 1 && PadWithZero) {
		for (auto start = Buffer + (BufferSize - free_buffer_size), end = Buffer + (BufferSize - aligned_free_buffer_size); start < end; ++start) {
			*start = 0;
		}
	}
	FreeBufferSize = aligned_free_buffer_size;

	//Write the data
	if (writeSmallData<T, alignment>(value)) {
		WrittenSize = byte_offset + sizeof(T);
		return value_type::make(type, byte_offset / MINIMUM_ALIGNMENT);
	}
	return value_type::makeNull();
}

template <class ValueType, bool MinimumAlignmentOnly, bool PadWithZero, size_t BufferSize, size_t MaxWriteSize>
auto JsopPackedFile<ValueType, MinimumAlignmentOnly, PadWithZero, BufferSize, MaxWriteSize>::writeSmallString(size_t n, const char *s) noexcept -> value_type {
	static_assert(SmallString::sizeofHeader() == sizeof(typename SmallString::size_type), "value_type::SmallString::sizeofHeader() == sizeof(typename value_type::SmallString::size_type)");

	constexpr size_t alignment = (MinimumAlignmentOnly || alignof(typename value_type::SmallString) <= MINIMUM_ALIGNMENT) ? MINIMUM_ALIGNMENT : alignof(typename value_type::SmallString);

	auto free_buffer_size = FreeBufferSize;
	size_t padding_size = free_buffer_size % alignment;
	size_type byte_offset = WrittenSize + padding_size;
	size_t aligned_free_buffer_size = (free_buffer_size / alignment) * alignment;

	//Adds zero padding if necessary
	if (alignment > 1 && PadWithZero) {
		for (auto start = Buffer + (BufferSize - free_buffer_size), end = Buffer + (BufferSize - aligned_free_buffer_size); start < end; ++start) {
			*start = 0;
		}
	}
	FreeBufferSize = aligned_free_buffer_size;

	//Write the data
	if (writeSmallData<typename SmallString::size_type, alignment>(static_cast<typename SmallString::size_type>(n)) &&
 		writeData(s, n * sizeof(char)) &&
		writeSmallData<char, 1>('\0')) {
		WrittenSize = byte_offset + SmallString::sizeofHeader() + (n + 1) * sizeof(char);
		return value_type::make(JsopPackedValueType::SmallString, byte_offset / MINIMUM_ALIGNMENT);
	}
	return value_type::makeNull();
}

template <class ValueType, bool MinimumAlignmentOnly, bool PadWithZero, size_t BufferSize, size_t MaxWriteSize>
auto JsopPackedFile<ValueType, MinimumAlignmentOnly, PadWithZero, BufferSize, MaxWriteSize>::writeString(size_t n, const char *s) noexcept -> value_type {
	static_assert(String::sizeofHeader() == sizeof(typename String::size_type), "String::sizeofHeader() == sizeof(String::size_type)");

	constexpr size_t alignment = (MinimumAlignmentOnly || alignof(String) <= MINIMUM_ALIGNMENT) ? MINIMUM_ALIGNMENT : alignof(String);

	auto free_buffer_size = FreeBufferSize;
	size_t padding_size = free_buffer_size % alignment;
	size_type byte_offset = WrittenSize + padding_size;
	size_t aligned_free_buffer_size = (free_buffer_size / alignment) * alignment;

	//Adds zero padding if necessary
	if (alignment > 1 && PadWithZero) {
		for (auto start = Buffer + (BufferSize - free_buffer_size), end = Buffer + (BufferSize - aligned_free_buffer_size); start < end; ++start) {
			*start = 0;
		}
	}
	FreeBufferSize = aligned_free_buffer_size;

	//Write the data
	if (writeSmallData<typename String::size_type, alignment>(static_cast<typename String::size_type>(n)) &&
 		writeData(s, n * sizeof(char)) &&
 		writeSmallData<char, 1>('\0')) {
		WrittenSize = byte_offset + String::sizeofHeader() + (n + 1) * sizeof(char);
		return value_type::make(JsopPackedValueType::String, byte_offset / MINIMUM_ALIGNMENT);
	}
	return value_type::makeNull();
}

template <class ValueType, bool MinimumAlignmentOnly, bool PadWithZero, size_t BufferSize, size_t MaxWriteSize>
auto JsopPackedFile<ValueType, MinimumAlignmentOnly, PadWithZero, BufferSize, MaxWriteSize>::writeSizeData(JsopPackedValueType type, size_t n, size_t number_of_bytes, const void *data) noexcept -> value_type {
	constexpr size_t alignment = (MinimumAlignmentOnly || alignof(size_type) <= MINIMUM_ALIGNMENT) ? MINIMUM_ALIGNMENT : alignof(size_type);

	auto free_buffer_size = FreeBufferSize;
	size_t padding_size = free_buffer_size % alignment;
	size_type byte_offset = WrittenSize + padding_size;
	size_t aligned_free_buffer_size = (free_buffer_size / alignment) * alignment;

	//Adds zero padding if necessary
	if (alignment > 1 && PadWithZero) {
		for (auto start = Buffer + (BufferSize - free_buffer_size), end = Buffer + (BufferSize - aligned_free_buffer_size); start < end; ++start) {
			*start = 0;
		}
	}
	FreeBufferSize = aligned_free_buffer_size;

	//Write the data
	if (writeSmallData<size_type, alignment>(static_cast<size_type>(n)) &&
 		writeData(data, number_of_bytes)) {
		WrittenSize = byte_offset + sizeof(size_type) + number_of_bytes;
		return value_type::make(type, byte_offset / MINIMUM_ALIGNMENT);
	}
	return value_type::makeNull();
}

#endif
