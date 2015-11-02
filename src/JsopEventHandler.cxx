//This Source Code Form is subject to the terms of the Mozilla Public
//License, v. 2.0. If a copy of the MPL was not distributed with this
//file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "JsopEventHandler.h"

size_t *JsopEventHandler::resize() noexcept {
	size_t *new_start;
	size_t capacity, n;

	capacity = (AllocEnd - Start) * sizeof(*Start);
	if (capacity <= SIZE_MAX / 2) {
		capacity *= 2;
		new_start = static_cast<size_t *>(realloc(Start, capacity));
		if (new_start != nullptr) {
			n = End - Start;
			Start = new_start;
			End = new_start + n;
			AllocEnd = new_start + capacity / sizeof(*Start);
			return new_start;
		}
	}
	return nullptr;
}
