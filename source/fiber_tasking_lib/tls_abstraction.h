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

#include "fiber_tasking_lib/thread_abstraction.h"


#if defined(_MSC_VER)

namespace FiberTaskingLib {

extern THREAD_LOCAL uint tls_threadId;

inline void SetThreadId(uint threadId) {
	tls_threadId = threadId;
}

inline uint GetThreadId() {
	return tls_threadId;
}


#define TLS_VARIABLE(type, name) __declspec(thread) type name;

template<class T>
inline void CreateTLSVariable(T *tlsVariable, size_t numThreads) {
	// No op
}

template<class T>
inline void DestroyTLSVariable(T tlsVariable) {
	// No op
}

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

#include<unordered_map>

namespace FiberTaskingLib {

extern std::unordered_map<ThreadId, uint> g_threadIdToIndexMap;

inline void SetThreadId(uint threadId) {
    g_threadIdToIndexMap[FTLGetCurrentThread()] = threadId;
}

inline uint GetThreadId() {
    return g_threadIdToIndexMap[FTLGetCurrentThread()];
}


#define TLS_VARIABLE(type, name) type *name

template<class T>
inline void CreateTLSVariable(T **tlsVariable, size_t numThreads) {
    *tlsVariable = new T[numThreads];
}

template<class T>
inline void DestroyTLSVariable(T *tlsVariable) {
    delete[] tlsVariable;
}

template<class T>
inline T GetTLSData(T *tlsVariable) {
    return tlsVariable[g_threadIdToIndexMap[FTLGetCurrentThread()]];
}

template<class T>
inline void SetTLSData(T *tlsVariable, T value) {
    tlsVariable[g_threadIdToIndexMap[FTLGetCurrentThread()]] = value;
}

} // End of namespace FiberTaskingLib

#endif
