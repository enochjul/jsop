//This Source Code Form is subject to the terms of the Mozilla Public
//License, v. 2.0. If a copy of the MPL was not distributed with this
//file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef JSOP_PACKED_VALUE_H
#define JSOP_PACKED_VALUE_H

#include <assert.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include <type_traits>

#include "JsopDefines.h"

enum class JsopPackedValueType : unsigned {
	Null,
	Bool,
	PackedInt,
	PackedUint,
	FullInt64,
	FullUint64,
	PackedDouble,
	FullDouble,
	TinyString,
	SmallString,
	String,
	Array,
	Object,
	MaxType = Object
};

template <typename SizeType = uint32_t, size_t MinimumAlignment = 1>
class JsopPackedValue;

struct JsopPackedSmallString final {
	typedef uint8_t size_type;
	typedef char value_type;

	size_type Size;
	value_type Data[];

	constexpr static size_t sizeofHeader() noexcept {
		return offsetof(JsopPackedSmallString, Data);
	}
};
static_assert(sizeof(JsopPackedSmallString) == sizeof(uint8_t), "sizeof(JsopPackedSmallString) == sizeof(uint8_t)");

template <typename SizeType>
struct JsopPackedString final {
	typedef SizeType size_type;
	typedef char value_type;

	size_type Size;
	value_type Data[];

	constexpr static size_t sizeofHeader() noexcept {
		return offsetof(JsopPackedString<SizeType>, Data);
	}
};

//! Represents a view to a string that does not include a terminating null character
struct JsopPackedStringView final {
public:
	typedef size_t size_type;
	typedef char value_type;
	typedef const value_type *const_iterator;

private:
	const value_type *Start;
	const value_type *Finish;

public:
	constexpr JsopPackedStringView(const value_type *start, const value_type *finish) noexcept : Start(start), Finish(finish) {
	}

	size_type size() const noexcept {
		return Finish - Start;
	}

	const_iterator begin() const noexcept {
		return Start;
	}

	const_iterator end() const noexcept {
		return Finish;
	}

	const value_type &operator [](size_type i) const noexcept {
		assert(i < size());
		return Start[i];
	}
};

template <typename SizeType, size_t MinimumAlignment>
struct JsopPackedArray;

template <typename SizeType, size_t MinimumAlignment>
class JsopPackedArrayView final {
public:
	typedef size_t size_type;
	typedef JsopPackedValue<SizeType, MinimumAlignment> value_type;
	typedef const value_type *const_iterator;

private:
	const value_type *Start;
	const value_type *Finish;

public:
	constexpr JsopPackedArrayView(const value_type *start, const value_type *finish) noexcept : Start(start), Finish(finish) {
	}

	JSOP_INLINE size_type size() const noexcept;

	const_iterator begin() const noexcept {
		return Start;
	}

	const_iterator end() const noexcept {
		return Finish;
	}

	JSOP_INLINE const value_type &operator [](size_type i) const noexcept;
};

template <typename SizeType, size_t MinimumAlignment>
struct JsopPackedKeyValue;

template <typename SizeType, size_t MinimumAlignment>
struct JsopPackedObject;

template <typename SizeType, size_t MinimumAlignment>
class JsopPackedObjectView final {
public:
	typedef size_t size_type;
	typedef JsopPackedKeyValue<SizeType, MinimumAlignment> value_type;
	typedef const value_type *const_iterator;

private:
	const value_type *Start;
	const value_type *Finish;

public:
	constexpr JsopPackedObjectView(const value_type *start, const value_type *finish) noexcept : Start(start), Finish(finish) {
	}

	JSOP_INLINE size_type size() const noexcept;

	const_iterator begin() const noexcept {
		return Start;
	}

	const_iterator end() const noexcept {
		return Finish;
	}

	JSOP_INLINE const value_type &operator [](size_type i) const noexcept;
};

