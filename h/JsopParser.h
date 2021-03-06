//This Source Code Form is subject to the terms of the Mozilla Public
//License, v. 2.0. If a copy of the MPL was not distributed with this
//file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef JSOP_PARSER_H
#define JSOP_PARSER_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <utility>

#ifdef __SSE2__
#include <emmintrin.h>
#endif

#include "JsopCodePoint.h"
#include "JsopDecimal.h"
#include "JsopDefines.h"
#include "JsopDocumentHandler.h"
#include "JsopMemoryPools.h"
#include "JsopStringBuffer.h"

#ifdef JSOP_PARSE_STRING_MULTI_BYTE_COPY

#ifdef __SSE2__

template <typename T>
struct JsopCountTrailingZerosReturnType {
		T Count;
		bool ZeroValue;
};

//! x86 bit scan forward instruction
JSOP_INLINE JsopCountTrailingZerosReturnType<uint32_t> jsop_count_trailing_zeros(uint32_t value) noexcept {
		uint32_t count;
		bool zero_value;

		asm ("bsfl %2, %1"
				: "=@ccz" (zero_value), "=r" (count)
				: "rm" (value)
				: "cc");
		return { count, zero_value };
}

#endif

#endif

#ifdef __amd64__

//For some reason gcc does not convert multiply by 10 to use lea instructions in nested if statements that are more than a few levels deep,
//so manually insert the lea instructions with inline assembly
JSOP_INLINE uint64_t jsop_mul10_add_64(uint64_t value, uint64_t a) noexcept {
		uint64_t value_x5, value_x10;
        asm ("lea (%1, %1, 4), %0"
                : "=r" (value_x5)
                : "r" (value)
                : );
        asm ("lea (%2, %1, 2), %0"
                : "=r" (value_x10)
                : "r" (value_x5), "r" (a)
                : );
        return value_x10;
}

//This is for the case where the output value is known to be no more than 32-bits, and constrains the output operand to be 32-bits as the upper
//parts of the register is automatically zeroed
JSOP_INLINE uint64_t jsop_mul10_add_32(uint64_t value, uint64_t a) noexcept {
		uint64_t value_x5, value_x10;
        asm ("lea (%1, %1, 4), %k0"
                : "=r" (value_x5)
                : "r" (value)
                : );
        asm ("lea (%2, %1, 2), %k0"
                : "=r" (value_x10)
                : "r" (value_x5), "r" (a)
                : );
        return value_x10;
}

#else

JSOP_INLINE uint64_t jsop_mul10_add_64(uint64_t value, unsigned a) noexcept {
	return value * 10 + a;
}

JSOP_INLINE uint64_t jsop_mul10_add_32(uint64_t value, unsigned a) noexcept {
	return value * 10 + a;
}

#endif

class JsopDocument;
class JsopValue;

struct JsopMemPool;

//! A JSON compatible parser
template <typename H = JsopDocumentHandler>
class JsopParser final : public H {
	enum State : unsigned {
		Start,
		NegativeValue,
		Zero,
		Number,
		FractionalPartFirstDigit,
		FractionalPart,
		ExponentSignOrFirstDigit,
		ExponentFirstDigit,
		Exponent,
#ifdef JSOP_PARSE_BINARY
		BinaryFirstDigit,
		BinaryNumber,
#endif
#ifdef JSOP_PARSE_HEXADECIMAL
		HexDotOrFirstDigit,
		HexNumber,
		HexFractionalPartFirstDigit,
		HexFractionalPart,
		BinaryExponentSignOrFirstDigit,
		BinaryExponentFirstDigit,
		BinaryExponent,
#endif
		LiteralF,
		LiteralFa,
		LiteralFal,
		LiteralFals,
#ifdef JSOP_PARSE_INFINITY
		LiteralI,
		LiteralIn,
		LiteralInf,
		LiteralInfi,
		LiteralInfin,
		LiteralInfini,
		LiteralInfinit,
#endif
#ifdef JSOP_PARSE_NAN
		LiteralN,
#endif
		Literaln,
#ifdef JSOP_PARSE_NAN
		LiteralNa,
#endif
		LiteralNu,
		LiteralNul,
		LiteralT,
		LiteralTr,
		LiteralTru,
		StringChars,
		StringEscapedChar,
		StringEscapedUtf16Hex1,
		StringEscapedUtf16Hex2,
		StringEscapedUtf16Hex3,
		StringEscapedUtf16Hex4,
		StringEscapedUtf16SurrogateBackslash,
		StringEscapedUtf16SurrogateU,
		StringEscapedUtf16SurrogateHex1,
		StringEscapedUtf16SurrogateHex2,
		StringEscapedUtf16SurrogateHex3,
		StringEscapedUtf16SurrogateHex4,
#ifdef JSOP_PARSE_BRACKET_ESCAPE
		StringEscapedUtf32HexFirst,
		StringEscapedUtf32Hex,
		StringEscapedUtf32SurrogateHex1,
		StringEscapedUtf32SurrogateHex2,
		StringEscapedUtf32SurrogateHex3,
		StringEscapedUtf32SurrogateHex4,
		StringEscapedUtf32SurrogateRightBracket,
#endif
		StringUtf8_0xF0,
		StringUtf8Trail3,
		StringUtf8_0xF4,
		StringUtf8_0xE0,
		StringUtf8Trail2,
		StringUtf8_0xED,
		StringUtf8Trail1,
		Values,
		ValuesSeparatorOrClose,
		KeyValues,
		KeySeparator,
		KeyValuesSeparatorOrClose,
#ifdef JSOP_PARSE_UNQUOTED_KEY
		UnquotedKeyIdContinue,
		UnquotedKeyEscapedChar,
		UnquotedKeyUtf8_0xF0,
		UnquotedKeyUtf8Trail3,
		UnquotedKeyUtf8_0xF4,
		UnquotedKeyUtf8_0xE0,
		UnquotedKeyUtf8Trail2,
		UnquotedKeyUtf8_0xED,
		UnquotedKeyUtf8Trail1,
#endif
#ifdef JSOP_PARSE_COMMENT
		SingleOrMultiLineComment,
		SingleLineComment,
		MultiLineComment,
		MultiLineCommentAsterisk,
#endif
#ifdef JSOP_PARSE_UTF8_BYTE_ORDER_MARK
		Utf8ByteOrderMark2,
		Utf8ByteOrderMark3,
#endif
		EndOfStream,
	};

	JsopStringBuffer Buffer;
	uint64_t CurrentInteger;
	State CurrentState;
	int CurrentExponent;
	int CurrentSpecifiedExponent;
	unsigned CurrentUtf32;
	State LastState;
#ifdef JSOP_PARSER_LOCATION
	unsigned Line, Column;
#endif
	bool Negate;
	bool NegateSpecifiedExponent;
	bool ParsingKey;
#ifndef JSOP_PARSE_TRAILING_COMMA
	bool CommaBeforeBrace;
#endif
#ifdef JSOP_PARSE_UTF8_BYTE_ORDER_MARK
	bool SkippedUtf8ByteOrderMark;
#endif
#ifdef JSOP_PARSE_UNQUOTED_KEY
	bool ParsingIdContinue;
	bool ParsingUnquotedKeyEscape;
#endif

	//! Makes a infinity value
	JSOP_INLINE bool makeInfinity(bool negative) noexcept(H::NoExceptions) {
		return H::makeDouble(!negative ? HUGE_VAL : -HUGE_VAL);
	}

	//! Indicates that the parsing reaches the end of stream and creates the top level value if necessary
	bool parseEndOfStream() noexcept(H::NoExceptions);

public:
	template <typename ... A>
	JSOP_INLINE JsopParser(A && ... args) noexcept(H::NoExceptions) : H(std::forward<A>(args) ...), Buffer(JSOP_STRING_BUFFER_MIN_SIZE / sizeof(char)) {
	}

	JsopParser(const JsopParser &) = delete;
	JsopParser &operator =(const JsopParser &) = delete;

	//! Initializes the parser
	template <typename ... A>
	JSOP_INLINE bool start(A && ... args) noexcept(H::NoExceptions) {
		if (Buffer.initialized() && H::start(std::forward<A>(args) ...)) {
			CurrentState = Start;
#ifdef JSOP_PARSER_LOCATION
			Line = 1;
			Column = 1;
#endif
			ParsingKey = false;
#ifndef JSOP_PARSE_TRAILING_COMMA
			CommaBeforeBrace = false;
#endif
#ifdef JSOP_PARSE_UTF8_BYTE_ORDER_MARK
			SkippedUtf8ByteOrderMark = false;
#endif
			return true;
		}
		return false;
	}

	//! Indicates that there are no more characters left to parse
	template <typename ... A>
	JSOP_INLINE bool finish(A && ... args) noexcept(H::NoExceptions) {
		if (parseEndOfStream()) {
			return H::finish(std::forward<A>(args) ...);
		} else {
			return false;
		}
	}

	//! Parse the given string, and can be called multiple times (as part of reading a file for example)
	bool parse(
		//! Pointer to the start of the string
		const char *start,
		//! Pointer to the end of the string
		const char *end) noexcept(H::NoExceptions);

	//! Parse the given string, and can be called multiple times (as part of reading a file for example)
	bool parse(
		//! Pointer to the start of the string
		const char *start,
		//! Length of the string
		size_t n) noexcept (H::NoExceptions) {
		return parse(start, start + n);
	}

	//! Parse the given string (which is assumed to be NUL-terminated), and can be called multiple times (as part of reading a file for example)
	bool parse(
		//! Pointer to the start of the string
		const char *start) noexcept(H::NoExceptions) {
		return parse(start, strlen(start));
	}
};

#ifndef JSOP_PARSE_TRAILING_COMMA
#define JSOP_PARSER_SET_COMMA_BEFORE_BRACE(value) (CommaBeforeBrace = (value))
#else
#define JSOP_PARSER_SET_COMMA_BEFORE_BRACE(value)
#endif

#define JSOP_PARSER_PUSH_VALUE_EPILOGUE \
	if (!H::inTop()) { \
		if (H::inArray()) { \
			goto state_values_separator_or_close; \
		} else { \
			assert(H::inObject()); \
			goto state_key_values_separator_or_close; \
		} \
	} \
	goto state_end_of_stream

#ifdef JSOP_PARSE_COMMENT
#define JSOP_PARSER_PUSH_VALUE_COMMENT_EPILOGUE \
	if (!H::inTop()) { \
		if (H::inArray()) { \
			LastState = ValuesSeparatorOrClose; \
			goto state_single_or_multi_line_comment; \
		} else { \
			assert(H::inObject()); \
			LastState = KeyValuesSeparatorOrClose; \
			goto state_single_or_multi_line_comment; \
		} \
	} \
	LastState = EndOfStream; \
	goto state_single_or_multi_line_comment
#endif

#define JSOP_PARSER_COMMA_COMMON_ACTION \
	assert(!H::inTop()); \
	JSOP_PARSER_SET_COMMA_BEFORE_BRACE(true); \
	if (H::inArray()) { \
		goto state_values; \
	} else { \
		assert(H::inObject()); \
		goto state_key_values; \
	}

#define JSOP_PARSER_APPEND_CHAR(label) \
	if (Buffer.append(ch)) { \
		goto label; \
	} \
	goto cleanup_on_error

#ifdef JSOP_PARSER_LOCATION
#define JSOP_PARSER_RETURN(state) \
	CurrentState = state; \
	Line = cur_line, Column = start - cur_line_start + 1; \
	return true
#else
#define JSOP_PARSER_RETURN(state) \
	CurrentState = state; \
	return true
#endif

template <typename H>
bool JsopParser<H>::parseEndOfStream() noexcept(H::NoExceptions) {
	State state = CurrentState;
#ifdef JSOP_PARSE_COMMENT
	if (state == SingleLineComment) {
		state = LastState;
	}
#endif

	switch (state) {
	case Zero:
		if (H::makeInteger(0, Negate)) {
			break;
		}
		goto cleanup_on_error;

	case Number:
		if (H::makeInteger(CurrentInteger, Negate)) {
			break;
		}
		goto cleanup_on_error;

	case FractionalPart:
		if (H::makeDouble(jsop_decimal_to_double(CurrentInteger, CurrentExponent, Negate))) {
			break;
		}
		goto cleanup_on_error;

	case Exponent:
		if (H::makeDouble(jsop_decimal_to_double(CurrentInteger, CurrentExponent + (NegateSpecifiedExponent ? -CurrentSpecifiedExponent : CurrentSpecifiedExponent), Negate))) {
			break;
		}
		goto cleanup_on_error;

#ifdef JSOP_PARSE_BINARY
	case BinaryNumber:
		if (H::makeInteger(CurrentInteger, Negate)) {
			break;
		}
		goto cleanup_on_error;
#endif

#ifdef JSOP_PARSE_HEXADECIMAL
	case HexNumber:
		if (H::makeInteger(CurrentInteger, Negate)) {
			break;
		}
		goto cleanup_on_error;

	case BinaryExponent:
		if (H::makeDouble(jsop_hexadecimal_to_double(CurrentInteger, CurrentExponent + (NegateSpecifiedExponent ? -CurrentSpecifiedExponent : CurrentSpecifiedExponent), Negate))) {
			break;
		}
		goto cleanup_on_error;
#endif

#if defined(JSOP_PARSE_INFINITY) && !defined(JSOP_PARSE_STRICT_INFINITY_AND_NAN)
	case LiteralInf:
		if (makeInfinity(Negate)) {
			break;
		}
		goto cleanup_on_error;
#endif

	case EndOfStream:
		if (H::inTop()) {
			break;
		}
		goto cleanup_on_error;

	default:
		goto cleanup_on_error;
	}

	return true;

cleanup_on_error:
	H::cleanup();
	return false;
}

