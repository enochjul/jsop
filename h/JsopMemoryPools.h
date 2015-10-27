//This Source Code Form is subject to the terms of the Mozilla Public
//License, v. 2.0. If a copy of the MPL was not distributed with this
//file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef JSOP_MEMORY_POOLS_H
#define JSOP_MEMORY_POOLS_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

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

	//! Allocates memory with the given size
	__attribute__((malloc)) void *allocate(const size_t size) noexcept;

	template <class T>
	T *alloc(size_t n = 1) noexcept {
		return static_cast<T *>(allocate(n * sizeof(T)));
	}
};

#endif
