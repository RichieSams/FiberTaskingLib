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

#include "fiber_tasking_lib/tagged_heap.h"


namespace FiberTaskingLib {

TaggedHeap::TaggedHeap(size_t pageSize)
	: m_pageSize(pageSize) {
}

TaggedHeap::~TaggedHeap() {
	for (auto &kvp : m_usedMemory) {
		MemoryNode *currentNode = kvp.second;
		MemoryNode *nodeToDelete;

		do {
			nodeToDelete = currentNode;
			currentNode = currentNode->NextNode;

			delete nodeToDelete;
		} while (currentNode != nullptr);
	}
	while (!m_freeMemory.empty()) {
		delete m_freeMemory.front();
		m_freeMemory.pop();
	}
}

MemoryPage *TaggedHeap::GetNextFreePage(uint64 id) {
	m_memoryLock.lock();

	MemoryNode *newNode;
	if (m_freeMemory.empty()) {
		newNode = new MemoryNode(m_pageSize);
	} else {
		newNode = m_freeMemory.front();
		m_freeMemory.pop();
	}
	newNode->NextNode = nullptr;

	auto iter = m_usedMemory.find(id);
	if (iter == m_usedMemory.end()) {
		m_usedMemory[id] = newNode;

		m_memoryLock.unlock();
		return &newNode->Page;
	}

	MemoryNode *head = iter->second;
	while (head->NextNode != nullptr) {
		head = head->NextNode;
	}

	head->NextNode = newNode;

	m_memoryLock.unlock();
	return &newNode->Page;
}

void TaggedHeap::FreeAllPagesWithId(uint64 id) {
	m_memoryLock.lock();

	auto iter = m_usedMemory.find(id);
	if (iter == m_usedMemory.end()) {
		m_memoryLock.unlock();
		return;
	}

	MemoryNode *head = iter->second;
	do {
		m_freeMemory.push(head);
		head = head->NextNode;
	} while (head != nullptr);

	m_usedMemory.erase(id);

	m_memoryLock.unlock();
}

} // End of namespace FiberTaskingLib
