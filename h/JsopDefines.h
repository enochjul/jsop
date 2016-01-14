//This Source Code Form is subject to the terms of the Mozilla Public
//License, v. 2.0. If a copy of the MPL was not distributed with this
//file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef JSOP_DEFINES_H
#define JSOP_DEFINES_H

#ifndef JSOP_WORD_SIZE
	#ifdef __LP64__
		#define JSOP_WORD_SIZE 64
	#else
		#define JSOP_WORD_SIZE 32
	#endif
#endif

#ifndef JSOP_STRING_BUFFER_MIN_SIZE
#define JSOP_STRING_BUFFER_MIN_SIZE 4096
#endif

#ifndef JSOP_VALUE_STACK_MIN_SIZE
#define JSOP_VALUE_STACK_MIN_SIZE 262144
#endif

#ifndef JSOP_MEMORY_POOL_MIN_SIZE
#define JSOP_MEMORY_POOL_MIN_SIZE 65536
#endif

#ifndef JSOP_EVENT_HANDLER_STACK_MIN_SIZE
#define JSOP_EVENT_HANDLER_STACK_MIN_SIZE 256
#endif

#define JSOP_PARSE_COMMENT
#define JSOP_PARSE_UTF8_BYTE_ORDER_MARK
#define JSOP_PARSE_TRAILING_COMMA
#define JSOP_PARSE_BINARY
#define JSOP_PARSE_HEXADECIMAL
#define JSOP_PARSE_INFINITY
#define JSOP_PARSE_NAN
#define JSOP_PARSE_BRACKET_ESCAPE
#define JSOP_PARSE_UNQUOTED_KEY

#define JSOP_USE_FP_MATH

#ifndef JSOP_INLINE
#define JSOP_INLINE __attribute__((always_inline)) inline
#endif

#define JSOP_LIKELY(x) __builtin_expect((x), true)

#endif
