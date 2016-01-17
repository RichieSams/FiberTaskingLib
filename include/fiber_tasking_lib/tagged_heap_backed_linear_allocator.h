/* FiberTaskingLib - A tasking library that uses fibers for efficient task switching
 *
 * This library was created as a proof of concept of the ideas presented by
 * Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
 *
 * http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
 *
 * FiberTaskingLib is the legal property of Adrian Astley
 * Copyright Adrian Astley 2015 - 2016
 */


#pragma once

#include "fiber_tasking_lib/typedefs.h"

#include "fiber_tasking_lib/tagged_heap.h"

#include <EASTL/allocator.h>
#include <memory>


namespace FiberTaskingLib {

struct MemoryPage;

#define TAGGED_HEAP_LINEAR_ALLOCATOR_DEFAULT_NAME "TaggedHeapLinearAllocator"

class TaggedHeapBackedLinearAllocator {
public:
	// Constructors
	EASTL_ALLOCATOR_EXPLICIT TaggedHeapBackedLinearAllocator(const char *name = TAGGED_HEAP_LINEAR_ALLOCATOR_DEFAULT_NAME);
	TaggedHeapBackedLinearAllocator(const TaggedHeapBackedLinearAllocator &alloc);
	TaggedHeapBackedLinearAllocator(const TaggedHeapBackedLinearAllocator &alloc, const char *name);

	// Assignment
	TaggedHeapBackedLinearAllocator &operator=(const TaggedHeapBackedLinearAllocator &alloc);


private:
	#if EASTL_NAME_ENABLED
		const char *m_name;
	#endif

	TaggedHeap *m_heap;
	uint64 m_id;

	MemoryPage **m_currentPage;

	byte **m_end;
	byte **m_current;

	std::mutex *m_lock;


public:
	void init(TaggedHeap *heap, uint64 id);
	void reset(uint64 id);
	void destroy();

	// Allocation & Deallocation
	void *allocate(size_t n, int flags = 0);
	void *allocate(size_t n, size_t alignment, size_t offset, int flags = 0);

	inline void deallocate(void *p, size_t n) {
		// NoOp
		// In order to free the memory, the user needs to flush the heap
	}

	// Name info
	inline const char *get_name() const {
		#if EASTL_NAME_ENABLED
			return m_name;
		#else
			return TAGGED_HEAP_LINEAR_ALLOCATOR_DEFAULT_NAME;
		#endif
	}
	inline void set_name(const char *name) {
		#if EASTL_NAME_ENABLED
			m_name = name;
		#endif
	}

	friend bool operator==(const TaggedHeapBackedLinearAllocator &a, const TaggedHeapBackedLinearAllocator &b);
	friend bool operator!=(const TaggedHeapBackedLinearAllocator &a, const TaggedHeapBackedLinearAllocator &b);

private:
	inline void AskForNewPageFromHeap() {
		*m_currentPage = m_heap->GetNextFreePage(m_id);
		*m_current = reinterpret_cast<byte *>((*m_currentPage)->Data);
		*m_end = *m_current + (*m_currentPage)->PageSize;
	}
};

} // End of namespace FiberTaskingLib