template <typename H>
bool JsopParser<H>::parse(const char *start, const char *end) noexcept(H::NoExceptions) {
#ifdef JSOP_PARSE_STRING_MULTI_BYTE_COPY

#ifdef __SSE2__
	const auto forward_slash_x16 = _mm_set1_epi8('\\'), quote_x16 = _mm_set1_epi8('"'), space_x16 = _mm_set1_epi8(0x20);
#endif

#endif

#ifndef JSOP_IGNORE_OVERFLOW
	uint64_t old_integer;
#endif
#ifdef JSOP_PARSER_LOCATION
	const char *cur_line_start;
	unsigned cur_line;
#endif
	unsigned digit;
	unsigned char ch;

#ifdef JSOP_PARSER_LOCATION
	cur_line = Line;
	cur_line_start = start - Column + 1;
#endif

dispatch_state:
	switch (CurrentState) {
	case Start:
		goto state_start;

	case NegativeValue:
		goto state_negative_value;

	case Zero:
		goto state_zero;

	case Number:
		goto state_number;

	case FractionalPartFirstDigit:
		goto state_fractional_part_first_digit;

	case FractionalPart:
		goto state_fractional_part;

	case ExponentSignOrFirstDigit:
		goto state_exponent_sign_or_first_digit;

	case ExponentFirstDigit:
		goto state_exponent_first_digit;

	case Exponent:
		goto state_exponent;

#ifdef JSOP_PARSE_BINARY
	case BinaryFirstDigit:
		goto state_binary_first_digit;

	case BinaryNumber:
		goto state_binary_number;
#endif

#ifdef JSOP_PARSE_HEXADECIMAL
	case HexDotOrFirstDigit:
		goto state_hex_dot_or_first_digit;

	case HexNumber:
		goto state_hex_number;

	case HexFractionalPartFirstDigit:
		goto state_hex_fractional_part_first_digit;

	case HexFractionalPart:
		goto state_hex_fractional_part;

	case BinaryExponentSignOrFirstDigit:
		goto state_binary_exponent_sign_or_first_digit;

	case BinaryExponentFirstDigit:
		goto state_binary_exponent_first_digit;

	case BinaryExponent:
		goto state_binary_exponent;
#endif

	case LiteralF:
		goto state_literal_f;

	case LiteralFa:
		goto state_literal_fa;

	case LiteralFal:
		goto state_literal_fal;

	case LiteralFals:
		goto state_literal_fals;

#ifdef JSOP_PARSE_INFINITY
	case LiteralI:
		goto state_literal_i;

	case LiteralIn:
		goto state_literal_in;

	case LiteralInf:
		goto state_literal_inf;

	case LiteralInfi:
		goto state_literal_infi;

	case LiteralInfin:
		goto state_literal_infin;

	case LiteralInfini:
		goto state_literal_infini;

	case LiteralInfinit:
		goto state_literal_infinit;
#endif

#ifdef JSOP_PARSE_NAN
	case LiteralN:
		goto state_literal_N;
#endif

	case Literaln:
		goto state_literal_n;

#ifdef JSOP_PARSE_NAN
	case LiteralNa:
		goto state_literal_na;
#endif

	case LiteralNu:
		goto state_literal_nu;

	case LiteralNul:
		goto state_literal_nul;

	case LiteralT:
		goto state_literal_t;

	case LiteralTr:
		goto state_literal_tr;

	case LiteralTru:
		goto state_literal_tru;

	case StringChars:
		goto state_string_chars;

	case StringEscapedChar:
		goto state_string_escaped_char;

	case StringEscapedUtf16Hex1:
		goto state_string_escaped_utf16_hex_1;

	case StringEscapedUtf16Hex2:
		goto state_string_escaped_utf16_hex_2;

	case StringEscapedUtf16Hex3:
		goto state_string_escaped_utf16_hex_3;

	case StringEscapedUtf16Hex4:
		goto state_string_escaped_utf16_hex_4;

	case StringEscapedUtf16SurrogateBackslash:
		goto state_string_escaped_utf16_surrogate_backslash;

	case StringEscapedUtf16SurrogateU:
		goto state_string_escaped_utf16_surrogate_u;

	case StringEscapedUtf16SurrogateHex1:
		goto state_string_escaped_utf16_surrogate_hex_1;

	case StringEscapedUtf16SurrogateHex2:
		goto state_string_escaped_utf16_surrogate_hex_2;

	case StringEscapedUtf16SurrogateHex3:
		goto state_string_escaped_utf16_surrogate_hex_3;

	case StringEscapedUtf16SurrogateHex4:
		goto state_string_escaped_utf16_surrogate_hex_4;

#ifdef JSOP_PARSE_BRACKET_ESCAPE
	case StringEscapedUtf32HexFirst:
		goto state_string_escaped_utf32_hex_first;

	case StringEscapedUtf32Hex:
		goto state_string_escaped_utf32_hex;

	case StringEscapedUtf32SurrogateHex1:
		goto state_string_escaped_utf32_surrogate_hex_1;

	case StringEscapedUtf32SurrogateHex2:
		goto state_string_escaped_utf32_surrogate_hex_2;

	case StringEscapedUtf32SurrogateHex3:
		goto state_string_escaped_utf32_surrogate_hex_3;

	case StringEscapedUtf32SurrogateHex4:
		goto state_string_escaped_utf32_surrogate_hex_4;

	case StringEscapedUtf32SurrogateRightBracket:
		goto state_string_escaped_utf32_surrogate_right_bracket;
#endif

	case StringUtf8_0xF0:
		goto state_string_utf8_0xF0;

	case StringUtf8Trail3:
		goto state_string_utf8_trail_3;

	case StringUtf8_0xF4:
		goto state_string_utf8_0xF4;

	case StringUtf8_0xE0:
		goto state_string_utf8_0xE0;

	case StringUtf8Trail2:
		goto state_string_utf8_trail_2;

	case StringUtf8_0xED:
		goto state_string_utf8_0xED;

	case StringUtf8Trail1:
		goto state_string_utf8_trail_1;

	case Values:
		goto state_values;

	case ValuesSeparatorOrClose:
		goto state_values_separator_or_close;

	case KeyValues:
		goto state_key_values;

	case KeySeparator:
		goto state_key_separator;

	case KeyValuesSeparatorOrClose:
		goto state_key_values_separator_or_close;

#ifdef JSOP_PARSE_UNQUOTED_KEY
	case UnquotedKeyIdContinue:
		goto state_unquoted_key_id_continue;

	case UnquotedKeyEscapedChar:
		goto state_unquoted_key_escaped_char;

	case UnquotedKeyUtf8_0xF0:
		goto state_unquoted_key_utf8_0xF0;

	case UnquotedKeyUtf8Trail3:
		goto state_unquoted_key_utf8_trail_3;

	case UnquotedKeyUtf8_0xF4:
		goto state_unquoted_key_utf8_0xF4;

	case UnquotedKeyUtf8_0xE0:
		goto state_unquoted_key_utf8_0xE0;

	case UnquotedKeyUtf8Trail2:
		goto state_unquoted_key_utf8_trail_2;

	case UnquotedKeyUtf8_0xED:
		goto state_unquoted_key_utf8_0xED;

	case UnquotedKeyUtf8Trail1:
		goto state_unquoted_key_utf8_trail_1;
#endif

#ifdef JSOP_PARSE_COMMENT
	case SingleOrMultiLineComment:
		goto state_single_or_multi_line_comment;

	case SingleLineComment:
		goto state_single_line_comment;

	case MultiLineComment:
		goto state_multi_line_comment;

	case MultiLineCommentAsterisk:
		goto state_multi_line_comment_asterisk;
#endif

#ifdef JSOP_PARSE_UTF8_BYTE_ORDER_MARK
	case Utf8ByteOrderMark2:
		goto state_utf8_byte_order_mark_2;

	case Utf8ByteOrderMark3:
		goto state_utf8_byte_order_mark_3;
#endif

	case EndOfStream:
		goto state_end_of_stream;

	default:
		goto cleanup_on_error;
	}

state_start:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case '0':
			Negate = false;
			goto state_zero;

		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			CurrentInteger = static_cast<size_t>(ch) - '0';
			Negate = false;
			goto action_number_1_non_zero_digit;

		case '.':
			Negate = false;
			goto state_fractional_part_first_digit;

		case '-':
			Negate = true;
			goto state_negative_value;

		case 'f':
			goto state_literal_f;

#ifdef JSOP_PARSE_INFINITY
		case 'I':
#ifndef JSOP_PARSE_STRICT_INFINITY_AND_NAN
		case 'i':
#endif
			Negate = false;
			goto state_literal_i;
#endif

#ifdef JSOP_PARSE_NAN
		case 'N':
			goto state_literal_N;
#endif

		case 'n':
			goto state_literal_n;

		case 't':
			goto state_literal_t;

		case '"':
			Buffer.clear();
			goto state_string_chars;

		case '[':
			if (H::pushArray()) {
				goto state_values;
			}
			goto cleanup_on_error;

		case '{':
			if (H::pushObject()) {
				goto state_key_values;
			}
			goto cleanup_on_error;

		case '\n':
#ifdef JSOP_PARSER_LOCATION
			++cur_line;
			cur_line_start = start;
#endif
		case ' ':
		case '\t':
		case '\r':
#ifdef JSOP_PARSE_UTF8_BYTE_ORDER_MARK
			SkippedUtf8ByteOrderMark = true;
#endif
			goto state_start;

#ifdef JSOP_PARSE_COMMENT
		case '/':
			LastState = Start;
#ifdef JSOP_PARSE_UTF8_BYTE_ORDER_MARK
			SkippedUtf8ByteOrderMark = true;
#endif
			goto state_single_or_multi_line_comment;
#endif

#ifdef JSOP_PARSE_UTF8_BYTE_ORDER_MARK
		case 0xEF:
			if (!SkippedUtf8ByteOrderMark) {
				SkippedUtf8ByteOrderMark = true;
				goto state_utf8_byte_order_mark_2;
			}
			goto cleanup_on_error;
#endif

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(Start);
	}

state_negative_value:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case '0':
			goto state_zero;

		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			CurrentInteger = static_cast<size_t>(ch) - '0';
			goto action_number_1_non_zero_digit;

		case '.':
			goto state_fractional_part_first_digit;

#ifdef JSOP_PARSE_INFINITY
		case 'I':
#ifndef JSOP_PARSE_STRICT_INFINITY_AND_NAN
		case 'i':
#endif
			goto state_literal_i;
#endif

		case '\n':
#ifdef JSOP_PARSER_LOCATION
			++cur_line;
			cur_line_start = start;
#endif
		case ' ':
		case '\t':
		case '\r':
			goto state_negative_value;

#ifdef JSOP_PARSE_COMMENT
		case '/':
			LastState = NegativeValue;
			goto state_single_or_multi_line_comment;
#endif

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(NegativeValue);
	}

state_zero:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case '.':
			CurrentInteger = 0;
			CurrentExponent = 0;
			goto state_fractional_part;

		case 'E':
		case 'e':
			CurrentInteger = 0;
			CurrentExponent = 0;
			goto state_exponent_sign_or_first_digit;

#ifdef JSOP_PARSE_BINARY
		case 'b':
		case 'B':
			goto state_binary_first_digit;
#endif

#ifdef JSOP_PARSE_HEXADECIMAL
		case 'x':
		case 'X':
			goto state_hex_dot_or_first_digit;
#endif

		case ',':
			if (!H::inTop()) {
				if (H::makeInteger(0, Negate)) {
					JSOP_PARSER_COMMA_COMMON_ACTION;
				}
			}
			goto cleanup_on_error;

		case ']':
			//Add the new value and create the array
			if (H::makeInteger(0, Negate)) {
				goto action_array_close_brace;
			}
			goto cleanup_on_error;

		case '}':
			//Add the new value and create the array
			if (H::makeInteger(0, Negate)) {
				goto action_object_close_brace;
			}
			goto cleanup_on_error;

		case '\n':
#ifdef JSOP_PARSER_LOCATION
			++cur_line;
			cur_line_start = start;
#endif
		case ' ':
		case '\t':
		case '\r':
			if (H::makeInteger(0, Negate)) {
				JSOP_PARSER_PUSH_VALUE_EPILOGUE;
			}
			goto cleanup_on_error;

#ifdef JSOP_PARSE_COMMENT
		case '/':
			if (H::makeInteger(0, Negate)) {
				JSOP_PARSER_PUSH_VALUE_COMMENT_EPILOGUE;
			}
			goto cleanup_on_error;
#endif

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(Zero);
	}

