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

#include <EASTL/internal/config.h>

#include <cstdio>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif


// EASTL expects us to define these, see allocator.h line 194
void *operator new[](size_t size, const char *pName, int flags, unsigned debugFlags, const char *file, int line) {
	return malloc(size);
}

void *operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char *pName, int flags, unsigned debugFlags, const char *file, int line) {
	// this allocator doesn't support alignment
	EASTL_ASSERT(alignment <= 8);
	return malloc(size);
}

// EASTL also wants us to define this (see string.h line 197)
int Vsnprintf8(char8_t *pDestination, size_t n, const char8_t *pFormat, va_list arguments) {
	#ifdef _MSC_VER
		return _vsnprintf(pDestination, n, pFormat, arguments);
	#else
		return vsnprintf(pDestination, n, pFormat, arguments);
	#endif
}
