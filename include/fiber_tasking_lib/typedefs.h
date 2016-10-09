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

#include <cstdint>

typedef unsigned char byte;
typedef std::int8_t  int8;
typedef std::uint8_t  uint8;

typedef std::int16_t int16;
typedef std::uint16_t uint16;

typedef unsigned int uint;
typedef std::int32_t int32;
typedef std::uint32_t uint32;

typedef std::int64_t int64;
typedef std::uint64_t uint64;

typedef wchar_t wchar;