action_number_1_non_zero_digit:
	if (reinterpret_cast<uintptr_t>(end) - reinterpret_cast<uintptr_t>(start) >= 19) {
		auto current_integer = CurrentInteger;
		ch = start[0];
		auto old_start = start + 1;
		auto token_end = old_start + 17;
		CurrentExponent = 0;
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
			current_integer = jsop_mul10_add_32(current_integer, digit);
			ch = start[1];
			digit = static_cast<unsigned>(ch) - '0';
			if (JSOP_LIKELY(digit < 10)) {
				current_integer = jsop_mul10_add_32(current_integer, digit);
				ch = start[2];
				digit = static_cast<unsigned>(ch) - '0';
				if (JSOP_LIKELY(digit < 10)) {
					current_integer = jsop_mul10_add_32(current_integer, digit);
					ch = start[3];
					digit = static_cast<unsigned>(ch) - '0';
					if (JSOP_LIKELY(digit < 10)) {
						current_integer = jsop_mul10_add_32(current_integer, digit);
						ch = start[4];
						digit = static_cast<unsigned>(ch) - '0';
						if (JSOP_LIKELY(digit < 10)) {
							current_integer = jsop_mul10_add_32(current_integer, digit);
							ch = start[5];
							digit = static_cast<unsigned>(ch) - '0';
							if (JSOP_LIKELY(digit < 10)) {
								current_integer = jsop_mul10_add_32(current_integer, digit);
								ch = start[6];
								digit = static_cast<unsigned>(ch) - '0';
								if (JSOP_LIKELY(digit < 10)) {
									current_integer = jsop_mul10_add_32(current_integer, digit);
									ch = start[7];
									digit = static_cast<unsigned>(ch) - '0';
									if (JSOP_LIKELY(digit < 10)) {
										current_integer = jsop_mul10_add_32(current_integer, digit);
										ch = start[8];
										digit = static_cast<unsigned>(ch) - '0';
										if (JSOP_LIKELY(digit < 10)) {
											current_integer = jsop_mul10_add_64(current_integer, digit);
											ch = start[9];
											digit = static_cast<unsigned>(ch) - '0';
											if (JSOP_LIKELY(digit < 10)) {
												current_integer = jsop_mul10_add_64(current_integer, digit);
												ch = start[10];
												digit = static_cast<unsigned>(ch) - '0';
												if (JSOP_LIKELY(digit < 10)) {
													current_integer = jsop_mul10_add_64(current_integer, digit);
													ch = start[11];
													digit = static_cast<unsigned>(ch) - '0';
													if (JSOP_LIKELY(digit < 10)) {
														current_integer = jsop_mul10_add_64(current_integer, digit);
														ch = start[12];
														digit = static_cast<unsigned>(ch) - '0';
														if (JSOP_LIKELY(digit < 10)) {
															current_integer = jsop_mul10_add_64(current_integer, digit);
															ch = start[13];
															digit = static_cast<unsigned>(ch) - '0';
															if (JSOP_LIKELY(digit < 10)) {
																current_integer = jsop_mul10_add_64(current_integer, digit);
																ch = start[14];
																digit = static_cast<unsigned>(ch) - '0';
																if (JSOP_LIKELY(digit < 10)) {
																	current_integer = jsop_mul10_add_64(current_integer, digit);
																	ch = start[15];
																	digit = static_cast<unsigned>(ch) - '0';
																	if (JSOP_LIKELY(digit < 10)) {
																		current_integer = jsop_mul10_add_64(current_integer, digit);
																		ch = start[16];
																		digit = static_cast<unsigned>(ch) - '0';
																		if (JSOP_LIKELY(digit < 10)) {
																			current_integer = jsop_mul10_add_64(current_integer, digit);
																			ch = start[17];
																			digit = static_cast<unsigned>(ch) - '0';
																			start += 18;
																			if (JSOP_LIKELY(digit < 10)) {
																				current_integer = jsop_mul10_add_64(current_integer, digit);
																				CurrentInteger = current_integer;
#ifdef JSOP_IGNORE_OVERFLOW
																				goto action_number_1_non_zero_digit;
#else
																				goto state_number;
#endif
																			} else {
																				if (JSOP_LIKELY(ch == '.')) {
																					goto action_fractional_part_18_non_zero_digit;
																				}
																			}
																		} else {
																			start += 17;
																			if (JSOP_LIKELY(ch == '.')) {
																				goto action_fractional_part_17_non_zero_digit;
																			}
																		}
																	} else {
																		start += 16;
																		if (JSOP_LIKELY(ch == '.')) {
																			goto action_fractional_part_16_non_zero_digit;
																		}
																	}
																} else {
																	start += 15;
																	if (JSOP_LIKELY(ch == '.')) {
																		goto action_fractional_part_15_non_zero_digit;
																	}
																}
															} else {
																start += 14;
																if (JSOP_LIKELY(ch == '.')) {
																	goto action_fractional_part_14_non_zero_digit;
																}
															}
														} else {
															start += 13;
															if (JSOP_LIKELY(ch == '.')) {
																goto action_fractional_part_13_non_zero_digit;
															}
														}
													} else {
														start += 12;
														if (JSOP_LIKELY(ch == '.')) {
															goto action_fractional_part_12_non_zero_digit;
														}
													}
												} else {
													start += 11;
													if (JSOP_LIKELY(ch == '.')) {
														goto action_fractional_part_11_non_zero_digit;
													}
												}
											} else {
												start += 10;
												if (JSOP_LIKELY(ch == '.')) {
													goto action_fractional_part_10_non_zero_digit;
												}
											}
										} else {
											start += 9;
											if (JSOP_LIKELY(ch == '.')) {
												goto action_fractional_part_9_non_zero_digit;
											}
										}
									} else {
										start += 8;
										if (JSOP_LIKELY(ch == '.')) {
											goto action_fractional_part_8_non_zero_digit;
										}
									}
								} else {
									start += 7;
									if (JSOP_LIKELY(ch == '.')) {
										goto action_fractional_part_7_non_zero_digit;
									}
								}
							} else {
								start += 6;
								if (JSOP_LIKELY(ch == '.')) {
									goto action_fractional_part_6_non_zero_digit;
								}
							}
						} else {
							start += 5;
							if (JSOP_LIKELY(ch == '.')) {
								goto action_fractional_part_5_non_zero_digit;
							}
						}
					} else {
						start += 4;
						if (JSOP_LIKELY(ch == '.')) {
							goto action_fractional_part_4_non_zero_digit;
						}
					}
				} else {
					start += 3;
					if (JSOP_LIKELY(ch == '.')) {
						goto action_fractional_part_3_non_zero_digit;
					}
				}
			} else {
				start += 2;
				if (JSOP_LIKELY(ch == '.')) {
					goto action_fractional_part_2_non_zero_digit;
				}
			}
		} else {
			start += 1;
			if (JSOP_LIKELY(ch == '.')) {
				goto action_fractional_part_1_non_zero_digit;
			}
		}

		CurrentInteger = current_integer;
		switch (ch) {
		case 'E':
		case 'e':
			CurrentExponent = 0;
			goto state_exponent_sign_or_first_digit;

		case ',':
			if (!H::inTop()) {
				if (H::makeInteger(current_integer, Negate)) {
					JSOP_PARSER_COMMA_COMMON_ACTION;
				}
			}
			goto cleanup_on_error;

		case ']':
			//Add the new value and create the array
			if (H::makeInteger(current_integer, Negate)) {
				goto action_array_close_brace;
			}
			goto cleanup_on_error;

		case '}':
			//Add the new value and create the array
			if (H::makeInteger(current_integer, Negate)) {
				goto action_object_close_brace;
			}
			goto cleanup_on_error;

		case '\n':
#ifdef JSOP_PARSER_LOCATION
			++cur_line;
			cur_line_start = start;
#endif
		case ' ':
		case '\t':
		case '\r':
			if (H::makeInteger(current_integer, Negate)) {
				JSOP_PARSER_PUSH_VALUE_EPILOGUE;
			}
			goto cleanup_on_error;

#ifdef JSOP_PARSE_COMMENT
		case '/':
			if (H::makeInteger(current_integer, Negate)) {
				JSOP_PARSER_PUSH_VALUE_COMMENT_EPILOGUE;
			}
			goto cleanup_on_error;
#endif

		default:
			goto cleanup_on_error;
		}

action_fractional_part_1_non_zero_digit:
		ch = token_end[-17];
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
			current_integer = jsop_mul10_add_32(current_integer, digit);
		} else {
			token_end -= 16;
			goto action_fractional_part_non_zero_digit_final;
		}

action_fractional_part_2_non_zero_digit:
		ch = token_end[-16];
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
			current_integer = jsop_mul10_add_32(current_integer, digit);
		} else {
			token_end -= 15;
			goto action_fractional_part_non_zero_digit_final;
		}

action_fractional_part_3_non_zero_digit:
		ch = token_end[-15];
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
			current_integer = jsop_mul10_add_32(current_integer, digit);
		} else {
			token_end -= 14;
			goto action_fractional_part_non_zero_digit_final;
		}

action_fractional_part_4_non_zero_digit:
		ch = token_end[-14];
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
			current_integer = jsop_mul10_add_32(current_integer, digit);
		} else {
			token_end -= 13;
			goto action_fractional_part_non_zero_digit_final;
		}

action_fractional_part_5_non_zero_digit:
		ch = token_end[-13];
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
			current_integer = jsop_mul10_add_32(current_integer, digit);
		} else {
			token_end -= 12;
			goto action_fractional_part_non_zero_digit_final;
		}

action_fractional_part_6_non_zero_digit:
		ch = token_end[-12];
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
			current_integer = jsop_mul10_add_32(current_integer, digit);
		} else {
			token_end -= 11;
			goto action_fractional_part_non_zero_digit_final;
		}

action_fractional_part_7_non_zero_digit:
		ch = token_end[-11];
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
			current_integer = jsop_mul10_add_32(current_integer, digit);
		} else {
			token_end -= 10;
			goto action_fractional_part_non_zero_digit_final;
		}

action_fractional_part_8_non_zero_digit:
		ch = token_end[-10];
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
			current_integer = jsop_mul10_add_32(current_integer, digit);
		} else {
			token_end -= 9;
			goto action_fractional_part_non_zero_digit_final;
		}

action_fractional_part_9_non_zero_digit:
		ch = token_end[-9];
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
			current_integer = jsop_mul10_add_64(current_integer, digit);
		} else {
			token_end -= 8;
			goto action_fractional_part_non_zero_digit_final;
		}

action_fractional_part_10_non_zero_digit:
		ch = token_end[-8];
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
			current_integer = jsop_mul10_add_64(current_integer, digit);
		} else {
			token_end -= 7;
			goto action_fractional_part_non_zero_digit_final;
		}

action_fractional_part_11_non_zero_digit:
		ch = token_end[-7];
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
			current_integer = jsop_mul10_add_64(current_integer, digit);
		} else {
			token_end -= 6;
			goto action_fractional_part_non_zero_digit_final;
		}

action_fractional_part_12_non_zero_digit:
		ch = token_end[-6];
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
			current_integer = jsop_mul10_add_64(current_integer, digit);
		} else {
			token_end -= 5;
			goto action_fractional_part_non_zero_digit_final;
		}

action_fractional_part_13_non_zero_digit:
		ch = token_end[-5];
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
			current_integer = jsop_mul10_add_64(current_integer, digit);
		} else {
			token_end -= 4;
			goto action_fractional_part_non_zero_digit_final;
		}

action_fractional_part_14_non_zero_digit:
		ch = token_end[-4];
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
			current_integer = jsop_mul10_add_64(current_integer, digit);
		} else {
			token_end -= 3;
			goto action_fractional_part_non_zero_digit_final;
		}

action_fractional_part_15_non_zero_digit:
		ch = token_end[-3];
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
			current_integer = jsop_mul10_add_64(current_integer, digit);
		} else {
			token_end -= 2;
			goto action_fractional_part_non_zero_digit_final;
		}

action_fractional_part_16_non_zero_digit:
		ch = token_end[-2];
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
			current_integer = jsop_mul10_add_64(current_integer, digit);
		} else {
			token_end -= 1;
			goto action_fractional_part_non_zero_digit_final;
		}

action_fractional_part_17_non_zero_digit:
		ch = token_end[-1];
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
			current_integer = jsop_mul10_add_64(current_integer, digit);
		} else {
			goto action_fractional_part_non_zero_digit_final;
		}

action_fractional_part_18_non_zero_digit:
		ch = token_end[0];
		token_end += 1;
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
			current_integer = jsop_mul10_add_64(current_integer, digit);
			CurrentInteger = current_integer;
			CurrentExponent -= static_cast<int>(reinterpret_cast<uintptr_t>(token_end) - reinterpret_cast<uintptr_t>(start));
			start = token_end;
			goto state_fractional_part;
		}

action_fractional_part_non_zero_digit_final:
		CurrentInteger = current_integer;
		CurrentExponent -= static_cast<int>(reinterpret_cast<uintptr_t>(token_end) - reinterpret_cast<uintptr_t>(start) - 1);
		start = token_end;
		goto action_fractional_part_not_a_digit;
	}
	//Fallthrough to state_number

state_number:
	if (start != end) {
		ch = *start;
		++start;
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
#ifdef JSOP_IGNORE_OVERFLOW
			CurrentInteger = jsop_mul10_add_64(CurrentInteger, digit);
			goto state_number;
#else
			old_integer = CurrentInteger;
			CurrentInteger = jsop_mul10_add_64(old_integer, digit);
			if (JSOP_LIKELY(old_integer <= UINT64_C(1844674407370955160))) {
				goto state_number;
			} else if (old_integer == UINT64_C(1844674407370955161)) {
				if(CurrentInteger >= old_integer) {
					goto state_number;
				}
			}
			goto cleanup_on_error;
#endif
		} else {
			switch (ch) {
			case '.':
				CurrentExponent = 0;
				goto state_fractional_part;

			case 'E':
			case 'e':
				CurrentExponent = 0;
				goto state_exponent_sign_or_first_digit;

			case ',':
				if (!H::inTop()) {
					if (H::makeInteger(CurrentInteger, Negate)) {
						JSOP_PARSER_COMMA_COMMON_ACTION;
					}
				}
				goto cleanup_on_error;

			case ']':
				//Add the new value and create the array
				if (H::makeInteger(CurrentInteger, Negate)) {
					goto action_array_close_brace;
				}
				goto cleanup_on_error;

			case '}':
				//Add the new value and create the array
				if (H::makeInteger(CurrentInteger, Negate)) {
					goto action_object_close_brace;
				}
				goto cleanup_on_error;

			case '\n':
#ifdef JSOP_PARSER_LOCATION
				++cur_line;
				cur_line_start = start;
#endif
			case ' ':
			case '\t':
			case '\r':
				if (H::makeInteger(CurrentInteger, Negate)) {
					JSOP_PARSER_PUSH_VALUE_EPILOGUE;
				}
				goto cleanup_on_error;

#ifdef JSOP_PARSE_COMMENT
			case '/':
				if (H::makeInteger(CurrentInteger, Negate)) {
					JSOP_PARSER_PUSH_VALUE_COMMENT_EPILOGUE;
				}
				goto cleanup_on_error;
#endif

			default:
				goto cleanup_on_error;
			}
		}
	} else {
		JSOP_PARSER_RETURN(Number);
	}

