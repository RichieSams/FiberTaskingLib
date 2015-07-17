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

#include "fiber_tasking_lib/config.h"
#include "fiber_tasking_lib/thread_abstraction.h"


#if defined(FIBER_IMPL_SUPPORTS_TLS)

namespace FiberTaskingLib {

#define TLS_VARIABLE(type, name) THREAD_LOCAL type name;

template<class T>
inline T GetTLSData(T &tlsVariable) {
	return tlsVariable;
}

template<class T>
inline void SetTLSData(T &tlsVariable, T value) {
	tlsVariable = value;
}

} // End of namespace FiberTaskingLib

#else

#include <unordered_map>
#include <mutex>

namespace FiberTaskingLib {

template<class T>
class TlsVariable {

private:
	std::unordered_map<ThreadId, T> data;
	std::mutex lock;

public:
	T Get() {
		ThreadId id = FTLGetCurrentThreadId();

		// operator[] is not thread-safe, since it will create a node if one doesn't already exist
		// So we have to lock on reads as well
		// We *could* use find() first, then only lock if it doesn't exist, but the
		// thread-contention should be low enough that it's not worth the extra code
		std::lock_guard<std::mutex> lockGuard(lock);
		return data[id];
	}

	void Set(T value) {
		ThreadId id = FTLGetCurrentThreadId();

		std::lock_guard<std::mutex> lockGuard(lock);
		data[id] = value;
	}
};

#define TLS_VARIABLE(type, name) ::FiberTaskingLib::TlsVariable<type> name

template<class T>
inline T GetTLSData(TlsVariable<T> &tlsVariable) {
	return tlsVariable.Get();
}

template<class T>
inline void SetTLSData(TlsVariable<T> &tlsVariable, T value) {
	tlsVariable.Set(value);
}

} // End of namespace FiberTaskingLib

#endif
