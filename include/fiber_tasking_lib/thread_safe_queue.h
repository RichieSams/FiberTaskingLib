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

#include <queue>
#include <mutex>


namespace FiberTaskingLib {

template<typename T>
class ThreadSafeQueue {
public:
	ThreadSafeQueue() { }

private:
	std::queue<T> m_queue;
	std::mutex m_lock;

public:
	void Push(T value) {
		std::lock_guard<std::mutex> guard(m_lock);
		m_queue.push(value);
	}

	bool Pop(T *value) {
		std::lock_guard<std::mutex> guard(m_lock);

		if (m_queue.empty()) {
			return false;
		}

		*value = m_queue.front();
		m_queue.pop();
		return true;
	}
};

} // End of namespace FiberTaskingLib