state_fractional_part_first_digit:
	if (start != end) {
		ch = *start;
		++start;
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
			CurrentExponent = -1;
			CurrentInteger = digit;
			goto state_fractional_part;
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(FractionalPartFirstDigit);
	}

state_fractional_part:
	if (start != end) {
		ch = *start;
		++start;
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
			CurrentExponent -= 1;
#ifdef JSOP_IGNORE_OVERFLOW
			CurrentInteger = jsop_mul10_add_64(CurrentInteger, digit);
			goto state_fractional_part;
#else
			old_integer = CurrentInteger;
			CurrentInteger = jsop_mul10_add_64(old_integer, digit);
			if (JSOP_LIKELY(old_integer <= UINT64_C(1844674407370955160))) {
				goto state_fractional_part;
			} else if (old_integer == UINT64_C(1844674407370955161)) {
				if(CurrentInteger >= old_integer) {
					goto state_fractional_part;
				}
			}
			goto cleanup_on_error;
#endif
		} else {
action_fractional_part_not_a_digit:
			switch (ch) {
			case 'E':
			case 'e':
				goto state_exponent_sign_or_first_digit;

			case ',':
				if (!H::inTop()) {
					if (H::makeDouble(jsop_decimal_to_double(CurrentInteger, CurrentExponent, Negate))) {
						JSOP_PARSER_COMMA_COMMON_ACTION;
					}
				}
				goto cleanup_on_error;

			case ']':
				//Add the new value and create the array
				if (H::makeDouble(jsop_decimal_to_double(CurrentInteger, CurrentExponent, Negate))) {
					goto action_array_close_brace;
				}
				goto cleanup_on_error;

			case '}':
				//Add the new value and create the array
				if (H::makeDouble(jsop_decimal_to_double(CurrentInteger, CurrentExponent, Negate))) {
					goto action_object_close_brace;
				}
				goto cleanup_on_error;

			case '\n':
#ifdef JSOP_PARSER_LOCATION
				++cur_line;
				cur_line_start = start;
#endif
			case ' ':
			case '\t':
			case '\r':
				if (H::makeDouble(jsop_decimal_to_double(CurrentInteger, CurrentExponent, Negate))) {
					JSOP_PARSER_PUSH_VALUE_EPILOGUE;
				}
				goto cleanup_on_error;

#ifdef JSOP_PARSE_COMMENT
			case '/':
				if (H::makeDouble(jsop_decimal_to_double(CurrentInteger, CurrentExponent, Negate))) {
					JSOP_PARSER_PUSH_VALUE_COMMENT_EPILOGUE;
				}
				goto cleanup_on_error;
#endif

			default:
				goto cleanup_on_error;
			}
		}
	} else {
		JSOP_PARSER_RETURN(FractionalPart);
	}

state_exponent_sign_or_first_digit:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			CurrentSpecifiedExponent = ch - '0';
			NegateSpecifiedExponent = false;
			goto state_exponent;

		case '+':
			NegateSpecifiedExponent = false;
			goto state_exponent_first_digit;

		case '-':
			NegateSpecifiedExponent = true;
			goto state_exponent_first_digit;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(ExponentSignOrFirstDigit);
	}

state_exponent_first_digit:
	if (start != end) {
		ch = *start;
		++start;
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
			CurrentSpecifiedExponent = digit;
			goto state_exponent;
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(ExponentFirstDigit);
	}

state_exponent:
	if (start != end) {
		ch = *start;
		++start;
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
#ifdef JSOP_IGNORE_OVERFLOW
			CurrentSpecifiedExponent = CurrentSpecifiedExponent * 10 + digit;
			goto state_exponent;
#else
			if (JSOP_LIKELY(CurrentSpecifiedExponent <= (INT_MAX / 10 - 1))) {
				CurrentSpecifiedExponent = CurrentSpecifiedExponent * 10 + digit;
			}
			goto state_exponent;
#endif
		} else {
			switch (ch) {
			case ',':
				if (!H::inTop()) {
					if (H::makeDouble(jsop_decimal_to_double(CurrentInteger, CurrentExponent + (NegateSpecifiedExponent ? -CurrentSpecifiedExponent : CurrentSpecifiedExponent), Negate))) {
						JSOP_PARSER_COMMA_COMMON_ACTION;
					}
				}
				goto cleanup_on_error;

			case ']':
				//Add the new value and create the array
				if (H::makeDouble(jsop_decimal_to_double(CurrentInteger, CurrentExponent + (NegateSpecifiedExponent ? -CurrentSpecifiedExponent : CurrentSpecifiedExponent), Negate))) {
					goto action_array_close_brace;
				}
				goto cleanup_on_error;

			case '}':
				//Add the new value and create the array
				if (H::makeDouble(jsop_decimal_to_double(CurrentInteger, CurrentExponent + (NegateSpecifiedExponent ? -CurrentSpecifiedExponent : CurrentSpecifiedExponent), Negate))) {
					goto action_object_close_brace;
				}
				goto cleanup_on_error;

			case '\n':
#ifdef JSOP_PARSER_LOCATION
				++cur_line;
				cur_line_start = start;
#endif
			case ' ':
			case '\t':
			case '\r':
				if (H::makeDouble(jsop_decimal_to_double(CurrentInteger, CurrentExponent + (NegateSpecifiedExponent ? -CurrentSpecifiedExponent : CurrentSpecifiedExponent), Negate))) {
					JSOP_PARSER_PUSH_VALUE_EPILOGUE;
				}
				goto cleanup_on_error;

#ifdef JSOP_PARSE_COMMENT
			case '/':
				if (H::makeDouble(jsop_decimal_to_double(CurrentInteger, CurrentExponent + (NegateSpecifiedExponent ? -CurrentSpecifiedExponent : CurrentSpecifiedExponent), Negate))) {
					JSOP_PARSER_PUSH_VALUE_COMMENT_EPILOGUE;
				}
				goto cleanup_on_error;
#endif

			default:
				goto cleanup_on_error;
			}
		}
	} else {
		JSOP_PARSER_RETURN(Exponent);
	}

#ifdef JSOP_PARSE_BINARY
state_binary_first_digit:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case '0':
		case '1':
			CurrentInteger = static_cast<size_t>(ch) - '0';
			goto state_binary_number;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(BinaryFirstDigit);
	}

state_binary_number:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case '0':
		case '1':
#ifdef JSOP_IGNORE_OVERFLOW
			CurrentInteger = CurrentInteger * 2 + (static_cast<size_t>(ch) - '0');
			goto state_binary_number;
#else
			old_integer = CurrentInteger;
			CurrentInteger = old_integer * 2 + (static_cast<size_t>(ch) - '0');
			if (JSOP_LIKELY(old_integer <= UINT64_C(0x7FFFFFFFFFFFFFFF))) {
				goto state_binary_number;
			} else {
				goto cleanup_on_error;
			}
#endif

		case ',':
			if (!H::inTop()) {
				if (H::makeInteger(CurrentInteger, Negate)) {
					JSOP_PARSER_COMMA_COMMON_ACTION;
				}
			}
			goto cleanup_on_error;

		case ']':
			//Add the new value and create the array
			if (H::makeInteger(CurrentInteger, Negate)) {
				goto action_array_close_brace;
			}
			goto cleanup_on_error;

		case '}':
			//Add the new value and create the array
			if (H::makeInteger(CurrentInteger, Negate)) {
				goto action_object_close_brace;
			}
			goto cleanup_on_error;

		case '\n':
#ifdef JSOP_PARSER_LOCATION
			++cur_line;
			cur_line_start = start;
#endif
		case ' ':
		case '\t':
		case '\r':
			if (H::makeInteger(CurrentInteger, Negate)) {
				JSOP_PARSER_PUSH_VALUE_EPILOGUE;
			}
			goto cleanup_on_error;

#ifdef JSOP_PARSE_COMMENT
		case '/':
			if (H::makeInteger(CurrentInteger, Negate)) {
				JSOP_PARSER_PUSH_VALUE_COMMENT_EPILOGUE;
			}
			goto cleanup_on_error;
#endif

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(BinaryNumber);
	}
#endif

#ifdef JSOP_PARSE_HEXADECIMAL
state_hex_dot_or_first_digit:
	if (start != end) {
		ch = *start;
		++start;
		digit = JsopCodePointHexDigitValue[ch];
		if (JSOP_LIKELY(digit < 16)) {
			CurrentInteger = digit;
			goto state_hex_number;
		} else if (JSOP_LIKELY(ch == '.')) {
			goto state_hex_fractional_part_first_digit;
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(HexDotOrFirstDigit);
	}

state_hex_number:
	if (start != end) {
		ch = *start;
		++start;
		digit = JsopCodePointHexDigitValue[ch];
		if (JSOP_LIKELY(digit < 16)) {
#ifdef JSOP_IGNORE_OVERFLOW
			CurrentInteger = CurrentInteger * 16 + digit;
			goto state_hex_number;
#else
			old_integer = CurrentInteger;
			CurrentInteger = old_integer * 16 + digit;
			if (JSOP_LIKELY(old_integer <= UINT64_C(0xFFFFFFFFFFFFFFF))) {
				goto state_hex_number;
			} else {
				goto cleanup_on_error;
			}
#endif
		} else {
			switch (ch) {
			case '.':
				CurrentExponent = 0;
				goto state_hex_fractional_part;

			case 'P':
			case 'p':
				CurrentExponent = 0;
				goto state_binary_exponent_sign_or_first_digit;

			case ',':
				if (!H::inTop()) {
					if (H::makeInteger(CurrentInteger, Negate)) {
						JSOP_PARSER_COMMA_COMMON_ACTION;
					}
				}
				goto cleanup_on_error;

			case ']':
				//Add the new value and create the array
				if (H::makeInteger(CurrentInteger, Negate)) {
					goto action_array_close_brace;
				}
				goto cleanup_on_error;

			case '}':
				//Add the new value and create the object
				if (H::makeInteger(CurrentInteger, Negate)) {
					goto action_object_close_brace;
				}
				goto cleanup_on_error;

			case '\n':
#ifdef JSOP_PARSER_LOCATION
				++cur_line;
				cur_line_start = start;
#endif
			case ' ':
			case '\t':
			case '\r':
				if (H::makeInteger(CurrentInteger, Negate)) {
					JSOP_PARSER_PUSH_VALUE_EPILOGUE;
				}
				goto cleanup_on_error;

#ifdef JSOP_PARSE_COMMENT
			case '/':
				if (H::makeInteger(CurrentInteger, Negate)) {
					JSOP_PARSER_PUSH_VALUE_COMMENT_EPILOGUE;
				}
				goto cleanup_on_error;
#endif

			default:
				goto cleanup_on_error;
			}
		}
	} else {
		JSOP_PARSER_RETURN(HexNumber);
	}

state_hex_fractional_part_first_digit:
	if (start != end) {
		ch = *start;
		++start;
		digit = JsopCodePointHexDigitValue[ch];
		if (JSOP_LIKELY(digit < 16)) {
			CurrentExponent = -4;
			CurrentInteger = digit;
			goto state_hex_fractional_part;
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(HexFractionalPartFirstDigit);
	}

state_hex_fractional_part:
	if (start != end) {
		ch = *start;
		++start;
		digit = JsopCodePointHexDigitValue[ch];
		if (JSOP_LIKELY(digit < 16)) {
			CurrentExponent -= 4;
#ifdef JSOP_IGNORE_OVERFLOW
			CurrentInteger = CurrentInteger * 16 + digit;
			goto state_hex_fractional_part;
#else
			old_integer = CurrentInteger;
			CurrentInteger = old_integer * 16 + digit;
			if (JSOP_LIKELY(old_integer <= UINT64_C(0xFFFFFFFFFFFFFFF))) {
				goto state_hex_fractional_part;
			} else {
				goto cleanup_on_error;
			}
#endif
		} else {
			switch (ch) {
			case 'P':
			case 'p':
				goto state_binary_exponent_sign_or_first_digit;

			default:
				goto cleanup_on_error;
			}
		}
	} else {
		JSOP_PARSER_RETURN(HexFractionalPart);
	}

state_binary_exponent_sign_or_first_digit:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			CurrentSpecifiedExponent = ch - '0';
			NegateSpecifiedExponent = false;
			goto state_binary_exponent;

		case '+':
			NegateSpecifiedExponent = false;
			goto state_binary_exponent_first_digit;

		case '-':
			NegateSpecifiedExponent = true;
			goto state_binary_exponent_first_digit;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(BinaryExponentSignOrFirstDigit);
	}

state_binary_exponent_first_digit:
	if (start != end) {
		ch = *start;
		++start;
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
			CurrentSpecifiedExponent = digit;
			goto state_binary_exponent;
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(BinaryExponentFirstDigit);
	}

state_binary_exponent:
	if (start != end) {
		ch = *start;
		++start;
		digit = static_cast<unsigned>(ch) - '0';
		if (JSOP_LIKELY(digit < 10)) {
#ifdef JSOP_IGNORE_OVERFLOW
			CurrentSpecifiedExponent = CurrentSpecifiedExponent * 10 + digit;
			goto state_binary_exponent;
#else
			if (CurrentSpecifiedExponent <= (INT_MAX / 10 - 1)) {
				CurrentSpecifiedExponent = CurrentSpecifiedExponent * 10 + digit;
			}
			goto state_binary_exponent;
#endif
		} else {
			switch (ch) {
			case ',':
				if (!H::inTop()) {
					if (H::makeDouble(jsop_hexadecimal_to_double(CurrentInteger, CurrentExponent + (NegateSpecifiedExponent ? -CurrentSpecifiedExponent : CurrentSpecifiedExponent), Negate))) {
						JSOP_PARSER_COMMA_COMMON_ACTION;
					}
				}
				goto cleanup_on_error;

			case ']':
				//Add the new value and create the array
				if (H::makeDouble(jsop_hexadecimal_to_double(CurrentInteger, CurrentExponent + (NegateSpecifiedExponent ? -CurrentSpecifiedExponent : CurrentSpecifiedExponent), Negate))) {
					goto action_array_close_brace;
				}
				goto cleanup_on_error;

			case '}':
				//Add the new value and create the array
				if (H::makeDouble(jsop_hexadecimal_to_double(CurrentInteger, CurrentExponent + (NegateSpecifiedExponent ? -CurrentSpecifiedExponent : CurrentSpecifiedExponent), Negate))) {
					goto action_object_close_brace;
				}
				goto cleanup_on_error;

			case '\n':
#ifdef JSOP_PARSER_LOCATION
				++cur_line;
				cur_line_start = start;
#endif
			case ' ':
			case '\t':
			case '\r':
				if (H::makeDouble(jsop_hexadecimal_to_double(CurrentInteger, CurrentExponent + (NegateSpecifiedExponent ? -CurrentSpecifiedExponent : CurrentSpecifiedExponent), Negate))) {
					JSOP_PARSER_PUSH_VALUE_EPILOGUE;
				}
				goto cleanup_on_error;

#ifdef JSOP_PARSE_COMMENT
			case '/':
				if (H::makeDouble(jsop_hexadecimal_to_double(CurrentInteger, CurrentExponent + (NegateSpecifiedExponent ? -CurrentSpecifiedExponent : CurrentSpecifiedExponent), Negate))) {
					JSOP_PARSER_PUSH_VALUE_COMMENT_EPILOGUE;
				}
				goto cleanup_on_error;
#endif

			default:
				goto cleanup_on_error;
			}
		}
	} else {
		JSOP_PARSER_RETURN(BinaryExponent);
	}
#endif

state_literal_f:
	if (reinterpret_cast<uintptr_t>(end) - reinterpret_cast<uintptr_t>(start) >= sizeof(uint32_t)) {
		uint32_t fragment;
		memcpy(&fragment, start, sizeof(fragment));
		if (fragment == 0x65736c61) {
			start += sizeof(fragment);
			if (H::makeBool(false)) {
				JSOP_PARSER_PUSH_VALUE_EPILOGUE;
			}
		}
		goto cleanup_on_error;
	} else if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case 'a':
			goto state_literal_fa;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(LiteralF);
	}

