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

#include "fiber_tasking_lib/tagged_heap_backed_linear_allocator.h"

#include "fiber_tasking_lib/tagged_heap.h"


namespace FiberTaskingLib {

TaggedHeapBackedLinearAllocator::TaggedHeapBackedLinearAllocator(TaggedHeap *heap, uint64 id) 
		: m_heap(heap),
		  m_id(id),
		  m_currentPage(heap->GetNextFreePage(id)) {
	m_current = reinterpret_cast<byte *>(m_currentPage->Data);
	m_end = m_current + m_currentPage->PageSize;
}

void *TaggedHeapBackedLinearAllocator::Allocate(size_t size) {
	if (m_current + size >= m_end) {
		// Ask for a new page
		MemoryPage *page = m_heap->GetNextFreePage(m_id);

		m_current = reinterpret_cast<byte *>(m_currentPage->Data);
		m_end = m_current + m_currentPage->PageSize;
	}

	void* userPtr = m_current;
	m_current += size;

	return userPtr;
}

void TaggedHeapBackedLinearAllocator::GetNewMemoryBlockFromHeap() {
	// Ask for a new page
	MemoryPage *page = m_heap->GetNextFreePage(m_id);

	m_current = reinterpret_cast<byte *>(m_currentPage->Data);
	m_end = m_current + m_currentPage->PageSize;
}

} // End of namespace FiberTaskingLib
