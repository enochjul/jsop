//This Source Code Form is subject to the terms of the Mozilla Public
//License, v. 2.0. If a copy of the MPL was not distributed with this
//file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef JSOP_VALUE_H
#define JSOP_VALUE_H

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "JsopDefines.h"

class JsopValue;

struct JsopKeyValue;

//! Represents a view to a string that does not include a terminating null character
class JsopStringView final {
public:
	typedef size_t size_type;
	typedef char value_type;
	typedef const value_type *const_iterator;

private:
	const value_type *Start;
	const value_type *Finish;

public:
	constexpr JsopStringView(const char *start, const char *finish) noexcept : Start(start), Finish(finish) {
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

	const value_type *data() const noexcept {
		return Start;
	}
};

//! Represents a view to an array
class JsopArrayView final {
public:
	typedef size_t size_type;
	typedef JsopValue value_type;
	typedef const value_type *const_iterator;

private:
	const value_type *Start;
	const value_type *Finish;

public:
	constexpr JsopArrayView(const value_type *start, const value_type *finish) noexcept : Start(start), Finish(finish) {
	}

	JSOP_INLINE size_type size() const noexcept;

	const_iterator begin() const noexcept {
		return Start;
	}

	const_iterator end() const noexcept {
		return Finish;
	}

	JSOP_INLINE const value_type &operator [](size_type i) const noexcept;

	const value_type *data() const noexcept {
		return Start;
	}
};

class JsopObjectView final {
public:
	typedef size_t size_type;
	typedef JsopKeyValue value_type;
	typedef const value_type *const_iterator;

private:
	const value_type *Start;
	const value_type *Finish;

public:
	constexpr JsopObjectView(const value_type *start, const value_type *finish) noexcept : Start(start), Finish(finish) {
	}

	JSOP_INLINE size_type size() const noexcept;

	const_iterator begin() const noexcept {
		return Start;
	}

	const_iterator end() const noexcept {
		return Finish;
	}

	JSOP_INLINE const value_type &operator [](size_type i) const noexcept;

	const value_type *data() const noexcept {
		return Start;
	}
};

//! Represents a value using a type field and a union of all possible values
class JsopValue final {
public:
	enum ValueType {
		NullType,
		BoolType,
		Int32Type,
		Uint32Type,
		Int64Type,
		Uint64Type,
		DoubleType,
		SmallStringType,
		StringType,
		ArrayType,
		ObjectType,
		MaxType = ObjectType
	};
	static_assert(MaxType < (1 << 4), "MaxType < (1 << 4)");

	typedef size_t size_type;

	enum : size_type {
#if JSOP_WORD_SIZE == 64
		MAX_SIZE = (UINT64_C(1) << (64 - 4)) - 1
#else
		MAX_SIZE = (1U << (32 - 4)) - 1
#endif
	};

private:
	struct {
		size_type Type : 4;
		size_type Size : sizeof(size_type) * CHAR_BIT - 4;
	};
	union {
		JsopValue *Values;
		JsopKeyValue *KeyValues;
		const char *String;
		char SmallString[sizeof(size_t) / sizeof(char)];
#if JSOP_WORD_SIZE == 64
		int64_t Int64;
		double Double;
#else
		int32_t Int32;
		int64_t *Int64;
		uint64_t *Uint64;
		double *Double;
#endif
		size_t StackSize;
	};

public:
	ValueType getType() const noexcept {
		return static_cast<ValueType>(Type);
	}

	bool isNull() const noexcept {
		return Type == NullType;
	}

	bool isBool() const noexcept {
		return Type == BoolType;
	}

	JSOP_INLINE bool isInteger() const noexcept {
		switch (Type) {
#if JSOP_WORD_SIZE == 32
		case Int32Type:
		case Uint32Type:
#endif
		case Int64Type:
		case Uint64Type:
			return true;

		default:
			return false;
		}
	}

	JSOP_INLINE bool isSignedInteger() const noexcept {
		switch (Type) {
#if JSOP_WORD_SIZE == 32
		case Int32Type:
#endif
		case Int64Type:
			return true;

		default:
			return false;
		}
	}

