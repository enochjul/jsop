//This Source Code Form is subject to the terms of the Mozilla Public
//License, v. 2.0. If a copy of the MPL was not distributed with this
//file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>

#include "JsopMemoryPools.h"

void JsopMemoryPools::destroy(Pool *pool) noexcept {
	Pool *next;

	for (; pool != nullptr; pool = next) {
		next = pool->Next;
		free(pool);
	}
}
