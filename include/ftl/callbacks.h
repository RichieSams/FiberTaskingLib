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

/**
 * @brief Called before worker threads are created
 *
 * @param context        EventCallbacks::Context
 * @param threadCount    The number of threads created
 */
using ThreadCreationCallback = void (*)(void *context, unsigned threadCount);
/**
 * @brief Called before fibers are created
 *
 * @param context       EventCallbacks::Context
 * @param fiberCount    The number of fibers created
 */
using FiberCreationCallback = void (*)(void *context, unsigned fiberCount);
/**
 * @brief Called for each thread event
 *
 * @param context        EventCallbacks::Context
 * @param threadIndex    The index of the thread
 */
using ThreadEventCallback = void (*)(void *context, unsigned threadIndex);
/**
 * @brief Called when a fiber is attached to a thread
 *
 * @param context       EventCallbacks::Context
 * @param fiberIndex    The index of the fiber
 */
using FiberAttachedCallback = void (*)(void *context, unsigned fiberIndex);
/**
 * @brief Called when a fiber is detached from a thread
 *
 * @param context       EventCallbacks::Context
 * @param fiberIndex    The index of the fiber
 * @param isMidTask     True = the fiber was suspended mid-task due to a wait. False = the fiber was suspended for another reason
 */
using FiberDetachedCallback = void (*)(void *context, unsigned fiberIndex, bool isMidTask);

struct EventCallbacks {
	void *Context = nullptr;

	ThreadCreationCallback OnThreadsCreated = nullptr;
	FiberCreationCallback OnFibersCreated = nullptr;

	ThreadEventCallback OnWorkerThreadStarted = nullptr;
	ThreadEventCallback OnWorkerThreadEnded = nullptr;

	FiberAttachedCallback OnFiberAttached = nullptr;
	FiberDetachedCallback OnFiberDetached = nullptr;
};

} // End of namespace ftl
