//This Source Code Form is subject to the terms of the Mozilla Public
//License, v. 2.0. If a copy of the MPL was not distributed with this
//file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef JSOP_PACKED_DOCUMENT_H
#define JSOP_PACKED_DOCUMENT_H

#include <stdint.h>
#include <stdlib.h>

#include "JsopPackedValue.h"

template <class ValueType, bool RootFirst = true>
class JsopPackedDocument {
public:
	typedef ValueType value_type;
	typedef typename value_type::size_type size_type;

private:
	value_type *Start;
	const void *End;

public:
	constexpr JsopPackedDocument() noexcept : Start(nullptr), End(nullptr) {
	}
	~JsopPackedDocument() noexcept {
		free(Start);
	}

	JsopPackedDocument(const JsopPackedDocument &) = delete;
	JsopPackedDocument &operator =(const JsopPackedDocument &) = delete;

	const value_type *get() const noexcept {
		if (RootFirst) {
			return Start;
		} else {
			return static_cast<const value_type *>(End) - 1;
		}
	}

	const void *getStart() const noexcept {
		return Start;
	}

	const void *getEnd() const noexcept {
		return End;
	}

	void set(value_type *value, const void *end) noexcept {
		free(Start);
		Start = value;
		End = end;
	}
};

#endif