template <typename SizeType, size_t MinimumAlignment>
class JsopPackedValue final {
public:
	typedef JsopPackedValue<SizeType, MinimumAlignment> this_type;
	typedef SizeType size_type;
	typedef typename std::make_signed<size_type>::type ssize_type;
	typedef JsopPackedSmallString SmallString;
	typedef JsopPackedString<SizeType> String;
	typedef JsopPackedArray<SizeType, MinimumAlignment> Array;
	typedef JsopPackedObject<SizeType, MinimumAlignment> Object;

	enum : size_t {
		MINIMUM_ALIGNMENT = MinimumAlignment,
	};

	enum : size_type {
		VALUE_TYPE_NUMBER_OF_BITS = 4,
		MAX_SIZE = static_cast<size_type>(1) << (sizeof(size_type) * CHAR_BIT - VALUE_TYPE_NUMBER_OF_BITS),
	};

	struct TinyString {
		typedef uint8_t size_type;
		typedef char value_type;

		size_type TypeAndSize;
		value_type Data[sizeof(SizeType) - sizeof(size_type)];

		size_t size() const noexcept {
			return TypeAndSize >> VALUE_TYPE_NUMBER_OF_BITS;
		}
	};

	static_assert(static_cast<unsigned>(JsopPackedValueType::MaxType) < (1 << VALUE_TYPE_NUMBER_OF_BITS), "static_cast<unsigned>(JsopPackedValueType::MaxType) < (1 << VALUE_TYPE_NUMBER_OF_BITS)");
	static_assert(std::is_unsigned<SizeType>::value, "std::is_unsigned<SizeType>::value");
	static_assert(sizeof(TinyString) == sizeof(SizeType), "sizeof(TinyString) == sizeof(SizeType)");
	static_assert(sizeof(String) == sizeof(SizeType), "sizeof(String) == sizeof(SizeType)");
	static_assert(MinimumAlignment > 0 && (MinimumAlignment & (MinimumAlignment - 1)) == 0, "MinimumAlignment > 0 && (MinimumAlignment & (MinimumAligment - 1)) == 0");

	enum : size_t {
		PACKED_DOUBLE_EXPONENT_BITS = sizeof(size_type) >= sizeof(double) ? sizeof(size_type) * CHAR_BIT - DBL_MANT_DIG - VALUE_TYPE_NUMBER_OF_BITS : 1,
		PACKED_DOUBLE_EXPONENT_BIAS = (1 << (PACKED_DOUBLE_EXPONENT_BITS - 1)) - 1,
	};

private:
	enum : uint64_t {
		DOUBLE_MANTISSA_MASK = (static_cast<uint64_t>(1) << (DBL_MANT_DIG - 1)) - 1,
		DOUBLE_EXPONENT_MASK = ((static_cast<uint64_t>(1) << (sizeof(double) * CHAR_BIT - DBL_MANT_DIG)) - 1) << (DBL_MANT_DIG - 1),
		DOUBLE_SIGN_MASK = UINT64_C(0x8000000000000000),
	};

	union {
		size_type Value;
		TinyString myString;
	};

public:
	static this_type make(JsopPackedValueType type, size_type offset) noexcept {
		this_type rv;

		rv.setTypeAndOffset(type, offset);
		return rv;
	}

	static this_type makeNull() noexcept {
		this_type rv;

		rv.setNull();
		return rv;
	}

	static this_type makeTinyString(size_t n, const char *s) noexcept {
		this_type rv;

		rv.setTinyString(n, s);
		return rv;
	}

	JsopPackedValueType getType() const noexcept {
		return static_cast<JsopPackedValueType>(Value & ((static_cast<size_type>(1) << VALUE_TYPE_NUMBER_OF_BITS) - 1));
	}

	size_type getOffset() const noexcept {
		return Value >> VALUE_TYPE_NUMBER_OF_BITS;
	}

	size_type getByteOffset() const noexcept {
		return getOffset() * MinimumAlignment;
	}

	template <typename T>
	const T *getPointer(const void *base) const noexcept {
		return reinterpret_cast<const T *>(static_cast<const uint8_t *>(base) + getByteOffset());
	}

	bool getBool() const noexcept {
		assert(getType() == JsopPackedValueType::Bool);
		return getOffset() != 0;
	}

