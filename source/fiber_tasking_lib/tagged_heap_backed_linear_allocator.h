/* FiberTaskingLib - A tasking library that uses fibers for efficient task switching
 *
 * This library was created as a proof of concept of the ideas presented by
 * Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
 *
 * http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
 *
 * FiberTaskingLib is the legal property of Adrian Astley
 * Copyright Adrian Astley 2015
 */


#pragma once

#include "fiber_tasking_lib/typedefs.h"


namespace FiberTaskingLib {

class TaggedHeap;
struct MemoryPage;

class TaggedHeapBackedLinearAllocator {
public:
    TaggedHeapBackedLinearAllocator(TaggedHeap *heap, uint64 id);

private:
	TaggedHeap *m_heap;
	const uint64 m_id;

    MemoryPage *m_currentPage;
    
    byte *m_end;
    byte *m_current;
    
public:
    /**
     * Allocates memory of the specified size
     *
     * @param size    The size, in bytes, of the memory to allocate
     * @return        A pointer to the allocated memory
     */
    void *Allocate(size_t size);
	
	template<typename T, typename... Args>
	T* AllocateAndConstruct(Args... args) {
		return new(Allocate(sizeof(T))) T(args...);
	}

    /**
     * Asks the TaggedHeap for a new block of memory to allocate from. 
	 *
	 * WARNING: If the user wishes to use this allocator after calling 
	 * FreeAllPagesWithId() on the heap, they MUST call this function before
	 * Allocate(). Otherwise, the internal memory pointer will be stale.    
     */
    void GetNewMemoryBlockFromHeap();
};

} // End of namespace FiberTaskingLib
