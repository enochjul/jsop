//This Source Code Form is subject to the terms of the Mozilla Public
//License, v. 2.0. If a copy of the MPL was not distributed with this
//file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>

#include "JsopMemoryPools.h"
#include "JsopDefines.h"

void JsopMemoryPools::destroy(Pool *pool) noexcept {
	Pool *next;

	for (; pool != nullptr; pool = next) {
		next = pool->Next;
		free(pool);
	}
}

void *JsopMemoryPools::allocate(const size_t size) noexcept {
	Pool *pool;
	Pool *new_pool;
	void *new_ptr;
	size_t aligned_size, pool_size, free_size, alloc_size;

	assert(size > 0);

	//Check that the request size is small enough to not overflow
	if (size <= (SIZE_MAX - (offsetof(Pool, Data) + alignof(max_align_t) - 1))) {
		//Align the request size and try allocate the space in the current pool
		aligned_size = (size + (alignof(max_align_t) - 1)) & ~(alignof(max_align_t) - 1);
		pool = Head;
		pool_size = SIZE_MAX;
		if (pool != nullptr) {
			pool_size = pool->Size;
			if (pool_size < (JSOP_MEMORY_POOL_MIN_SIZE - offsetof(Pool, Data))) {
				free_size = (JSOP_MEMORY_POOL_MIN_SIZE - offsetof(Pool, Data)) - pool_size;
				if (aligned_size <= free_size) {
					new_ptr = static_cast<void *>(pool->Data + pool_size);
					pool->Size = pool_size + aligned_size;
					return new_ptr;
				}
			}
		}

		alloc_size = aligned_size + offsetof(Pool, Data);
		if (pool_size > aligned_size) {
			//The new pool has at least as much free space as the old pool if
			//fully allocated, so create and use it for new allocations
			if (alloc_size < JSOP_MEMORY_POOL_MIN_SIZE) {
				alloc_size = JSOP_MEMORY_POOL_MIN_SIZE;
			}
			new_pool = static_cast<Pool *>(malloc(alloc_size));
			if (new_pool != nullptr) {
				new_pool->Next = pool;
				new_pool->Size = aligned_size;
				Head = new_pool;
				return static_cast<void *>(new_pool->Data);
			}
		} else {
			//The current pool has more free space, so allocate the exact size
			//for the new pool and insert it after the current pool
			new_pool = static_cast<Pool *>(malloc(alloc_size));
			if (new_pool != nullptr) {
				new_pool->Next = pool->Next;
				new_pool->Size = aligned_size;
				pool->Next = new_pool;
				return static_cast<void *>(new_pool->Data);
			}
		}
	}
	return nullptr;
}
