/**
 * FiberTaskingLib - A tasking library that uses fibers for efficient task switching
 *
 * This library was created as a proof of concept of the ideas presented by
 * Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
 *
 * http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
 *
 * FiberTaskingLib is the legal property of Adrian Astley
 * Copyright Adrian Astley 2015 - 2018
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#if defined(FTL_VALGRIND)
#	include <valgrind/helgrind.h>
#	include <valgrind/valgrind.h>

#	define FTL_VALGRIND_ID unsigned m_stackId
#	define FTL_VALGRIND_REGISTER(s, e) m_stackId = VALGRIND_STACK_REGISTER(s, e)
#	define FTL_VALGRIND_DEREGISTER() VALGRIND_STACK_DEREGISTER(m_stackId)

#	define FTL_VALGRIND_HG_DISABLE_CHECKING(s, e) VALGRIND_HG_DISABLE_CHECKING(s, e)
#else
#	define FTL_VALGRIND_ID
#	define FTL_VALGRIND_REGISTER(s, e)
#	define FTL_VALGRIND_DEREGISTER()

#	define FTL_VALGRIND_HG_DISABLE_CHECKING(s, e)
#endif
