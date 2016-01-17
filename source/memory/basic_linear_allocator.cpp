/* The Halfling Project - A Graphics Engine and Projects
 *
 * The Halfling Project is the legal property of Adrian Astley
 * Copyright Adrian Astley 2013 - 2015
 */

/* Modified for use by FiberTaskingLib
 *
 * FiberTaskingLib - A tasking library that uses fibers for efficient task switching
 *
 * This library was created as a proof of concept of the ideas presented by
 * Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
 *
 * http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
 *
 * FiberTaskingLib is the legal property of Adrian Astley
 * Copyright Adrian Astley 2015
 */

#include "fiber_tasking_lib/basic_linear_allocator.h"


namespace FiberTaskingLib {

BasicLinearAllocator::BasicLinearAllocator(std::size_t pageSize)
		: m_pageSize(pageSize),
		  m_numPages(1u) {
	m_currentPage = m_firstPage = new Page(pageSize);
	m_current = reinterpret_cast<byte *>(m_currentPage->Data);
	m_end = m_current + m_pageSize;
}

BasicLinearAllocator::~BasicLinearAllocator() {
	Page *currentPage = m_firstPage;
	Page *pageToDelete;

	do {
		pageToDelete = currentPage;
		currentPage = currentPage->NextPage;

		delete pageToDelete;
	} while (currentPage != nullptr);
}

void *BasicLinearAllocator::Allocate(std::size_t size) {
	if (m_current + size >= m_end) {
		// Check if we already have a new page allocated
		if (m_currentPage->NextPage != nullptr) {
			m_currentPage = m_currentPage->NextPage;
		} else {
			// Allocate a new page;
			Page *newPage = new Page(m_pageSize);

			m_currentPage->NextPage = newPage;
			m_currentPage = newPage;

			++m_numPages;
		}

		m_current = reinterpret_cast<byte *>(m_currentPage->Data);
		m_end = m_current + m_pageSize;
	}

	void *userPtr = m_current;
	m_current += size;

	return userPtr;
}

} // End of namespace Common
