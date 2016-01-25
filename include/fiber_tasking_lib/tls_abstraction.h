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

#include "fiber_tasking_lib/config.h"
#include "fiber_tasking_lib/thread_abstraction.h"


#if defined(FTL_FIBER_IMPL_SUPPORTS_TLS)

namespace FiberTaskingLib {

#define TLS_VARIABLE(type, name) thread_local type name;

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

#include "fiber_tasking_lib/read_write_lock.h"

#include <vector>

namespace FiberTaskingLib {

template<class T>
class TlsVariable {
private:
	struct DataPacket {
		DataPacket() {};
		DataPacket(ThreadId id, T data)
			: Id(id),
			  Data(data) {
		}

		ThreadId Id;
		T Data;
	};

	std::vector<DataPacket> m_data;
	ReadWriteLock m_lock;

public:
	T Get() {
		ThreadId id = FTLGetCurrentThreadId();

		m_lock.LockRead();
		for (auto &packet : m_data) {
			if (packet.Id == id) {
				return packet.Data;
			}
		}
		m_lock.UnlockRead();

		// If we can't find it, default construct it
		return T();
	}

	void Set(T value) {
		ThreadId id = FTLGetCurrentThreadId();

		m_lock.LockWrite();
		for (auto &packet : m_data) {
			if (packet.Id == id) {
				packet.Data = value;

				m_lock.UnlockWrite();
				return;
			}
		}

		// This thread has not yet set a value.
		// Create an entry
		m_data.emplace_back(id, value);
		m_lock.UnlockWrite();
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
