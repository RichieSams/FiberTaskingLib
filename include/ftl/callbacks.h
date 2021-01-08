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

namespace ftl {

enum class FiberState : int {
	// The fiber has started execution on a worker thread
	Attached,
	// The fiber is no longer being executed on a worker thread
	Detached
};

using ThreadCreationCallback = void (*)(void *context, unsigned threadCount);
using FiberCreationCallback = void (*)(void *context, unsigned fiberCount);
using ThreadEventCallback = void (*)(void *context, unsigned threadIndex);
using FiberEventCallback = void (*)(void *context, unsigned fiberIndex, FiberState newState);

struct EventCallbacks {
	void *Context = nullptr;

	ThreadCreationCallback OnThreadsCreated = nullptr;
	FiberCreationCallback OnFibersCreated = nullptr;

	ThreadEventCallback OnWorkerThreadStarted = nullptr;
	ThreadEventCallback OnWorkerThreadEnded = nullptr;

	FiberEventCallback OnFiberStateChanged = nullptr;
};

} // End of namespace ftl