	ssize_type getPackedInt() const noexcept {
		assert(getType() == JsopPackedValueType::PackedInt);
		return static_cast<ssize_type>(Value) >> VALUE_TYPE_NUMBER_OF_BITS;
	}

	size_type getPackedUint() const noexcept {
		assert(getType() == JsopPackedValueType::PackedUint);
		return Value >> VALUE_TYPE_NUMBER_OF_BITS;
	}

	int64_t getFullInt64(const void *base) const noexcept {
		assert(getType() == JsopPackedValueType::FullInt64);
		return *getPointer<int64_t>(base);
	}

	int64_t getFullUint64(const void *base) const noexcept {
		assert(getType() == JsopPackedValueType::FullUint64);
		return *getPointer<uint64_t>(base);
	}

	double getPackedDouble() const noexcept {
		assert(getType() == JsopPackedValueType::PackedDouble && sizeof(size_type) == sizeof(uint64_t));

		union {
			int64_t Int64Value;
			double DoubleValue;
		} value;
		int e;

		auto v = static_cast<int64_t>(Value);
		//extract and convert the packed exponent to full width
		e = static_cast<int>(v >> (DBL_MANT_DIG - 1 + VALUE_TYPE_NUMBER_OF_BITS));
		e = (e & ((1 << PACKED_DOUBLE_EXPONENT_BITS) - 1)) + (2 - DBL_MIN_EXP) - PACKED_DOUBLE_EXPONENT_BIAS;
		v >>= VALUE_TYPE_NUMBER_OF_BITS;
		v = (v & (~DOUBLE_EXPONENT_MASK)) | (static_cast<int64_t>(e) << (DBL_MANT_DIG - 1));
		value.Int64Value = v;
		return value.DoubleValue;
	}

	double getFullDouble(const void *base) const noexcept {
		return *getPointer<double>(base);
	}

	JsopPackedStringView getTinyStringView(const void *base) const noexcept {
		assert(getType() == JsopPackedValueType::TinyString);

		const char *start = myString.Data;
		const char *finish = start + myString.size();
		return JsopPackedStringView(start, finish);
	}

	JsopPackedStringView getSmallStringView(const void *base) const noexcept {
		assert(getType() == JsopPackedValueType::SmallString);

		const auto *s = getPointer<SmallString>(base);
		const char *start = s->Data;
		const char *finish = start + s->Size;
		return JsopPackedStringView(start, finish);
	}

	JsopPackedStringView getStringView(const void *base) const noexcept {
		assert(getType() == JsopPackedValueType::String);

		const auto *s = getPointer<String>(base);
		const char *start = s->Data;
		const char *finish = start + s->Size;
		return JsopPackedStringView(start, finish);
	}

	JSOP_INLINE JsopPackedArrayView<SizeType, MinimumAlignment> getArrayView(const void *base) const noexcept;
	JSOP_INLINE JsopPackedObjectView<SizeType, MinimumAlignment> getObjectView(const void *base) const noexcept;

	bool isNull() const noexcept {
		return getType() == JsopPackedValueType::Null;
	}

	bool isBool() const noexcept {
		return getType() == JsopPackedValueType::Bool;
	}

	bool isInteger() const noexcept {
		switch (getType()) {
		case JsopPackedValueType::PackedInt:
		case JsopPackedValueType::PackedUint:
		case JsopPackedValueType::FullInt64:
		case JsopPackedValueType::FullUint64:
			return true;

		default:
			return false;
		}
	}

	bool isSignedInteger() const noexcept {
		switch (getType()) {
		case JsopPackedValueType::PackedInt:
		case JsopPackedValueType::FullInt64:
			return true;

		default:
			return false;
		}
	}

	bool isUnsignedInteger() const noexcept {
		switch (getType()) {
		case JsopPackedValueType::PackedUint:
		case JsopPackedValueType::FullUint64:
			return true;

		default:
			return false;
		}
	}

	bool isDouble() const noexcept {
		switch (getType()) {
		case JsopPackedValueType::PackedDouble:
		case JsopPackedValueType::FullDouble:
			return true;

		default:
			return false;
		}
	}