state_literal_fa:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case 'l':
			goto state_literal_fal;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(LiteralFa);
	}

state_literal_fal:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case 's':
			goto state_literal_fals;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(LiteralFal);
	}

state_literal_fals:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case 'e':
			if (H::makeBool(false)) {
				JSOP_PARSER_PUSH_VALUE_EPILOGUE;
			}
			goto cleanup_on_error;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(LiteralFals);
	}

#ifdef JSOP_PARSE_INFINITY
state_literal_i:
#ifdef JSOP_PARSE_STRICT_INFINITY_AND_NAN
	if (reinterpret_cast<uintptr_t>(end) - reinterpret_cast<uintptr_t>(start) >= sizeof(uint64_t)) {
#if JSOP_WORD_SIZE == 64
		uint64_t fragment;
		memcpy(&fragment, start, sizeof(fragment));
		if ((fragment & UINT64_C(0xFFFFFFFFFFFFFF)) == UINT64_C(0x7974696e69666e)) {
			start += sizeof(fragment) - 1;
			if (makeInfinity(Negate)) {
				JSOP_PARSER_PUSH_VALUE_EPILOGUE;
			}
		}
#else
		uint32_t fragment;
		memcpy(&fragment, start, sizeof(fragment));
		if (fragment == 0x6e69666e) {
			memcpy(&fragment, start + sizeof(fragment), sizeof(fragment));
			if ((fragment & 0xFFFFFF) == 0x797469) {
				start += 2 * sizeof(fragment) - 1;
				if (makeInfinity(Negate)) {
					JSOP_PARSER_PUSH_VALUE_EPILOGUE;
				}
			}
		}
#endif
		goto cleanup_on_error;
	} else
#endif
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
#ifndef JSOP_PARSE_STRICT_INFINITY_AND_NAN
		case 'N':
#endif
		case 'n':
			goto state_literal_in;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(LiteralI);
	}

state_literal_in:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
#ifndef JSOP_PARSE_STRICT_INFINITY_AND_NAN
		case 'F':
#endif
		case 'f':
			goto state_literal_inf;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(LiteralIn);
	}

state_literal_inf:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
#ifndef JSOP_PARSE_STRICT_INFINITY_AND_NAN
		case 'I':
#endif
		case 'i':
			goto state_literal_infi;

#ifndef JSOP_PARSE_STRICT_INFINITY_AND_NAN
		case ',':
			if (!H::inTop()) {
				if (makeInfinity(Negate)) {
					JSOP_PARSER_COMMA_COMMON_ACTION;
				}
			}
			goto cleanup_on_error;

		case ']':
			//Add the new value and create the array
			if (makeInfinity(Negate)) {
				goto action_array_close_brace;
			}
			goto cleanup_on_error;

		case '}':
			//Add the new value and create the array
			if (makeInfinity(Negate)) {
				goto action_object_close_brace;
			}
			goto cleanup_on_error;

		case '\n':
			++cur_line;
			cur_line_start = start;
		case ' ':
		case '\t':
		case '\r':
			if (makeInfinity(Negate)) {
				JSOP_PARSER_PUSH_VALUE_EPILOGUE;
			}
			goto cleanup_on_error;

#ifdef JSOP_PARSE_COMMENT
		case '/':
			if (makeInfinity(Negate)) {
				JSOP_PARSER_PUSH_VALUE_COMMENT_EPILOGUE;
			}
			goto cleanup_on_error;
#endif
#endif

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(LiteralInf);
	}

state_literal_infi:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
#ifndef JSOP_PARSE_STRICT_INFINITY_AND_NAN
		case 'N':
#endif
		case 'n':
			goto state_literal_infin;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(LiteralInfi);
	}

state_literal_infin:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
#ifndef JSOP_PARSE_STRICT_INFINITY_AND_NAN
		case 'I':
#endif
		case 'i':
			goto state_literal_infini;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(LiteralInfin);
	}

state_literal_infini:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
#ifndef JSOP_PARSE_STRICT_INFINITY_AND_NAN
		case 'T':
#endif
		case 't':
			goto state_literal_infinit;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(LiteralInfini);
	}

state_literal_infinit:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
#ifndef JSOP_PARSE_STRICT_INFINITY_AND_NAN
		case 'Y':
#endif
		case 'y':
			if (makeInfinity(Negate)) {
				JSOP_PARSER_PUSH_VALUE_EPILOGUE;
			}
			goto cleanup_on_error;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(LiteralInfinit);
	}
#endif

#ifdef JSOP_PARSE_NAN
state_literal_N:
#ifdef JSOP_PARSE_STRICT_INFINITY_AND_NAN
	if (reinterpret_cast<uintptr_t>(end) - reinterpret_cast<uintptr_t>(start) >= sizeof(uint16_t)) {
		uint16_t fragment;
		memcpy(&fragment, start, sizeof(fragment));
		if (fragment == 0x4e61) {
			start += sizeof(fragment);
			if (H::makeDouble(NAN)) {
				JSOP_PARSER_PUSH_VALUE_EPILOGUE;
			}
		}
		goto cleanup_on_error;
	} else
#endif
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
#ifndef JSOP_PARSE_STRICT_INFINITY_AND_NAN
		case 'A':
#endif
		case 'a':
			goto state_literal_na;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(LiteralN);
	}
#endif

state_literal_n:
#ifdef JSOP_PARSE_STRICT_INFINITY_AND_NAN
	if (reinterpret_cast<uintptr_t>(end) - reinterpret_cast<uintptr_t>(start) >= sizeof(uint32_t)) {
		uint32_t fragment;
		memcpy(&fragment, start, sizeof(fragment));
		if ((fragment & 0xFFFFFF) == 0x6c6c75) {
			start += sizeof(fragment) - 1;
			if (H::makeNull()) {
				JSOP_PARSER_PUSH_VALUE_EPILOGUE;
			}
		}
		goto cleanup_on_error;
	} else
#endif
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
#if defined(JSOP_PARSE_NAN) && !defined(JSOP_PARSE_STRICT_INFINITY_AND_NAN)
		case 'A':
		case 'a':
			goto state_literal_na;
#endif

		case 'u':
			goto state_literal_nu;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(Literaln);
	}

#ifdef JSOP_PARSE_NAN
state_literal_na:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case 'N':
#ifndef JSOP_PARSE_STRICT_INFINITY_AND_NAN
		case 'n':
#endif
			if (H::makeDouble(NAN)) {
				JSOP_PARSER_PUSH_VALUE_EPILOGUE;
			}
			goto cleanup_on_error;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(LiteralNa);
	}
#endif

state_literal_nu:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case 'l':
			goto state_literal_nul;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(LiteralNu);
	}

state_literal_nul:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case 'l':
			if (H::makeNull()) {
				JSOP_PARSER_PUSH_VALUE_EPILOGUE;
			}
			goto cleanup_on_error;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(LiteralNul);
	}

state_literal_t:
	if (reinterpret_cast<uintptr_t>(end) - reinterpret_cast<uintptr_t>(start) >= sizeof(uint32_t)) {
		uint32_t fragment;
		memcpy(&fragment, start, sizeof(fragment));
		fragment &= 0xFFFFFF;
		if (fragment == 0x657572) {
			start += sizeof(fragment) - 1;
			if (H::makeBool(true)) {
				JSOP_PARSER_PUSH_VALUE_EPILOGUE;
			}
		}
		goto cleanup_on_error;
	} else if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case 'r':
			goto state_literal_tr;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(LiteralT);
	}

state_literal_tr:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case 'u':
			goto state_literal_tru;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(LiteralTr);
	}

state_literal_tru:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case 'e':
			if (H::makeBool(true)) {
				JSOP_PARSER_PUSH_VALUE_EPILOGUE;
			}
			goto cleanup_on_error;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(LiteralTru);
	}

state_end_of_stream:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case '\n':
#ifdef JSOP_PARSER_LOCATION
			++cur_line;
			cur_line_start = start;
#endif
		case ' ':
		case '\t':
		case '\r':
			goto state_end_of_stream;

#ifdef JSOP_PARSE_COMMENT
		case '/':
			LastState = EndOfStream;
			goto state_single_or_multi_line_comment;
#endif

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(EndOfStream);
	}

state_string_chars:
#ifdef JSOP_PARSE_STRING_MULTI_BYTE_COPY

#ifdef __SSE2__
	static_assert(JSOP_STRING_BUFFER_MIN_SIZE >= 16, "JSOP_STRING_BUFFER_MIN_SIZE >= 16");
	if (reinterpret_cast<uintptr_t>(end) - reinterpret_cast<uintptr_t>(start) >= 16) {
		if (Buffer.resize_if(16)) {
			auto fragment = _mm_loadu_si128(reinterpret_cast<const __m128i *>(start));

			auto special_mask = _mm_movemask_epi8(_mm_cmpeq_epi8(fragment, forward_slash_x16)) |
				_mm_movemask_epi8(_mm_cmpeq_epi8(fragment, quote_x16)) |
				_mm_movemask_epi8(_mm_cmplt_epi8(fragment, space_x16));

			auto new_end = Buffer.getEnd();
			_mm_storeu_si128(reinterpret_cast<__m128i *>(new_end), fragment);

			auto special_mask_trailing_zeros = jsop_count_trailing_zeros(special_mask);
			if (JSOP_LIKELY(special_mask_trailing_zeros.ZeroValue)) {
				Buffer.setEnd(new_end + 16);
				start += 16;
				goto state_string_chars;
			} else {
				auto special_pos = special_mask_trailing_zeros.Count;
				Buffer.setEnd(new_end + special_pos);
				start += special_pos;
				ch = *start;
				start++;
				goto action_string_chars_test_special_chars;
			}
		} else {
			goto cleanup_on_error;
		}
	}
#elif JSOP_WORD_SIZE == 64
	static_assert(JSOP_STRING_BUFFER_MIN_SIZE >= sizeof(uint64_t), "JSOP_STRING_BUFFER_MIN_SIZE >= sizeof(uint64_t)");
	if (reinterpret_cast<uintptr_t>(end) - reinterpret_cast<uintptr_t>(start) >= sizeof(uint64_t)) {
		if (Buffer.resize_if(sizeof(uint64_t))) {
			uint64_t fragment;
			memcpy(&fragment, start, sizeof(fragment));

			auto new_end = Buffer.getEnd();
			memcpy(new_end, &fragment, sizeof(fragment));

			ch = fragment & 0xFF;
			if (jsop_code_point_is_normal_ascii_string_char(ch)) {
				ch = (fragment >> 8) & 0xFF;
				if (jsop_code_point_is_normal_ascii_string_char(ch)) {
					ch = (fragment >> 16) & 0xFF;
					if (jsop_code_point_is_normal_ascii_string_char(ch)) {
						ch = (fragment >> 24) & 0xFF;
						if (jsop_code_point_is_normal_ascii_string_char(ch)) {
							ch = (fragment >> 32) & 0xFF;
							if (jsop_code_point_is_normal_ascii_string_char(ch)) {
								ch = (fragment >> 40) & 0xFF;
								if (jsop_code_point_is_normal_ascii_string_char(ch)) {
									ch = (fragment >> 48) & 0xFF;
									if (jsop_code_point_is_normal_ascii_string_char(ch)) {
										ch = (fragment >> 56) & 0xFF;
										start += sizeof(fragment);
										if (jsop_code_point_is_normal_ascii_string_char(ch)) {
											Buffer.setEnd(new_end + sizeof(fragment));
											goto state_string_chars;
										} else {
											Buffer.setEnd(new_end + 7);
											goto action_string_chars_test_special_chars;
										}
									} else {
										Buffer.setEnd(new_end + 6);
										start += 7;
										goto action_string_chars_test_special_chars;
									}
								} else {
									Buffer.setEnd(new_end + 5);
									start += 6;
									goto action_string_chars_test_special_chars;
								}
							} else {
								Buffer.setEnd(new_end + 4);
								start += 5;
								goto action_string_chars_test_special_chars;
							}
						} else {
							Buffer.setEnd(new_end + 3);
							start += 4;
							goto action_string_chars_test_special_chars;
						}
					} else {
						Buffer.setEnd(new_end + 2);
						start += 3;
						goto action_string_chars_test_special_chars;
					}
				} else {
					Buffer.setEnd(new_end + 1);
					start += 2;
					goto action_string_chars_test_special_chars;
				}
			} else {
				++start;
				goto action_string_chars_test_special_chars;
			}
		} else {
			goto cleanup_on_error;
		}
	}
