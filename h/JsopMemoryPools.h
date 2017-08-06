//This Source Code Form is subject to the terms of the Mozilla Public
//License, v. 2.0. If a copy of the MPL was not distributed with this
//file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef JSOP_MEMORY_POOLS_H
#define JSOP_MEMORY_POOLS_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "JsopDefines.h"

//! A singly linked list of memory pools
class JsopMemoryPools final {
public:
	struct Pool {
		Pool *Next;
		size_t Size;
		alignas(max_align_t) uint8_t Data[];
	};

private:
	Pool *Head = nullptr;

	//Destroys the list of pools specified by the start
	static void destroy(Pool *pool) noexcept;

	//! Allocates memory with the given size
	template <size_t Alignment>
	__attribute__((malloc, assume_aligned(Alignment))) void *allocate(const size_t aligned_size) noexcept;

public:
	JsopMemoryPools() = default;
	~JsopMemoryPools() noexcept {
		destroy(Head);
	}

	JsopMemoryPools(const JsopMemoryPools &) = delete;
	JsopMemoryPools &operator =(const JsopMemoryPools &) = delete;

	void clear() noexcept {
		destroy(Head);
		Head = nullptr;
	}
	//! Moves the pools from the given list to itself and clears the given list
	void move(JsopMemoryPools *pools) noexcept {
		destroy(Head);
		Head = pools->Head;
		pools->Head = nullptr;
	}

	template <class T>
	T *alloc(size_t n = 1) noexcept {
		//Check that the request size is small enough to not overflow
		if (JSOP_LIKELY(n <= (SIZE_MAX - offsetof(Pool, Data)) / sizeof(T))) {
			return static_cast<T *>(allocate<alignof(T)>(n * sizeof(T)));
		} else {
			return nullptr;
		}
	}
};

template <size_t Alignment>
void *JsopMemoryPools::allocate(const size_t aligned_size) noexcept {
	static_assert((JSOP_MEMORY_POOL_MIN_SIZE - offsetof(Pool, Data)) % alignof(max_align_t) == 0, "(JSOP_MEMORY_POOL_MIN_SIZE - offsetof(Pool, Data)) % alignof(max_align_t) == 0");
	static_assert(Alignment <= alignof(max_align_t), "Alignment <= alignof(max_align_t)");

	Pool *pool;
	Pool *new_pool;
	void *new_ptr;
	size_t pool_size, free_size, alloc_size;

	assert(aligned_size > 0 && aligned_size % Alignment == 0);

	//Align the request size and try allocate the space in the current pool
	pool = Head;
	pool_size = SIZE_MAX;
	if (pool != nullptr) {
		pool_size = pool->Size;
		if (JSOP_LIKELY(pool_size < (JSOP_MEMORY_POOL_MIN_SIZE - offsetof(Pool, Data)))) {
			free_size = (JSOP_MEMORY_POOL_MIN_SIZE - offsetof(Pool, Data)) - pool_size;
			if (Alignment > 1) {
				free_size &= ~(Alignment - 1);
			}
			if (JSOP_LIKELY(aligned_size <= free_size)) {
				if (Alignment > 1) {
					pool_size = (JSOP_MEMORY_POOL_MIN_SIZE - offsetof(Pool, Data)) - free_size;
				}
				new_ptr = static_cast<void *>(pool->Data + pool_size);
				pool->Size = pool_size + aligned_size;
				return static_cast<void *>(new_ptr);
			}
		}
	}

	alloc_size = aligned_size + offsetof(Pool, Data);
	if (JSOP_LIKELY(pool_size > aligned_size)) {
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
	return nullptr;
}

#endif