	bool isString() const noexcept {
		switch (getType()) {
		case JsopPackedValueType::TinyString:
		case JsopPackedValueType::SmallString:
		case JsopPackedValueType::String:
			return true;

		default:
			return false;
		}
	}

	bool isArray() const noexcept {
		return getType() == JsopPackedValueType::Array;
	}

	bool isObject() const noexcept {
		return getType() == JsopPackedValueType::Object;
	}

	bool isPartialArray() const noexcept {
		return getType() == JsopPackedValueType::Array;
	}

	bool isPartialObject() const noexcept {
		return getType() == JsopPackedValueType::Object;
	}

	JSOP_INLINE size_type size(const void *base) const noexcept;

	void setTypeAndOffset(JsopPackedValueType type, size_type offset) noexcept {
		assert(offset < (static_cast<size_type>(1) << (sizeof(size_type) * CHAR_BIT - VALUE_TYPE_NUMBER_OF_BITS)));
		Value = (offset << VALUE_TYPE_NUMBER_OF_BITS) | static_cast<size_type>(type);
	}

	void setNull() noexcept {
		Value = static_cast<size_type>(JsopPackedValueType::Null);
	}

	void setBool(bool value) noexcept {
		setTypeAndOffset(JsopPackedValueType::Bool, static_cast<size_type>(value));
	}

	void setPackedInt(ssize_type value) noexcept {
		assert(value >= -(static_cast<ssize_type>(1) << (sizeof(size_type) * CHAR_BIT - VALUE_TYPE_NUMBER_OF_BITS - 1)) &&
			value < (static_cast<ssize_type>(1) << (sizeof(size_type) * CHAR_BIT - VALUE_TYPE_NUMBER_OF_BITS - 1)));
		Value = (value << VALUE_TYPE_NUMBER_OF_BITS) | static_cast<size_type>(JsopPackedValueType::PackedInt);
	}

	void setPackedUint(size_type value) noexcept {
		setTypeAndOffset(JsopPackedValueType::PackedUint, value);
	}

	void setFullInt64(size_type offset) noexcept {
		setTypeAndOffset(JsopPackedValueType::FullInt64, offset);
	}

	void setFullUint64(size_type offset) noexcept {
		setTypeAndOffset(JsopPackedValueType::FullUint64, offset);
	}

	void setPackedDouble(double value) noexcept {
		union {
			uint64_t Uint64Value;
			double DoubleValue;
		} u;

		assert(sizeof(size_type) == sizeof(uint64_t));

		u.DoubleValue = value;
		auto v = u.Uint64Value;
		auto e = static_cast<unsigned>(v >> (DBL_MANT_DIG - 1)) & ((1 << (sizeof(double) * CHAR_BIT - DBL_MANT_DIG)) - 1);
		e = e + PACKED_DOUBLE_EXPONENT_BIAS - (2 - DBL_MIN_EXP);
		Value = ((v & DOUBLE_MANTISSA_MASK) << VALUE_TYPE_NUMBER_OF_BITS) | (static_cast<uint64_t>(e) << (DBL_MANT_DIG - 1 + VALUE_TYPE_NUMBER_OF_BITS)) | (v & DOUBLE_SIGN_MASK) | static_cast<size_type>(JsopPackedValueType::PackedDouble);
	}

	void setFullDouble(size_type offset) noexcept {
		setTypeAndOffset(JsopPackedValueType::FullDouble, offset);
	}

	void setTinyString(size_t n, const char *s) noexcept {
		assert(n <= ((sizeof(size_type) - sizeof(typename TinyString::size_type) - sizeof(char)) / sizeof(char)));

		Value = (n << VALUE_TYPE_NUMBER_OF_BITS) | static_cast<size_type>(JsopPackedValueType::TinyString);
		if (n > 0) {
			char *data = myString.Data;
			for (size_t i = 0; i < n; ++i) {
				data[i] = s[i];
			}
			data[n] = '\0';
		}
	}

	void setSmallString(size_type offset) noexcept {
		setTypeAndOffset(JsopPackedValueType::SmallString, offset);
	}

	void setString(size_type offset) noexcept {
		setTypeAndOffset(JsopPackedValueType::String, offset);
	}