#else
	static_assert(JSOP_STRING_BUFFER_MIN_SIZE >= sizeof(uint32_t), "JSOP_STRING_BUFFER_MIN_SIZE >= sizeof(uint32_t)");
	if (reinterpret_cast<uintptr_t>(end) - reinterpret_cast<uintptr_t>(start) >= sizeof(uint32_t)) {
		if (Buffer.resize_if(sizeof(uint32_t))) {
			uint32_t fragment;
			memcpy(&fragment, start, sizeof(fragment));

			auto new_end = Buffer.getEnd();
			memcpy(new_end, &fragment, sizeof(fragment));

			ch = fragment & 0xFF;
			if (jsop_code_point_is_normal_ascii_string_char(ch)) {
				ch = (fragment >> 8) & 0xFF;
				if (jsop_code_point_is_normal_ascii_string_char(ch)) {
					ch = (fragment >> 16) & 0xFF;
					if (jsop_code_point_is_normal_ascii_string_char(ch)) {
						ch = (fragment >> 24) & 0xFF;
						start += sizeof(fragment);
						if (jsop_code_point_is_normal_ascii_string_char(ch)) {
							Buffer.setEnd(new_end + sizeof(fragment));
							goto state_string_chars;
						} else {
							Buffer.setEnd(new_end + 3);
							goto action_string_chars_test_special_chars;
						}
					} else {
						Buffer.setEnd(new_end + 2);
						start += 3;
						goto action_string_chars_test_special_chars;
					}
				} else {
					Buffer.setEnd(new_end + 1);
					start += 2;
					goto action_string_chars_test_special_chars;
				}
			} else {
				++start;
				goto action_string_chars_test_special_chars;
			}
		} else {
			goto cleanup_on_error;
		}
	}
#endif

#endif

action_string_chars_x1:
	if (start != end) {
		ch = *start;
		++start;
		if (jsop_code_point_is_normal_ascii_string_char(ch)) {
			JSOP_PARSER_APPEND_CHAR(action_string_chars_x1);
		} else {
action_string_chars_test_special_chars:
			if (ch == '"') {
				if ((!H::requireNullTerminator() || Buffer.append('\0')) && H::makeString(Buffer.getStart(), Buffer.getEnd(), ParsingKey)) {
					if (!H::inTop()) {
						if (!ParsingKey) {
							if (H::inArray()) {
								goto state_values_separator_or_close;
							} else {
								assert(H::inObject());
								goto state_key_values_separator_or_close;
							}
						} else {
							goto state_key_separator;
						}
					}
					goto state_end_of_stream;
				}
				goto cleanup_on_error;
			} else if (ch == '\\') {
				goto state_string_escaped_char;
			} else {
				switch (ch) {
				//2-byte utf-8 sequences
				case 0xC2:
				case 0xC3:
				case 0xC4:
				case 0xC5:
				case 0xC6:
				case 0xC7:
				case 0xC8:
				case 0xC9:
				case 0xCA:
				case 0xCB:
				case 0xCC:
				case 0xCD:
				case 0xCE:
				case 0xCF:
				case 0xD0:
				case 0xD1:
				case 0xD2:
				case 0xD3:
				case 0xD4:
				case 0xD5:
				case 0xD6:
				case 0xD7:
				case 0xD8:
				case 0xD9:
				case 0xDA:
				case 0xDB:
				case 0xDC:
				case 0xDD:
				case 0xDE:
				case 0xDF:
					JSOP_PARSER_APPEND_CHAR(state_string_utf8_trail_1);

				case 0xE0:
					JSOP_PARSER_APPEND_CHAR(state_string_utf8_0xE0);

				case 0xE1:
				case 0xE2:
				case 0xE3:
				case 0xE4:
				case 0xE5:
				case 0xE6:
				case 0xE7:
				case 0xE8:
				case 0xE9:
				case 0xEA:
				case 0xEB:
				case 0xEC:
				case 0xEE:
				case 0xEF:
					JSOP_PARSER_APPEND_CHAR(state_string_utf8_trail_2);

				case 0xED:
					JSOP_PARSER_APPEND_CHAR(state_string_utf8_0xED);

				case 0xF0:
					JSOP_PARSER_APPEND_CHAR(state_string_utf8_0xF0);

				case 0xF1:
				case 0xF2:
				case 0xF3:
					JSOP_PARSER_APPEND_CHAR(state_string_utf8_trail_3);

				case 0xF4:
					JSOP_PARSER_APPEND_CHAR(state_string_utf8_0xF4);

				default:
					goto cleanup_on_error;
				}
			}
		}
	} else {
		JSOP_PARSER_RETURN(StringChars);
	}

state_string_escaped_char:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
#ifdef JSOP_PARSE_SINGLE_QUOTE_ESCAPE
		case '\'':
#endif
		case '"':
		case '\\':
		case '/':
			break;

		case 'b':
			ch = '\b';
			break;

		case 'f':
			ch = '\f';
			break;

		case 'n':
			ch = '\n';
			break;

		case 'r':
			ch = '\r';
			break;

		case 't':
			ch = '\t';
			break;

		case 'u':
#ifdef JSOP_PARSE_UNQUOTED_KEY
			ParsingUnquotedKeyEscape = false;
#endif
			goto state_string_escaped_utf16_hex_1;

#ifdef JSOP_PARSE_VERTICAL_TAB_ESCAPE
		case 'v':
			ch = '\v';
			break;
#endif

		default:
			goto cleanup_on_error;
		}

		JSOP_PARSER_APPEND_CHAR(state_string_chars);
	} else {
		JSOP_PARSER_RETURN(StringEscapedChar);
	}

