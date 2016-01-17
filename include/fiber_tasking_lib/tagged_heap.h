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

#include <unordered_map>
#include <queue>
#include <mutex>


namespace FiberTaskingLib {

struct MemoryPage {
	MemoryPage(size_t pageSize)
		: PageSize(pageSize),
		  Data(::operator new(pageSize)) {}
	~MemoryPage() {
		::operator delete(Data);
	}

	const size_t PageSize;
	void *Data;
};

class TaggedHeap {
public:
	TaggedHeap(size_t pageSize);
	~TaggedHeap();

private:
	const size_t m_pageSize;

	struct MemoryNode {
		MemoryNode(size_t pageSize)
			: Page(pageSize),
			  NextNode(nullptr) {
		}

		MemoryPage Page;
		MemoryNode *NextNode;
	};

	std::unordered_map<uint64, MemoryNode *> m_usedMemory;
	std::queue<MemoryNode *> m_freeMemory;

	std::mutex m_memoryLock;

public:
	MemoryPage *GetNextFreePage(uint64 id);
	void FreeAllPagesWithId(uint64 id);
};

} // End of namespace FiberTaskingLib
