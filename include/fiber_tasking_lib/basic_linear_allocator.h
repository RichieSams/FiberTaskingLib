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
 * Copyright Adrian Astley 2015 - 2016
 */


#pragma once

#include "fiber_tasking_lib/typedefs.h"

#include <new>


namespace FiberTaskingLib {

class BasicLinearAllocator {
public:
	BasicLinearAllocator(std::size_t pageSize);
	~BasicLinearAllocator();

private:
	struct Page {
		Page(std::size_t pageSize)
			: NextPage(nullptr),
			  Data(::operator new(pageSize)) {
		}
		~Page() {
			::operator delete(Data);
		}

		Page *NextPage;
		void *Data;
	};

	const std::size_t m_pageSize;

	uint m_numPages;

	Page *m_firstPage;
	Page *m_currentPage;

	byte *m_end;
	byte *m_current;

public:
	void *Allocate(std::size_t size);
	inline void Reset() {
		m_currentPage = m_firstPage;
		m_current = reinterpret_cast<byte *>(m_currentPage->Data);
		m_end = m_current + m_pageSize;
	}
};

} // End of namespace FiberTaskingLib