	JSOP_INLINE bool isUnsignedInteger() const noexcept {
		switch (Type) {
#if JSOP_WORD_SIZE == 32
		case Uint32Type:
#endif
		case Uint64Type:
			return true;

		default:
			return false;
		}
	}

	bool isDouble() const noexcept {
		return Type == DoubleType;
	}

	JSOP_INLINE bool isString() const noexcept {
		switch (Type) {
		case SmallStringType:
		case StringType:
			return true;

		default:
			return false;
		}
	}

	bool isArray() const noexcept {
		return Type == ArrayType;
	}

	bool isObject() const noexcept {
		return Type == ObjectType;
	}

	const JsopValue *getValues() const noexcept {
		return Values;
	}

	JSOP_INLINE JsopArrayView getArrayView() const noexcept;
	JSOP_INLINE JsopObjectView getObjectView() const noexcept;

	JSOP_INLINE JsopStringView getStringView() const noexcept {
		const char *start;
		const char *finish;

		if (Type == SmallStringType) {
			start = SmallString;
			finish = start + Size;
		} else {
			assert(Type == StringType);
			start = String;
			finish = start + Size;
		}
		return JsopStringView(start, finish);
	}

	//! Gets the null-terminated string stored in the value
	const char *c_str() const noexcept {
		if (Type == SmallStringType) {
			return SmallString;
		} else {
			assert(Type == StringType);
			return String;
		}
	}

	bool getBool() const noexcept {
		assert(Type == BoolType);

#if JSOP_WORD_SIZE == 64
		return Int64 != 0;
#else
		return Int32 != 0;
#endif
	}

	double getDouble() const noexcept {
		assert(Type == DoubleType);
#if JSOP_WORD_SIZE == 64
		return Double;
#else
		return *Double;
#endif
	}

	JSOP_INLINE size_type size() const noexcept;

	size_t getStackSize() const noexcept {
		assert(Type == ArrayType || Type == ObjectType);
		return StackSize;
	}

	void setNull() noexcept {
		Type = NullType;
		Size = 0;
	}

	void setValues(JsopValue *value) noexcept {
		Values = value;
	}

	void setArray(JsopValue *value, size_type n) noexcept {
		Type = ArrayType;
		Size = n;
		Values = value;
	}

	void setObject(JsopValue *value, size_type n) noexcept {
		Type = ObjectType;
		Size = n;
		Values = value;
	}

	void setSmallString(size_type n, const char *value) noexcept {
		Type = SmallStringType;
		Size = n;
		StackSize = *reinterpret_cast<const size_t *>(value);
		SmallString[n] = '\0';
	}

	void setString(size_type n, const char *value) noexcept {
		Type = StringType;
		Size = n;
		String = value;
	}

	void setBool(bool value) noexcept {
		Type = BoolType;
		Size = 0;
#if JSOP_WORD_SIZE == 64
		Int64 = static_cast<decltype(Int64)>(value);
#else
		Int32 = static_cast<decltype(Int32)>(value);
#endif
	}

#if JSOP_WORD_SIZE == 64
	void setDouble(double value) noexcept {
		Type = DoubleType;
		Size = 0;
		Double = value;
	}
#else
	void setDouble(double *value) noexcept {
		Type = DoubleType;
		Size = 0;
		Double = value;
	}
#endif

#if JSOP_WORD_SIZE == 64
	void setInt64(int64_t value) noexcept {
		Type = Int64Type;
		Size = 0;
		Int64 = value;
	}

	void setUint64(uint64_t value) noexcept {
		Type = Uint64Type;
		Size = 0;
		Int64 = static_cast<int64_t>(value);
	}
#else
	void setInt32(int32_t value) noexcept {
		Type = Int32Type;
		Size = 0;
		Int32 = value;
	}

	void setUint32(uint32_t value) noexcept {
		Type = Uint32Type;
		Size = 0;
		Int32 = static_cast<int32_t>(value);
	}

	void setInt64(int64_t *value) noexcept {
		Type = Int64Type;
		Size = 0;
		Int64 = value;
	}

	void setUint64(uint64_t *value) noexcept {
		Type = Uint64Type;
		Size = 0;
		Uint64 = value;
	}
#endif

