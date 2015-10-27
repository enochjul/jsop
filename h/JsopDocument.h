//This Source Code Form is subject to the terms of the Mozilla Public
//License, v. 2.0. If a copy of the MPL was not distributed with this
//file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef JSOP_DOCUMENT_H
#define JSOP_DOCUMENT_H

#include <stdlib.h>

#include "JsopMemoryPools.h"

class JsopValue;

//! Represents a hierarchy of values with a single top level value
class JsopDocument final {
	friend class JsopDocumentHandler;

	JsopMemoryPools Pools;
	JsopValue *Value = nullptr;

public:
	JsopDocument() = default;
	~JsopDocument() noexcept {
		free(Value);
	}

	JsopDocument(const JsopDocument &) = delete;
	JsopDocument &operator =(const JsopDocument &) = delete;

	//! Gets the top level value
	const JsopValue &get() const noexcept {
		return *Value;
	}
};

#endif
