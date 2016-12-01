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
	ThreadSafeQueue()
		: m_numThreads(0) {
	}
	ThreadSafeQueue(std::size_t numThreads) 
		: m_tls(numThreads),
		  m_numThreads(numThreads) {
	}
	

private:
	template<typename U>
	struct TLSData {
		std::queue<U> Queue;
		std::mutex Lock;

		TLSData<U> &operator=(const TLSData<U> &other) = delete;
	};
	std::vector<TLSData<T> > m_tls;
	std::size_t m_numThreads;

public:
	void Push(std::size_t threadId, T value) {
		std::lock_guard<std::mutex> guard(m_tls[threadId].Lock);
		m_tls[threadId].Queue.push(value);
	}

	bool Pop(std::size_t threadId, T *value) {
		{
			TLSData<T> &tls = m_tls[threadId];
			std::lock_guard<std::mutex> guard(tls.Lock);

			// Try to pop from our own
			if (!tls.Queue.empty()) {
				*value = tls.Queue.front();
				tls.Queue.pop();
				return true;
			}
		}

		// Try to steal from the other threads
		for (uint i = 1; i < m_numThreads; ++i) {
			TLSData<T> &tls = m_tls[(threadId + i) % m_numThreads];
			std::lock_guard<std::mutex> guard(tls.Lock);
			
			if (!tls.Queue.empty()) {
				*value = tls.Queue.front();
				tls.Queue.pop();
				return true;
			}
		}

		return false;
	}
};

} // End of namespace FiberTaskingLib
