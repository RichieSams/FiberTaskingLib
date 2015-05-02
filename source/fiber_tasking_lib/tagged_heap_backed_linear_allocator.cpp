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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


namespace FiberTaskingLib {

EASTL_ALLOCATOR_EXPLICIT TaggedHeapBackedLinearAllocator::TaggedHeapBackedLinearAllocator(const char *name)
		: m_heap(nullptr), 
		  m_id(0u),
		  m_currentPage(nullptr),
		  m_lock(nullptr),
		  m_end(nullptr),
		  m_current(nullptr) {
	#if EASTL_NAME_ENABLED
		m_name = name;
	#endif
}

TaggedHeapBackedLinearAllocator::TaggedHeapBackedLinearAllocator(const TaggedHeapBackedLinearAllocator &alloc)
		: m_heap(alloc.m_heap),
		  m_id(alloc.m_id),
		  m_currentPage(alloc.m_currentPage),
		  m_lock(alloc.m_lock),
		  m_end(alloc.m_end),
		  m_current(alloc.m_current) {
	#if EASTL_NAME_ENABLED
		m_name = alloc.m_name;
	#endif
}

TaggedHeapBackedLinearAllocator::TaggedHeapBackedLinearAllocator(const TaggedHeapBackedLinearAllocator &alloc, const char *name) 
		: m_heap(alloc.m_heap),
		  m_id(alloc.m_id),
		  m_currentPage(alloc.m_currentPage),
		  m_lock(alloc.m_lock),
		  m_end(alloc.m_end),
		  m_current(alloc.m_current) {
	#if EASTL_NAME_ENABLED
		m_name = name ? name : EASTL_ALLOCATOR_DEFAULT_NAME;
	#endif
}

TaggedHeapBackedLinearAllocator &TaggedHeapBackedLinearAllocator::operator=(const TaggedHeapBackedLinearAllocator &alloc) {
	#if EASTL_NAME_ENABLED
		m_name = alloc.m_name;
	#endif

	m_lock = alloc.m_lock;

	m_heap = alloc.m_heap;
	m_id = alloc.m_id;

	m_currentPage = alloc.m_currentPage;

	m_end = alloc.m_end;
	m_current = alloc.m_current;

	return *this;
}


void TaggedHeapBackedLinearAllocator::init(TaggedHeap *heap, uint64 id) {
	m_lock = new CRITICAL_SECTION();
	InitializeCriticalSection(m_lock);

	m_heap = heap;
	m_id = id;

	m_currentPage = new MemoryPage *();
	m_current = new byte *();
	m_end = new byte *();

	*m_currentPage = heap->GetNextFreePage(id);
	*m_current = reinterpret_cast<byte *>((*m_currentPage)->Data);
	*m_end = *m_current + (*m_currentPage)->PageSize;
}

void TaggedHeapBackedLinearAllocator::reset(uint64 id) {
	m_id = id;

	// Ask for a new page
	*m_currentPage = m_heap->GetNextFreePage(id);
	*m_current = reinterpret_cast<byte *>((*m_currentPage)->Data);
	*m_end = *m_current + (*m_currentPage)->PageSize;
}

void TaggedHeapBackedLinearAllocator::destroy() {
	DeleteCriticalSection(m_lock);

	delete m_currentPage;
	delete m_current;
	delete m_end;
	delete m_lock;
}


void *TaggedHeapBackedLinearAllocator::allocate(size_t n, int flags) {
	EASTL_ASSERT(m_currentPage);

	EnterCriticalSection(m_lock);

	if (*m_current + n >= *m_end) {
		// Ask for a new page
		*m_currentPage = m_heap->GetNextFreePage(m_id);
		*m_current = reinterpret_cast<byte *>((*m_currentPage)->Data);
		*m_end = *m_current + (*m_currentPage)->PageSize;
	}

	void* userPtr = *m_current;
	*m_current += n;

	LeaveCriticalSection(m_lock);

	return userPtr;
}

void *TaggedHeapBackedLinearAllocator::allocate(size_t n, size_t alignment, size_t offset, int flags) {
	EASTL_ASSERT(m_currentPage);

	EnterCriticalSection(m_lock);

	if (*m_current + n >= *m_end) {
		// Ask for a new page
		*m_currentPage = m_heap->GetNextFreePage(m_id);
		*m_current = reinterpret_cast<byte *>((*m_currentPage)->Data);
		*m_end = *m_current + (*m_currentPage)->PageSize;
	}

	size_t bufferSize = *m_end - *m_current;
	void *start = *m_current;
	void *userPtr;
	while ((userPtr = std::align(alignment, n, start, bufferSize)) == nullptr) {
		// Ask for a new page
		*m_currentPage = m_heap->GetNextFreePage(m_id);
		*m_current = reinterpret_cast<byte *>((*m_currentPage)->Data);
		*m_end = *m_current + (*m_currentPage)->PageSize;

		// Recalculate bounds for the align check
		bufferSize = *m_end - *m_current;
		start = *m_current;
	}

	*m_current += ((byte *)userPtr + n) - start;
	LeaveCriticalSection(m_lock);

	return userPtr;
}

// EASTL expects us to define these operators (allocator.h L103)
bool operator==(const TaggedHeapBackedLinearAllocator &a, const TaggedHeapBackedLinearAllocator &b) {
	return a.m_current == b.m_current;
}
bool operator!=(const TaggedHeapBackedLinearAllocator &a, const TaggedHeapBackedLinearAllocator &b) {
	return a.m_current != b.m_current;
}

} // End of namespace FiberTaskingLib