state_string_escaped_utf16_hex_1:
	if (start != end) {
		ch = *start;
		++start;
		digit = JsopCodePointHexDigitValue[ch];
		if (JSOP_LIKELY(digit < 16)) {
			CurrentUtf32 = digit * 4096;
			goto state_string_escaped_utf16_hex_2;
#ifdef JSOP_PARSE_BRACKET_ESCAPE
		} else if (ch == '{') {
			goto state_string_escaped_utf32_hex_first;
#endif
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(StringEscapedUtf16Hex1);
	}

state_string_escaped_utf16_hex_2:
	if (start != end) {
		ch = *start;
		++start;
		digit = JsopCodePointHexDigitValue[ch];
		if (JSOP_LIKELY(digit < 16)) {
			CurrentUtf32 += digit * 256;
			goto state_string_escaped_utf16_hex_3;
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(StringEscapedUtf16Hex2);
	}

state_string_escaped_utf16_hex_3:
	if (start != end) {
		ch = *start;
		++start;
		digit = JsopCodePointHexDigitValue[ch];
		if (JSOP_LIKELY(digit < 16)) {
			CurrentUtf32 += digit * 16;
			goto state_string_escaped_utf16_hex_4;
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(StringEscapedUtf16Hex3);
	}

state_string_escaped_utf16_hex_4:
	if (start != end) {
		ch = *start;
		++start;
		digit = JsopCodePointHexDigitValue[ch];
		if (JSOP_LIKELY(digit < 16)) {
			CurrentUtf32 += digit;
		} else {
			goto cleanup_on_error;
		}

		if (CurrentUtf32 < 0xD800 || CurrentUtf32 >= 0xE000) {
#ifndef JSOP_PARSE_UNQUOTED_KEY
			if (Buffer.appendUtf32(CurrentUtf32)) {
				goto state_string_chars;
			} else {
				goto cleanup_on_error;
			}
#else
			if (Buffer.appendUtf32(CurrentUtf32)) {
				if (!ParsingUnquotedKeyEscape) {
					goto state_string_chars;
				} else {
					if (ParsingIdContinue) {
						if (jsop_code_point_is_id_continue(CurrentUtf32)) {
							goto state_unquoted_key_id_continue;
						}
					} else {
						if (jsop_code_point_is_id_start(CurrentUtf32)) {
							goto state_unquoted_key_id_continue;
						}
					}
				}
			}
			goto cleanup_on_error;
#endif
		} else if (CurrentUtf32 <= 0xDBFF) {
			//Utf16 high surrogate
			CurrentUtf32 = (CurrentUtf32 - 0xD800) * 1024 + 0x10000;
			goto state_string_escaped_utf16_surrogate_backslash;
		} else {
			//Utf16 low surrogate should not be encountered first
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(StringEscapedUtf16Hex4);
	}

state_string_escaped_utf16_surrogate_backslash:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case '\\':
			goto state_string_escaped_utf16_surrogate_u;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(StringEscapedUtf16SurrogateBackslash);
	}

state_string_escaped_utf16_surrogate_u:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case 'u':
			goto state_string_escaped_utf16_surrogate_hex_1;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(StringEscapedUtf16SurrogateU);
	}

state_string_escaped_utf16_surrogate_hex_1:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case 'D':
		case 'd':
			goto state_string_escaped_utf16_surrogate_hex_2;

#ifdef JSOP_PARSE_BRACKET_ESCAPE
		case '{':
			goto state_string_escaped_utf32_surrogate_hex_1;
#endif

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(StringEscapedUtf16SurrogateHex1);
	}

state_string_escaped_utf16_surrogate_hex_2:
	if (start != end) {
		ch = *start;
		++start;
		digit = JsopCodePointHexDigitValue[ch] - 12;
		if (JSOP_LIKELY(digit < 4)) {
			CurrentUtf32 += digit * 256;
			goto state_string_escaped_utf16_surrogate_hex_3;
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(StringEscapedUtf16SurrogateHex2);
	}

state_string_escaped_utf16_surrogate_hex_3:
	if (start != end) {
		ch = *start;
		++start;
		digit = JsopCodePointHexDigitValue[ch];
		if (JSOP_LIKELY(digit < 16)) {
			CurrentUtf32 += digit * 16;
			goto state_string_escaped_utf16_surrogate_hex_4;
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(StringEscapedUtf16SurrogateHex3);
	}

state_string_escaped_utf16_surrogate_hex_4:
	if (start != end) {
		ch = *start;
		++start;
		digit = JsopCodePointHexDigitValue[ch];
		if (JSOP_LIKELY(digit < 16)) {
			CurrentUtf32 += digit;
		} else {
			goto cleanup_on_error;
		}

#ifndef JSOP_PARSE_UNQUOTED_KEY
		if (Buffer.appendUtf32(CurrentUtf32)) {
			goto state_string_chars;
		} else {
			goto cleanup_on_error;
		}
#else
		if (Buffer.appendUtf32(CurrentUtf32)) {
			if (!ParsingUnquotedKeyEscape) {
				goto state_string_chars;
			} else {
				if (ParsingIdContinue) {
					if (jsop_code_point_is_id_continue(CurrentUtf32)) {
						goto state_unquoted_key_id_continue;
					}
				} else {
					if (jsop_code_point_is_id_start(CurrentUtf32)) {
						goto state_unquoted_key_id_continue;
					}
				}
			}
		}
		goto cleanup_on_error;
#endif
	} else {
		JSOP_PARSER_RETURN(StringEscapedUtf16SurrogateHex4);
	}

#ifdef JSOP_PARSE_BRACKET_ESCAPE
state_string_escaped_utf32_hex_first:
	if (start != end) {
		ch = *start;
		++start;
		digit = JsopCodePointHexDigitValue[ch];
		if (JSOP_LIKELY(digit < 16)) {
			CurrentUtf32 = digit;
			goto state_string_escaped_utf32_hex;
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(StringEscapedUtf32HexFirst);
	}

state_string_escaped_utf32_hex:
	if (start != end) {
		ch = *start;
		++start;
		digit = JsopCodePointHexDigitValue[ch];
		if (JSOP_LIKELY(digit < 16)) {
			CurrentUtf32 = CurrentUtf32 * 16 + digit;
			if (CurrentUtf32 < 0x110000) {
				goto state_string_escaped_utf32_hex;
			} else {
				goto cleanup_on_error;
			}
		} else if (ch == '}') {
			if (CurrentUtf32 < 0xD800 || CurrentUtf32 >= 0xE000) {
#ifndef JSOP_PARSE_UNQUOTED_KEY
				if (Buffer.appendUtf32(CurrentUtf32)) {
					goto state_string_chars;
				} else {
					goto cleanup_on_error;
				}
#else
				if (Buffer.appendUtf32(CurrentUtf32)) {
					if (!ParsingUnquotedKeyEscape) {
						goto state_string_chars;
					} else {
						if (ParsingIdContinue) {
							if (jsop_code_point_is_id_continue(CurrentUtf32)) {
								goto state_unquoted_key_id_continue;
							}
						} else {
							if (jsop_code_point_is_id_start(CurrentUtf32)) {
								goto state_unquoted_key_id_continue;
							}
						}
					}
				}
				goto cleanup_on_error;
#endif
			} else if (CurrentUtf32 <= 0xDBFF) {
				//Utf16 high surrogate
				CurrentUtf32 = (CurrentUtf32 - 0xD800) * 1024 + 0x10000;
				goto state_string_escaped_utf16_surrogate_backslash;
			} else {
				//Utf16 low surrogate should not be encountered first
				goto cleanup_on_error;
			}
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(StringEscapedUtf32Hex);
	}

state_string_escaped_utf32_surrogate_hex_1:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case '0':
			goto state_string_escaped_utf32_surrogate_hex_1;

		case 'D':
		case 'd':
			goto state_string_escaped_utf32_surrogate_hex_2;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(StringEscapedUtf32SurrogateHex1);
	}

state_string_escaped_utf32_surrogate_hex_2:
	if (start != end) {
		ch = *start;
		++start;
		digit = JsopCodePointHexDigitValue[ch] - 12;
		if (JSOP_LIKELY(digit < 4)) {
			CurrentUtf32 += digit * 256;
			goto state_string_escaped_utf32_surrogate_hex_3;
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(StringEscapedUtf32SurrogateHex2);
	}

state_string_escaped_utf32_surrogate_hex_3:
	if (start != end) {
		ch = *start;
		++start;
		digit = JsopCodePointHexDigitValue[ch];
		if (JSOP_LIKELY(digit < 16)) {
			CurrentUtf32 += digit * 16;
			goto state_string_escaped_utf32_surrogate_hex_4;
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(StringEscapedUtf32SurrogateHex3);
	}

state_string_escaped_utf32_surrogate_hex_4:
	if (start != end) {
		ch = *start;
		++start;
		digit = JsopCodePointHexDigitValue[ch];
		if (JSOP_LIKELY(digit < 16)) {
			CurrentUtf32 += digit;
			goto state_string_escaped_utf32_surrogate_right_bracket;
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(StringEscapedUtf32SurrogateHex4);
	}

state_string_escaped_utf32_surrogate_right_bracket:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case '}':
#ifndef JSOP_PARSE_UNQUOTED_KEY
			if (Buffer.appendUtf32(CurrentUtf32)) {
				goto state_string_chars;
			} else {
				goto cleanup_on_error;
			}
#else
			if (Buffer.appendUtf32(CurrentUtf32)) {
				if (!ParsingUnquotedKeyEscape) {
					goto state_string_chars;
				} else {
					if (ParsingIdContinue) {
						if (jsop_code_point_is_id_continue(CurrentUtf32)) {
							goto state_unquoted_key_id_continue;
						}
					} else {
						if (jsop_code_point_is_id_start(CurrentUtf32)) {
							goto state_unquoted_key_id_continue;
						}
					}
				}
			}
			goto cleanup_on_error;
#endif

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(StringEscapedUtf32SurrogateRightBracket);
	}
#endif

state_string_utf8_0xF0:
	if (start != end) {
		ch = *start;
		++start;
		if (ch >= 0x90 && ch <= 0xBF) {
			JSOP_PARSER_APPEND_CHAR(state_string_utf8_trail_2);
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(StringUtf8_0xF0);
	}

state_string_utf8_trail_3:
	if (start != end) {
		ch = *start;
		++start;
		if (ch >= 0x80 && ch <= 0xBF) {
			JSOP_PARSER_APPEND_CHAR(state_string_utf8_trail_2);
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(StringUtf8Trail3);
	}

state_string_utf8_0xF4:
	if (start != end) {
		ch = *start;
		++start;
		if (ch >= 0x80 && ch <= 0x8F) {
			JSOP_PARSER_APPEND_CHAR(state_string_utf8_trail_2);
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(StringUtf8_0xF4);
	}

state_string_utf8_0xE0:
	if (start != end) {
		ch = *start;
		++start;
		if (ch >= 0xA0 && ch <= 0xBF) {
			JSOP_PARSER_APPEND_CHAR(state_string_utf8_trail_1);
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(StringUtf8_0xE0);
	}

state_string_utf8_trail_2:
	if (start != end) {
		ch = *start;
		++start;
		if (ch >= 0x80 && ch <= 0xBF) {
			JSOP_PARSER_APPEND_CHAR(state_string_utf8_trail_1);
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(StringUtf8Trail2);
	}

state_string_utf8_0xED:
	if (start != end) {
		ch = *start;
		++start;
		if (ch >= 0x80 && ch <= 0x9F) {
			JSOP_PARSER_APPEND_CHAR(state_string_utf8_trail_1);
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(StringUtf8_0xED);
	}

state_string_utf8_trail_1:
	if (start != end) {
		ch = *start;
		++start;
		if (ch >= 0x80 && ch <= 0xBF) {
			JSOP_PARSER_APPEND_CHAR(state_string_chars);
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(StringUtf8Trail1);
	}

state_values:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case '0':
			JSOP_PARSER_SET_COMMA_BEFORE_BRACE(false);
			Negate = false;
			goto state_zero;

		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			JSOP_PARSER_SET_COMMA_BEFORE_BRACE(false);
			CurrentInteger = static_cast<size_t>(ch) - '0';
			Negate = false;
			goto action_number_1_non_zero_digit;

		case '.':
			JSOP_PARSER_SET_COMMA_BEFORE_BRACE(false);
			Negate = false;
			goto state_fractional_part_first_digit;

		case '-':
			JSOP_PARSER_SET_COMMA_BEFORE_BRACE(false);
			Negate = true;
			goto state_negative_value;

		case 'f':
			JSOP_PARSER_SET_COMMA_BEFORE_BRACE(false);
			goto state_literal_f;

#ifdef JSOP_PARSE_INFINITY
		case 'I':
#ifndef JSOP_PARSE_STRICT_INFINITY_AND_NAN
		case 'i':
#endif
			JSOP_PARSER_SET_COMMA_BEFORE_BRACE(false);
			Negate = false;
			goto state_literal_i;
#endif

#ifdef JSOP_PARSE_NAN
		case 'N':
			JSOP_PARSER_SET_COMMA_BEFORE_BRACE(false);
			goto state_literal_N;
#endif

		case 'n':
			JSOP_PARSER_SET_COMMA_BEFORE_BRACE(false);
			goto state_literal_n;

		case 't':
			JSOP_PARSER_SET_COMMA_BEFORE_BRACE(false);
			goto state_literal_t;

		case '"':
			JSOP_PARSER_SET_COMMA_BEFORE_BRACE(false);
			Buffer.clear();
			goto state_string_chars;

		case '[':
			JSOP_PARSER_SET_COMMA_BEFORE_BRACE(false);
			if (H::pushArray()) {
				goto state_values;
			}
			goto cleanup_on_error;

		case '{':
			JSOP_PARSER_SET_COMMA_BEFORE_BRACE(false);
			if (H::pushObject()) {
				goto state_key_values;
			}
			goto cleanup_on_error;

		case ']':
#ifndef JSOP_PARSE_TRAILING_COMMA
			if (!CommaBeforeBrace) {
#endif
				goto action_array_close_brace;
#ifndef JSOP_PARSE_TRAILING_COMMA
			}
#endif
			goto cleanup_on_error;

		case '\n':
#ifdef JSOP_PARSER_LOCATION
			++cur_line;
			cur_line_start = start;
#endif
		case ' ':
		case '\t':
		case '\r':
			goto state_values;

#ifdef JSOP_PARSE_COMMENT
		case '/':
			LastState = Values;
			goto state_single_or_multi_line_comment;
#endif

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(Values);
	}

state_values_separator_or_close:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case ',':
			JSOP_PARSER_COMMA_COMMON_ACTION;

		case ']':
			goto action_array_close_brace;

		case '\n':
#ifdef JSOP_PARSER_LOCATION
			++cur_line;
			cur_line_start = start;
#endif
		case ' ':
		case '\t':
		case '\r':
			goto state_values_separator_or_close;

#ifdef JSOP_PARSE_COMMENT
		case '/':
			LastState = ValuesSeparatorOrClose;
			goto state_single_or_multi_line_comment;
#endif

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(ValuesSeparatorOrClose);
	}

state_key_values:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case '"':
			ParsingKey = true;
			JSOP_PARSER_SET_COMMA_BEFORE_BRACE(false);
			Buffer.clear();
			goto state_string_chars;

		case '}':
#ifndef JSOP_PARSE_TRAILING_COMMA
			if (!CommaBeforeBrace) {
#endif
				goto action_object_close_brace;
#ifndef JSOP_PARSE_TRAILING_COMMA
			}
#endif
			goto cleanup_on_error;

		case '\n':
#ifdef JSOP_PARSER_LOCATION
			++cur_line;
			cur_line_start = start;
#endif
		case ' ':
		case '\t':
		case '\r':
			goto state_key_values;

#ifdef JSOP_PARSE_COMMENT
		case '/':
			LastState = KeyValues;
			goto state_single_or_multi_line_comment;
#endif

#ifdef JSOP_PARSE_UNQUOTED_KEY
		case '\\':
			Buffer.clear();
			ParsingIdContinue = false;
			goto state_unquoted_key_escaped_char;
#endif

		default:
#ifndef JSOP_PARSE_UNQUOTED_KEY
			goto cleanup_on_error;
#else
			Buffer.clear();
			if (jsop_code_point_is_ascii_id_start(ch)) {
				JSOP_PARSER_APPEND_CHAR(state_unquoted_key_id_continue);
			} else {
				ParsingIdContinue = false;
				switch (ch) {
				//2-byte utf-8 sequences
				case 0xC2:
				case 0xC3:
				case 0xC4:
				case 0xC5:
				case 0xC6:
				case 0xC7:
				case 0xC8:
				case 0xC9:
				case 0xCA:
				case 0xCB:
				case 0xCC:
				case 0xCD:
				case 0xCE:
				case 0xCF:
				case 0xD0:
				case 0xD1:
				case 0xD2:
				case 0xD3:
				case 0xD4:
				case 0xD5:
				case 0xD6:
				case 0xD7:
				case 0xD8:
				case 0xD9:
				case 0xDA:
				case 0xDB:
				case 0xDC:
				case 0xDD:
				case 0xDE:
				case 0xDF:
					CurrentUtf32 = (ch - 0xC0) * 64;
					JSOP_PARSER_APPEND_CHAR(state_unquoted_key_utf8_trail_1);

				case 0xE0:
					JSOP_PARSER_APPEND_CHAR(state_unquoted_key_utf8_0xE0);

				case 0xE1:
				case 0xE2:
				case 0xE3:
				case 0xE4:
				case 0xE5:
				case 0xE6:
				case 0xE7:
				case 0xE8:
				case 0xE9:
				case 0xEA:
				case 0xEB:
				case 0xEC:
				case 0xEE:
				case 0xEF:
					CurrentUtf32 = (ch - 0xE0) * 4096;
					JSOP_PARSER_APPEND_CHAR(state_unquoted_key_utf8_trail_2);

				case 0xED:
					JSOP_PARSER_APPEND_CHAR(state_unquoted_key_utf8_0xED);

				case 0xF0:
					JSOP_PARSER_APPEND_CHAR(state_unquoted_key_utf8_0xF0);

				case 0xF1:
				case 0xF2:
				case 0xF3:
					CurrentUtf32 = (ch - 0xF0) * 262144;
					JSOP_PARSER_APPEND_CHAR(state_unquoted_key_utf8_trail_3);

				case 0xF4:
					JSOP_PARSER_APPEND_CHAR(state_unquoted_key_utf8_0xF4);

				default:
					goto cleanup_on_error;
				}
			}
#endif
		}
	} else {
		JSOP_PARSER_RETURN(KeyValues);
	}

state_key_separator:
	ParsingKey = false;
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case ':':
			goto state_values;

		case '\n':
#ifdef JSOP_PARSER_LOCATION
			++cur_line;
			cur_line_start = start;
#endif
		case ' ':
		case '\t':
		case '\r':
			goto state_key_separator;

#ifdef JSOP_PARSE_COMMENT
		case '/':
			LastState = KeySeparator;
			goto state_single_or_multi_line_comment;
#endif

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(KeySeparator);
	}

state_key_values_separator_or_close:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case ',':
			JSOP_PARSER_SET_COMMA_BEFORE_BRACE(true);
			goto state_key_values;

		case '}':
			goto action_object_close_brace;

		case '\n':
#ifdef JSOP_PARSER_LOCATION
			++cur_line;
			cur_line_start = start;
#endif
		case ' ':
		case '\t':
		case '\r':
			goto state_key_values_separator_or_close;

#ifdef JSOP_PARSE_COMMENT
		case '/':
			LastState = KeyValuesSeparatorOrClose;
			goto state_single_or_multi_line_comment;
#endif

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(KeyValuesSeparatorOrClose);
	}

#ifdef JSOP_PARSE_UNQUOTED_KEY
state_unquoted_key_id_continue:

#ifdef JSOP_PARSE_UNQUOTED_KEY_MULTI_BYTE_COPY

#if JSOP_WORD_SIZE == 64
	static_assert(JSOP_STRING_BUFFER_MIN_SIZE >= sizeof(uint64_t), "JSOP_STRING_BUFFER_MIN_SIZE >= sizeof(uint64_t)");
	if (reinterpret_cast<uintptr_t>(end) - reinterpret_cast<uintptr_t>(start) >= sizeof(uint64_t)) {
		if (Buffer.resize_if(sizeof(uint64_t))) {
			uint64_t fragment;
			memcpy(&fragment, start, sizeof(fragment));

			auto new_end = Buffer.getEnd();
			memcpy(new_end, &fragment, sizeof(fragment));

			ch = fragment & 0xFF;
			if (jsop_code_point_is_ascii_id_continue(ch)) {
				ch = (fragment >> 8) & 0xFF;
				if (jsop_code_point_is_ascii_id_continue(ch)) {
					ch = (fragment >> 16) & 0xFF;
					if (jsop_code_point_is_ascii_id_continue(ch)) {
						ch = (fragment >> 24) & 0xFF;
						if (jsop_code_point_is_ascii_id_continue(ch)) {
							ch = (fragment >> 32) & 0xFF;
							if (jsop_code_point_is_ascii_id_continue(ch)) {
								ch = (fragment >> 40) & 0xFF;
								if (jsop_code_point_is_ascii_id_continue(ch)) {
									ch = (fragment >> 48) & 0xFF;
									if (jsop_code_point_is_ascii_id_continue(ch)) {
										ch = (fragment >> 56) & 0xFF;
										start += sizeof(fragment);
										if (jsop_code_point_is_ascii_id_continue(ch)) {
											Buffer.setEnd(new_end + sizeof(fragment));
											goto state_unquoted_key_id_continue;
										} else {
											Buffer.setEnd(new_end + 7);
											goto action_unquoted_key_id_continue_test_special_chars;
										}
									} else {
										Buffer.setEnd(new_end + 6);
										start += 7;
										goto action_unquoted_key_id_continue_test_special_chars;
									}
								} else {
									Buffer.setEnd(new_end + 5);
									start += 6;
									goto action_unquoted_key_id_continue_test_special_chars;
								}
							} else {
								Buffer.setEnd(new_end + 4);
								start += 5;
								goto action_unquoted_key_id_continue_test_special_chars;
							}
						} else {
							Buffer.setEnd(new_end + 3);
							start += 4;
							goto action_unquoted_key_id_continue_test_special_chars;
						}
					} else {
						Buffer.setEnd(new_end + 2);
						start += 3;
						goto action_unquoted_key_id_continue_test_special_chars;
					}
				} else {
					Buffer.setEnd(new_end + 1);
					start += 2;
					goto action_unquoted_key_id_continue_test_special_chars;
				}
			} else {
				++start;
				goto action_unquoted_key_id_continue_test_special_chars;
			}
		} else {
			goto cleanup_on_error;
		}
	}
#else
	static_assert(JSOP_STRING_BUFFER_MIN_SIZE >= sizeof(uint32_t), "JSOP_STRING_BUFFER_MIN_SIZE >= sizeof(uint32_t)");
	if (reinterpret_cast<uintptr_t>(end) - reinterpret_cast<uintptr_t>(start) >= sizeof(uint32_t)) {
		if (Buffer.resize_if(sizeof(uint32_t))) {
			uint32_t fragment;
			memcpy(&fragment, start, sizeof(fragment));

			auto new_end = Buffer.getEnd();
			memcpy(new_end, &fragment, sizeof(fragment));

			ch = fragment & 0xFF;
			if (jsop_code_point_is_ascii_id_continue(ch)) {
				ch = (fragment >> 8) & 0xFF;
				if (jsop_code_point_is_ascii_id_continue(ch)) {
					ch = (fragment >> 16) & 0xFF;
					if (jsop_code_point_is_ascii_id_continue(ch)) {
						ch = (fragment >> 24) & 0xFF;
						start += sizeof(fragment);
						if (jsop_code_point_is_ascii_id_continue(ch)) {
							Buffer.setEnd(new_end + sizeof(fragment));
							goto state_unquoted_key_id_continue;
						} else {
							Buffer.setEnd(new_end + 3);
							goto action_unquoted_key_id_continue_test_special_chars;
						}
					} else {
						Buffer.setEnd(new_end + 2);
						start += 3;
						goto action_unquoted_key_id_continue_test_special_chars;
					}
				} else {
					Buffer.setEnd(new_end + 1);
					start += 2;
					goto action_unquoted_key_id_continue_test_special_chars;
				}
			} else {
				++start;
				goto action_unquoted_key_id_continue_test_special_chars;
			}
		} else {
			goto cleanup_on_error;
		}
	}
#endif

#endif

action_unquoted_key_id_continue_x1:
	if (start != end) {
		ch = *start;
		++start;
		if (jsop_code_point_is_ascii_id_continue(ch)) {
			JSOP_PARSER_APPEND_CHAR(action_unquoted_key_id_continue_x1);
		} else {
action_unquoted_key_id_continue_test_special_chars:
			switch (ch) {
			case ':':
				assert(!H::inTop());
				if ((!H::requireNullTerminator() || Buffer.append('\0')) && H::makeString(Buffer.getStart(), Buffer.getEnd(), true)) {
					goto state_values;
				}
				goto cleanup_on_error;

			case '\n':
#ifdef JSOP_PARSER_LOCATION
				++cur_line;
				cur_line_start = start;
#endif
			case ' ':
			case '\t':
			case '\r':
				assert(!H::inTop());
				if ((!H::requireNullTerminator() || Buffer.append('\0')) && H::makeString(Buffer.getStart(), Buffer.getEnd(), true)) {
					goto state_key_separator;
				}
				goto cleanup_on_error;

#ifdef JSOP_PARSE_COMMENT
			case '/':
				assert(!H::inTop());
				if ((!H::requireNullTerminator() || Buffer.append('\0')) && H::makeString(Buffer.getStart(), Buffer.getEnd(), true)) {
					LastState = KeySeparator;
					goto state_single_or_multi_line_comment;
				}
				goto cleanup_on_error;
#endif

			case '\\':
				ParsingIdContinue = true;
				goto state_unquoted_key_escaped_char;

			//2-byte utf-8 sequences
			case 0xC2:
			case 0xC3:
			case 0xC4:
			case 0xC5:
			case 0xC6:
			case 0xC7:
			case 0xC8:
			case 0xC9:
			case 0xCA:
			case 0xCB:
			case 0xCC:
			case 0xCD:
			case 0xCE:
			case 0xCF:
			case 0xD0:
			case 0xD1:
			case 0xD2:
			case 0xD3:
			case 0xD4:
			case 0xD5:
			case 0xD6:
			case 0xD7:
			case 0xD8:
			case 0xD9:
			case 0xDA:
			case 0xDB:
			case 0xDC:
			case 0xDD:
			case 0xDE:
			case 0xDF:
				ParsingIdContinue = true;
				CurrentUtf32 = (ch - 0xC0) * 64;
				JSOP_PARSER_APPEND_CHAR(state_unquoted_key_utf8_trail_1);

			case 0xE0:
				ParsingIdContinue = true;
				JSOP_PARSER_APPEND_CHAR(state_unquoted_key_utf8_0xE0);

			case 0xE1:
			case 0xE2:
			case 0xE3:
			case 0xE4:
			case 0xE5:
			case 0xE6:
			case 0xE7:
			case 0xE8:
			case 0xE9:
			case 0xEA:
			case 0xEB:
			case 0xEC:
			case 0xEE:
			case 0xEF:
				ParsingIdContinue = true;
				CurrentUtf32 = (ch - 0xE0) * 4096;
				JSOP_PARSER_APPEND_CHAR(state_unquoted_key_utf8_trail_2);

			case 0xED:
				ParsingIdContinue = true;
				JSOP_PARSER_APPEND_CHAR(state_unquoted_key_utf8_0xED);

			case 0xF0:
				ParsingIdContinue = true;
				JSOP_PARSER_APPEND_CHAR(state_unquoted_key_utf8_0xF0);

			case 0xF1:
			case 0xF2:
			case 0xF3:
				ParsingIdContinue = true;
				CurrentUtf32 = (ch - 0xF0) * 262144;
				JSOP_PARSER_APPEND_CHAR(state_unquoted_key_utf8_trail_3);

			case 0xF4:
				ParsingIdContinue = true;
				JSOP_PARSER_APPEND_CHAR(state_unquoted_key_utf8_0xF4);

			default:
				goto cleanup_on_error;
			}
		}
	} else {
		JSOP_PARSER_RETURN(UnquotedKeyIdContinue);
	}

state_unquoted_key_escaped_char:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case 'u':
			ParsingUnquotedKeyEscape = true;
			goto state_string_escaped_utf16_hex_1;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(UnquotedKeyEscapedChar);
	}

state_unquoted_key_utf8_0xF0:
	if (start != end) {
		ch = *start;
		++start;
		if (ch >= 0x90 && ch <= 0xBF) {
			CurrentUtf32 = (ch - 0x80) * 4096;
			JSOP_PARSER_APPEND_CHAR(state_unquoted_key_utf8_trail_2);
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(UnquotedKeyUtf8_0xF0);
	}

state_unquoted_key_utf8_trail_3:
	if (start != end) {
		ch = *start;
		++start;
		if (ch >= 0x80 && ch <= 0xBF) {
			CurrentUtf32 += (ch - 0x80) * 4096;
			JSOP_PARSER_APPEND_CHAR(state_unquoted_key_utf8_trail_2);
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(UnquotedKeyUtf8Trail3);
	}

state_unquoted_key_utf8_0xF4:
	if (start != end) {
		ch = *start;
		++start;
		if (ch >= 0x80 && ch <= 0x8F) {
			CurrentUtf32 = 4 * 262144 + (ch - 0x80) * 4096;
			JSOP_PARSER_APPEND_CHAR(state_unquoted_key_utf8_trail_2);
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(UnquotedKeyUtf8_0xF4);
	}

state_unquoted_key_utf8_0xE0:
	if (start != end) {
		ch = *start;
		++start;
		if (ch >= 0xA0 && ch <= 0xBF) {
			CurrentUtf32 = (ch - 0x80) * 64;
			JSOP_PARSER_APPEND_CHAR(state_unquoted_key_utf8_trail_1);
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(UnquotedKeyUtf8_0xE0);
	}

state_unquoted_key_utf8_trail_2:
	if (start != end) {
		ch = *start;
		++start;
		if (ch >= 0x80 && ch <= 0xBF) {
			CurrentUtf32 += (ch - 0x80) * 64;
			JSOP_PARSER_APPEND_CHAR(state_unquoted_key_utf8_trail_1);
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(UnquotedKeyUtf8Trail2);
	}

state_unquoted_key_utf8_0xED:
	if (start != end) {
		ch = *start;
		++start;
		if (ch >= 0x80 && ch <= 0x9F) {
			CurrentUtf32 = 0xD * 4096 + (ch - 0x80) * 64;
			JSOP_PARSER_APPEND_CHAR(state_unquoted_key_utf8_trail_1);
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(UnquotedKeyUtf8_0xED);
	}

state_unquoted_key_utf8_trail_1:
	if (start != end) {
		ch = *start;
		++start;
		if (ch >= 0x80 && ch <= 0xBF) {
			CurrentUtf32 += ch - 0x80;
			if (ParsingIdContinue) {
				if (!jsop_code_point_is_id_continue(CurrentUtf32)) {
					goto cleanup_on_error;
				}
			} else {
				if (!jsop_code_point_is_id_start(CurrentUtf32)) {
					goto cleanup_on_error;
				}
			}
			JSOP_PARSER_APPEND_CHAR(state_unquoted_key_id_continue);
		} else {
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(UnquotedKeyUtf8Trail1);
	}
#endif

#ifdef JSOP_PARSE_COMMENT
state_single_or_multi_line_comment:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case '/':
			goto state_single_line_comment;

		case '*':
			goto state_multi_line_comment;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(SingleOrMultiLineComment);
	}

state_single_line_comment:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case '\n':
#ifdef JSOP_PARSER_LOCATION
			++cur_line;
			cur_line_start = start;
#endif
		case '\r':
			CurrentState = LastState;
			goto dispatch_state;

		default:
			goto state_single_line_comment;
		}
	} else {
		JSOP_PARSER_RETURN(SingleLineComment);
	}

state_multi_line_comment:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case '*':
			goto state_multi_line_comment_asterisk;

		default:
			goto state_multi_line_comment;
		}
	} else {
		JSOP_PARSER_RETURN(MultiLineComment);
	}

state_multi_line_comment_asterisk:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case '*':
			goto state_multi_line_comment_asterisk;

		case '/':
			CurrentState = LastState;
			goto dispatch_state;

		default:
			goto state_multi_line_comment;
		}
	} else {
		JSOP_PARSER_RETURN(MultiLineCommentAsterisk);
	}
#endif

#ifdef JSOP_PARSE_UTF8_BYTE_ORDER_MARK
state_utf8_byte_order_mark_2:
	if (start != end) {
		ch = *start;
		++start;
		switch (ch) {
		case 0xBB:
			goto state_utf8_byte_order_mark_3;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(Utf8ByteOrderMark2);
	}

state_utf8_byte_order_mark_3:
	if (start != end) {
		ch = *start;
		++start;
#ifdef JSOP_PARSER_LOCATION
		//Reset the column
		cur_line_start = start;
#endif
		switch (ch) {
		case 0xBF:
			goto state_start;

		default:
			goto cleanup_on_error;
		}
	} else {
		JSOP_PARSER_RETURN(Utf8ByteOrderMark3);
	}
#endif

action_array_close_brace:
	if (H::popArray()) {
		if (!H::inTop()) {
			if (H::inArray()) {
				goto state_values_separator_or_close;
			} else {
				assert(H::inObject());
				goto state_key_values_separator_or_close;
			}
		} else {
			goto state_end_of_stream;
		}
	}
	goto cleanup_on_error;

action_object_close_brace:
	if (H::popObject()) {
		if (!H::inTop()) {
			if (H::inArray()) {
				goto state_values_separator_or_close;
			} else {
				assert(H::inObject());
				goto state_key_values_separator_or_close;
			}
		} else {
			goto state_end_of_stream;
		}
	}
	goto cleanup_on_error;

cleanup_on_error:
	H::cleanup();
	return false;
}

#endif