	void setArray(size_type offset) noexcept {
		setTypeAndOffset(JsopPackedValueType::Array, offset);
	}

	void setObject(size_type offset) noexcept {
		setTypeAndOffset(JsopPackedValueType::Object, offset);
	}

	void setPartialArray(size_type value) noexcept {
		setTypeAndOffset(JsopPackedValueType::Array, value);
	}

	void setPartialObject(size_type value) noexcept {
		setTypeAndOffset(JsopPackedValueType::Object, value);
	}

	int64_t toInt64(const void *base) const noexcept {
		switch (getType()) {
		case JsopPackedValueType::Bool:
		case JsopPackedValueType::PackedInt:
			return static_cast<int64_t>(static_cast<ssize_type>(Value) >> VALUE_TYPE_NUMBER_OF_BITS);

		case JsopPackedValueType::PackedUint:
			return static_cast<int64_t>(getPackedUint());

		case JsopPackedValueType::FullInt64:
			return getFullInt64(base);

		case JsopPackedValueType::FullUint64:
			return static_cast<int64_t>(getFullUint64(base));

		case JsopPackedValueType::PackedDouble:
			return static_cast<int64_t>(getPackedDouble());

		case JsopPackedValueType::FullDouble:
			return static_cast<int64_t>(getFullDouble(base));

		default:
			return 0;
		}
	}

	uint64_t toUint64(const void *base) const noexcept {
		switch (getType()) {
		case JsopPackedValueType::Bool:
		case JsopPackedValueType::PackedInt:
			return static_cast<uint64_t>(static_cast<ssize_type>(Value) >> VALUE_TYPE_NUMBER_OF_BITS);

		case JsopPackedValueType::PackedUint:
			return static_cast<uint64_t>(getPackedUint());

		case JsopPackedValueType::FullInt64:
			return static_cast<uint64_t>(getFullInt64(base));

		case JsopPackedValueType::FullUint64:
			return getFullUint64(base);

		case JsopPackedValueType::PackedDouble:
			return static_cast<uint64_t>(getPackedDouble());

		case JsopPackedValueType::FullDouble:
			return static_cast<uint64_t>(getFullDouble(base));

		default:
			return 0;
		}
	}

	double toDouble(const void *base) const noexcept {
		switch (getType()) {
		case JsopPackedValueType::Bool:
		case JsopPackedValueType::PackedInt:
			return static_cast<double>(static_cast<ssize_type>(Value) >> VALUE_TYPE_NUMBER_OF_BITS);

		case JsopPackedValueType::PackedUint:
			return static_cast<double>(getPackedUint());

		case JsopPackedValueType::FullInt64:
			return static_cast<double>(getFullInt64(base));

		case JsopPackedValueType::FullUint64:
			return static_cast<double>(getFullUint64(base));

		case JsopPackedValueType::PackedDouble:
			return getPackedDouble();

		case JsopPackedValueType::FullDouble:
			return getFullDouble(base);

		default:
			return NAN;
		}
	}

	JsopPackedStringView toStringView(const void *base) const noexcept {
		const char *start;
		const char *finish;

		switch (getType()) {
		case JsopPackedValueType::TinyString:
			start = myString.Data;
			finish = start + myString.size();
			break;

		case JsopPackedValueType::SmallString: {
			const auto *s = getPointer<SmallString>(base);
			start = s->Data;
			finish = start + s->Size;
			break;
		}

		default: {
			assert(getType() == JsopPackedValueType::String);
			const auto *s = getPointer<String>(base);
			start = s->Data;
			finish = start + s->Size;
			break;
		}
		}
		return JsopPackedStringView(start, finish);
	}

	const char *c_str(const void *base) const noexcept {
		switch (getType()) {
		case JsopPackedValueType::TinyString:
			return myString.Data;

		case JsopPackedValueType::SmallString:
			return getPointer<SmallString>(base)->Data;

		default:
			assert(getType() == JsopPackedValueType::String);
			return getPointer<String>(base)->Data;
		}
	}
};