	void setPartialObject(size_t value) noexcept {
		Type = ObjectType;
		Size = 0;
		StackSize = value;
	}

	void setPartialArray(size_t value) noexcept {
		Type = ArrayType;
		Size = 0;
		StackSize = value;
	}

	//! Converts the value into a 64-bit signed integer
	int64_t toInt64() const noexcept {
		switch (Type) {
#if JSOP_WORD_SIZE == 64
		case BoolType:
		case Int64Type:
		case Uint64Type:
			return Int64;

		case DoubleType:
			return static_cast<int64_t>(Double);
#else
		case BoolType:
		case Int32Type:
			return Int32;

		case Uint32Type:
			return static_cast<uint32_t>(Int32);

		case Int64Type:
			return *Int64;

		case Uint64Type:
			return *Uint64;

		case DoubleType:
			return static_cast<int64_t>(*Double);
#endif

		default:
			return 0;
		}
	}

	//! Converts the value into a 64-bit unsigned integer
	uint64_t toUint64() const noexcept {
		switch (Type) {
#if JSOP_WORD_SIZE == 64
		case BoolType:
		case Int64Type:
		case Uint64Type:
			return static_cast<uint64_t>(Int64);

		case DoubleType:
			return static_cast<uint64_t>(Double);
#else
		case BoolType:
		case Int32Type:
		case Uint32Type:
			return Int32;

		case Int64Type:
			return *Int64;

		case Uint64Type:
			return *Uint64;

		case DoubleType:
			return static_cast<int64_t>(*Double);
#endif
		default:
			return 0;
		}
	}

	//! Converts the value into a double precision number
	double toDouble() const noexcept {
		switch (Type) {
#if JSOP_WORD_SIZE == 64
		case BoolType:
		case Int64Type:
			return static_cast<double>(Int64);

		case Uint64Type:
			return static_cast<double>(static_cast<uint64_t>(Int64));

		case DoubleType:
			return Double;
#else
		case BoolType:
		case Int32Type:
			return static_cast<double>(Int32);

		case Uint32Type:
			return static_cast<double>(static_cast<uint32_t>(Int32));

		case Int64Type:
			return static_cast<double>(*Int64);

		case Uint64Type:
			return static_cast<double>(*Uint64);

		case DoubleType:
			return *Double;
#endif

		default:
			return NAN;
		}
	}
};

//! Represents a (key, value) pair
struct JsopKeyValue {
	JsopValue Key;
	JsopValue Value;
};

static_assert(sizeof(JsopKeyValue) == sizeof(JsopValue) * 2, "sizeof(JsopKeyValue) == sizeof(JsopValue) * 2");
static_assert(alignof(JsopKeyValue) == alignof(JsopValue), "alignof(JsopKeyValue) == alignof(JsopValue)");
static_assert(offsetof(JsopKeyValue, Key) == 0, "offsetof(JsopKeyValue, Key) == 0");
static_assert(offsetof(JsopKeyValue, Value) == sizeof(JsopValue), "offsetof(JsopKeyValue, Value) == sizeof(JsopValue)");

JSOP_INLINE JsopArrayView JsopValue::getArrayView() const noexcept {
	assert(Type == ArrayType);
	return JsopArrayView(Values, Values + Size);
}

JSOP_INLINE JsopObjectView JsopValue::getObjectView() const noexcept {
	assert(Type == ObjectType);
	return JsopObjectView(KeyValues, KeyValues + Size);
}

JSOP_INLINE JsopValue::size_type JsopValue::size() const noexcept {
	assert(Type == ArrayType || Type == ObjectType || Type == SmallStringType || Type == StringType);
	return Size;
}

JSOP_INLINE JsopArrayView::size_type JsopArrayView::size() const noexcept {
	return Finish - Start;
}

JSOP_INLINE const JsopValue &JsopArrayView::operator [](size_type i) const noexcept {
	assert(i < size());
	return Start[i];
}

JSOP_INLINE JsopObjectView::size_type JsopObjectView::size() const noexcept {
	return Finish - Start;
}

JSOP_INLINE const JsopKeyValue &JsopObjectView::operator [](size_type i) const noexcept {
	assert(i < size());
	return Start[i];
}

#endif
