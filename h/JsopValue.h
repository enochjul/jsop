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
		VALUE_TYPE_NUMBER_OF_BITS = 4,
#if JSOP_WORD_SIZE == 64
		MAX_SIZE = (UINT64_C(1) << (64 - VALUE_TYPE_NUMBER_OF_BITS)) - 1
#else
		MAX_SIZE = (1U << (32 - VALUE_TYPE_NUMBER_OF_BITS)) - 1
#endif
	};

	struct SmallString {
		typedef uint8_t size_type;
		typedef char value_type;

		size_type TypeAndSize;
		value_type Data[sizeof(JsopValue::size_type) * 2 - sizeof(size_type)];
	};
	static_assert(sizeof(SmallString) == sizeof(size_type) * 2, "sizeof(SmallString) == sizeof(size_type) * 2");

private:
	union {
		struct {
			size_type TypeAndSize;
			union {
				JsopValue *Values;
				JsopKeyValue *KeyValues;
				const char *String;
#if JSOP_WORD_SIZE == 64
				int64_t Int64;
				double Double;
#else
				int32_t Int32;
				int64_t *Int64;
				uint64_t *Uint64;
				double *Double;
#endif
				size_type StackSize;
			};
		} Value;
		SmallString mySmallString;
	};

public:
	ValueType getType() const noexcept {
		return static_cast<ValueType>(Value.TypeAndSize & ((1 << VALUE_TYPE_NUMBER_OF_BITS) - 1));
	}

	bool isNull() const noexcept {
		return Value.TypeAndSize == NullType;
	}

	bool isBool() const noexcept {
		return getType() == BoolType;
	}

	JSOP_INLINE bool isInteger() const noexcept {
		switch (getType()) {
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
		switch (getType()) {
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
		switch (getType()) {
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
		return getType() == DoubleType;
	}

	JSOP_INLINE bool isString() const noexcept {
		switch (getType()) {
		case SmallStringType:
		case StringType:
			return true;

		default:
			return false;
		}
	}

	bool isArray() const noexcept {
		return getType() == ArrayType;
	}

	bool isObject() const noexcept {
		return getType() == ObjectType;
	}

	const JsopValue *getValues() const noexcept {
		return Value.Values;
	}

	JSOP_INLINE JsopArrayView getArrayView() const noexcept;
	JSOP_INLINE JsopObjectView getObjectView() const noexcept;

	JSOP_INLINE JsopStringView getStringView() const noexcept {
		const char *start;
		const char *finish;

		if (getType() == SmallStringType) {
			start = mySmallString.Data;
			finish = start + ((Value.TypeAndSize >> VALUE_TYPE_NUMBER_OF_BITS) & ((1 << (CHAR_BIT - VALUE_TYPE_NUMBER_OF_BITS)) - 1));
		} else {
			assert(getType() == StringType);
			start = Value.String;
			finish = start + (Value.TypeAndSize >> VALUE_TYPE_NUMBER_OF_BITS);
		}
		return JsopStringView(start, finish);
	}

	//! Gets the null-terminated string stored in the value
	const char *c_str() const noexcept {
		if (getType() == SmallStringType) {
			return mySmallString.Data;
		} else {
			assert(getType() == StringType);
			return Value.String;
		}
	}

	bool getBool() const noexcept {
		assert(getType() == BoolType);

#if JSOP_WORD_SIZE == 64
		return Value.Int64 != 0;
#else
		return Value.Int32 != 0;
#endif
	}

	double getDouble() const noexcept {
		assert(getType() == DoubleType);
#if JSOP_WORD_SIZE == 64
		return Value.Double;
#else
		return *(Value.Double);
#endif
	}

	JSOP_INLINE size_type size() const noexcept;

	size_t getStackSize() const noexcept {
		assert(getType() == ArrayType || getType() == ObjectType);
		return Value.StackSize;
	}

	void setTypeAndSize(ValueType type, size_type n) noexcept {
		Value.TypeAndSize = static_cast<size_type>(type) | (n << VALUE_TYPE_NUMBER_OF_BITS);
	}

	void setNull() noexcept {
		Value.TypeAndSize = NullType;
	}

	void setValues(JsopValue *value) noexcept {
		Value.Values = value;
	}

	void setArray(JsopValue *value, size_type n) noexcept {
		setTypeAndSize(ArrayType, n);
		Value.Values = value;
	}

	void setObject(JsopValue *value, size_type n) noexcept {
		setTypeAndSize(ObjectType, n);
		Value.Values = value;
	}

	void setSmallString(size_type n, const char *value) noexcept {
		assert(n < sizeof(mySmallString.Data));

		size_type v0, v1;
		memcpy(&v0, value, sizeof(size_type));
		memcpy(&v1, value + (sizeof(size_type) - 1), sizeof(size_type));
		Value.TypeAndSize = static_cast<size_type>(SmallStringType) | (n << VALUE_TYPE_NUMBER_OF_BITS) | (v0 << CHAR_BIT);
		Value.StackSize = v1;
		mySmallString.Data[n] = '\0';
	}

	void setString(size_type n, const char *value) noexcept {
		setTypeAndSize(StringType, n);
		Value.String = value;
	}

	void setBool(bool value) noexcept {
		Value.TypeAndSize = BoolType;
#if JSOP_WORD_SIZE == 64
		Value.Int64 = static_cast<decltype(Value.Int64) > (value);
#else
		Value.Int32 = static_cast<decltype(Value.Int32) > (value);
#endif
	}

#if JSOP_WORD_SIZE == 64
	void setDouble(double value) noexcept {
		Value.TypeAndSize = DoubleType;
		Value.Double = value;
	}
#else
	void setDouble(double *value) noexcept {
		Value.TypeAndSize = DoubleType;
		Value.Double = value;
	}
#endif

#if JSOP_WORD_SIZE == 64
	void setInt64(int64_t value) noexcept {
		Value.TypeAndSize = Int64Type;
		Value.Int64 = value;
	}

	void setUint64(uint64_t value) noexcept {
		Value.TypeAndSize = Uint64Type;
		Value.Int64 = static_cast<int64_t>(value);
	}
#else
	void setInt32(int32_t value) noexcept {
		Value.TypeAndSize = Int32Type;
		Value.Int32 = value;
	}

	void setUint32(uint32_t value) noexcept {
		Value.TypeAndSize = Uint32Type;
		Value.Int32 = static_cast<int32_t>(value);
	}

	void setInt64(int64_t *value) noexcept {
		Value.TypeAndSize = Int64Type;
		Value.Int64 = value;
	}

	void setUint64(uint64_t *value) noexcept {
		Value.TypeAndSize = Uint64Type;
		Value.Uint64 = value;
	}
#endif

	void setPartialObject(size_t value) noexcept {
		Value.TypeAndSize = ObjectType;
		Value.StackSize = value;
	}

	void setPartialArray(size_t value) noexcept {
		Value.TypeAndSize = ArrayType;
		Value.StackSize = value;
	}

	//! Converts the value into a 64-bit signed integer
	int64_t toInt64() const noexcept {
		switch (getType()) {
#if JSOP_WORD_SIZE == 64
		case BoolType:
		case Int64Type:
		case Uint64Type:
			return Value.Int64;

		case DoubleType:
			return static_cast<int64_t>(Value.Double);

#else
		case BoolType:
		case Int32Type:
			return Value.Int32;

		case Uint32Type:
			return static_cast<uint32_t>(Value.Int32);

		case Int64Type:
			return *(Value.Int64);

		case Uint64Type:
			return *(Value.Uint64);

		case DoubleType:
			return static_cast<int64_t>(*(Value.Double));
#endif

		default:
			return 0;
		}
	}

	//! Converts the value into a 64-bit unsigned integer
	uint64_t toUint64() const noexcept {
		switch (getType()) {
#if JSOP_WORD_SIZE == 64
		case BoolType:
		case Int64Type:
		case Uint64Type:
			return static_cast<uint64_t>(Value.Int64);

		case DoubleType:
			return static_cast<uint64_t>(Value.Double);

#else
		case BoolType:
		case Int32Type:
		case Uint32Type:
			return Value.Int32;

		case Int64Type:
			return *(Value.Int64);

		case Uint64Type:
			return *(Value.Uint64);

		case DoubleType:
			return static_cast<int64_t>(*(Value.Double));
#endif
		default:
			return 0;
		}
	}

	//! Converts the value into a double precision number
	double toDouble() const noexcept {
		switch (getType()) {
#if JSOP_WORD_SIZE == 64
		case BoolType:
		case Int64Type:
			return static_cast<double>(Value.Int64);

		case Uint64Type:
			return static_cast<double>(static_cast<uint64_t>(Value.Int64));

		case DoubleType:
			return Value.Double;
#else
		case BoolType:
		case Int32Type:
			return static_cast<double>(Value.Int32);

		case Uint32Type:
			return static_cast<double>(static_cast<uint32_t>(Value.Int32));

		case Int64Type:
			return static_cast<double>(*(Value.Int64));

		case Uint64Type:
			return static_cast<double>(*(Value.Uint64));

		case DoubleType:
			return *(Value.Double);
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
	assert(getType() == ArrayType);
	return JsopArrayView(Value.Values, Value.Values + (Value.TypeAndSize >> VALUE_TYPE_NUMBER_OF_BITS));
}

JSOP_INLINE JsopObjectView JsopValue::getObjectView() const noexcept {
	assert(getType() == ObjectType);
	return JsopObjectView(Value.KeyValues, Value.KeyValues + (Value.TypeAndSize >> VALUE_TYPE_NUMBER_OF_BITS));
}

JSOP_INLINE JsopValue::size_type JsopValue::size() const noexcept {
	assert(getType() == ArrayType || getType() == ObjectType || getType() == SmallStringType || getType() == StringType);
	size_type n = Value.TypeAndSize >> VALUE_TYPE_NUMBER_OF_BITS;
	if (JSOP_UNLIKELY(getType() == SmallStringType)) {
		n &= (1 << (CHAR_BIT - VALUE_TYPE_NUMBER_OF_BITS)) - 1;
	}
	return n;
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