template <typename SizeType, size_t MinimumAlignment>
struct JsopPackedArray final {
	typedef JsopPackedArray<SizeType, MinimumAlignment> this_type;
	typedef SizeType size_type;
	typedef JsopPackedValue<SizeType, MinimumAlignment> value_type;

	static_assert(std::is_standard_layout<value_type>::value, "std::is_standard_layout<value_type>::value");

	size_type Size;
	value_type Data[];

	constexpr static size_t sizeofHeader() noexcept {
		return offsetof(this_type, Data);
	}
};

template <typename SizeType, size_t MinimumAlignment>
struct JsopPackedKeyValue {
	JsopPackedValue<SizeType, MinimumAlignment> Key;
	JsopPackedValue<SizeType, MinimumAlignment> Value;
};

template <typename SizeType, size_t MinimumAlignment>
struct JsopPackedObject final {
	typedef JsopPackedObject<SizeType, MinimumAlignment> this_type;
	typedef SizeType size_type;
	typedef JsopPackedKeyValue<SizeType, MinimumAlignment> value_type;

	static_assert(std::is_standard_layout<value_type>::value, "std::is_standard_layout<value_type>::value");

	size_type Size;
	value_type Data[];

	constexpr static size_t sizeofHeader() noexcept {
		return offsetof(this_type, Data);
	}
};

template <typename SizeType, size_t MinimumAlignment>
JSOP_INLINE JsopPackedArrayView<SizeType, MinimumAlignment> JsopPackedValue<SizeType, MinimumAlignment>::getArrayView(const void *base) const noexcept {
	assert(getType() == JsopPackedValueType::Array);

	const auto *array = getPointer<Array>(base);
	return JsopPackedArrayView<SizeType, MinimumAlignment>(array->Data, array->Data + array->Size);
}

template <typename SizeType, size_t MinimumAlignment>
JSOP_INLINE JsopPackedObjectView<SizeType, MinimumAlignment> JsopPackedValue<SizeType, MinimumAlignment>::getObjectView(const void *base) const noexcept {
	assert(getType() == JsopPackedValueType::Object);

	const auto *object = getPointer<Object>(base);
	return JsopPackedObjectView<SizeType, MinimumAlignment>(object->Data, object->Data + object->Size);
}

template <typename SizeType, size_t MinimumAlignment>
JSOP_INLINE auto JsopPackedValue<SizeType, MinimumAlignment>::size(const void *base) const noexcept -> size_type {
	static_assert(sizeof(String) == sizeof(size_type), "sizeof(String) == sizeof(size_type)");
	static_assert(sizeof(Array) == sizeof(size_type), "sizeof(Array) == sizeof(size_type)");
	static_assert(sizeof(Object) == sizeof(size_type), "sizeof(Object) == sizeof(size_type)");

	switch (getType()) {
	case JsopPackedValueType::TinyString:
		return myString.size();

	case JsopPackedValueType::SmallString:
		return getPointer<SmallString>(base)->Size;

	case JsopPackedValueType::String:
		return getPointer<String>(base)->Size;

	case JsopPackedValueType::Array:
		return getPointer<Array>(base)->Size;

	case JsopPackedValueType::Object:
		return getPointer<Object>(base)->Size;

	default:
		assert(false);
		return 0;
	}
}

template <typename SizeType, size_t MinimumAlignment>
JSOP_INLINE auto JsopPackedArrayView<SizeType, MinimumAlignment>::size() const noexcept -> size_type {
	return Finish - Start;
}

template <typename SizeType, size_t MinimumAlignment>
JSOP_INLINE auto JsopPackedArrayView<SizeType, MinimumAlignment>::operator [](size_type i) const noexcept -> const value_type & {
	assert(i < size());
	return Start[i];
}

template <typename SizeType, size_t MinimumAlignment>
JSOP_INLINE auto JsopPackedObjectView<SizeType, MinimumAlignment>::size() const noexcept -> size_type {
	return Finish - Start;
}

template <typename SizeType, size_t MinimumAlignment>
JSOP_INLINE auto JsopPackedObjectView<SizeType, MinimumAlignment>::operator [](size_type i) const noexcept -> const value_type & {
	assert(i < size());
	return Start[i];
}

#endif
