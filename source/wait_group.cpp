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

/**
 * This class is directly inspired by golang's WaitGroup struct
 *
 * https://cs.opensource.google/go/go/+/refs/tags/go1.21.3:src/sync/waitgroup.go
 *
 * golang is distributed under the BSD license as follows:
 *
 * Copyright (c) 2009 The Go Authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *    * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/**
 * This class also uses elements from WebKit's WordLock class
 *
 * https://webkit.org/blog/6161/locking-in-webkit/
 * https://trac.webkit.org/browser/trunk/Source/WTF/wtf/WordLock.h?rev=200444
 * https://trac.webkit.org/browser/trunk/Source/WTF/wtf/WordLock.cpp?rev=200444#L151
 *
 * WebKit is distributed under the 2 clause BSD license as follows:
 *
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ftl/wait_group.h"

#include "ftl/assert.h"
#include "ftl/task_scheduler.h"
#include "task_scheduler_internal.h"

namespace ftl {

WaitGroup::WaitGroup(TaskScheduler *taskScheduler)
        : m_taskScheduler(taskScheduler),
          m_counter(0),
          m_word(0) {
}

void WaitGroup::Add(int32_t delta) {
	int32_t prev = m_counter.fetch_add(delta);
	int32_t newValue = prev + delta;
	FTL_ASSERT("Invalid WaitGroup usage - counter went negative", newValue >= 0);

	if (newValue > 0) {
		return;
	}

	// This fiber reduced the value to zero.
	// Wake all the waiting fibers

	// Fast path
	// There are no waiters
	uintptr_t expected = 0;
	if (std::atomic_compare_exchange_weak(&m_word, &expected, kIsQueueLockedBit)) {
		// Release the lock and return
		// We own the lock, so there's no need for a CAS
		m_word.store(0);
		return;
	}

	// Slow path
	while (true) {
		uintptr_t currentWordValue = m_word.load();

		// The fast path CAS spurriously failed
		// Try again
		if (currentWordValue == 0) {
			if (std::atomic_compare_exchange_weak(&m_word, &currentWordValue, kIsQueueLockedBit)) {
				// Release the lock and return
				// We own the lock, so there's no need for a CAS
				m_word.store(0);
				return;
			}
			// Loop around and try again.
			YieldThread();
			continue;
		}

		// There *are* waiters in the queue

		// Acquire the queue lock
		// We proceed only if the queue lock is not held and we succeed in acquiring the queue lock.
		if ((currentWordValue & kIsQueueLockedBit) == kIsQueueLockedBit || !std::atomic_compare_exchange_weak(&m_word, &currentWordValue, currentWordValue | kIsQueueLockedBit)) {
			YieldThread();
			continue;
		}

		break;
	}

	uintptr_t currentWordValue = m_word.load();

	// After we acquire the queue lock the queue must be non-empty. The queue must be non-empty since the fast path failed
	// and we're the only one that can remove from the queue (since WaitGroup will hit zero only once for an Add() / Wait() cycle)
	FTL_ASSERT("lock should be held", (currentWordValue & kIsQueueLockedBit) == kIsQueueLockedBit);
	WaitingFiberBundle *queueHead = reinterpret_cast<WaitingFiberBundle *>(currentWordValue & ~kQueueHeadMask);
	FTL_ASSERT("there should be a WaitingFiberBundle in the queue", queueHead != nullptr);

	// Resume all waiters
	while (queueHead != nullptr) {
		WaitingFiberBundle *next = queueHead->Next;

		queueHead->Next = nullptr;
		queueHead->QueueTail = nullptr;
		m_taskScheduler->AddReadyFiber(queueHead);
		queueHead = next;
	}

	// Reset the lock and the queue
	m_word.store(0);
}

void WaitGroup::Wait(bool pinToCurrentThread) {
	while (true) {
		// Fast path
		// Counter is zero. No need to wait
		if (m_counter.load(std::memory_order_relaxed) == 0) {
			return;
		}

		uintptr_t currentWordValue = m_word.load();

		// Acquire the queue lock
		// We proceed only if the queue lock is not held and we succeed in acquiring the queue lock.
		if ((currentWordValue & kIsQueueLockedBit) == kIsQueueLockedBit || !std::atomic_compare_exchange_weak(&m_word, &currentWordValue, currentWordValue | kIsQueueLockedBit)) {
			YieldThread();
			continue;
		}

		break;
	}

	// We now own the lock
	// Check the counter one last time
	// It's possible Add() released the lock after it already woke up other threads
	if (m_counter.load() == 0) {
		// Release the lock and return
		// We own the lock, so there's no need for a CAS
		uintptr_t currentWordValue = m_word.load();
		m_word.store(currentWordValue & ~kIsQueueLockedBit);

		return;
	}

	uintptr_t currentWordValue = m_word.load();

	WaitingFiberBundle currentFiber{};
	m_taskScheduler->InitWaitingFiberBundle(&currentFiber, pinToCurrentThread);

	WaitingFiberBundle *queueHead = reinterpret_cast<WaitingFiberBundle *>(currentWordValue & ~kQueueHeadMask);
	if (queueHead != nullptr) {
		// Put this fiber at the end of the queue.
		queueHead->QueueTail->Next = &currentFiber;
		queueHead->QueueTail = &currentFiber;

		// Release the queue lock.
		FTL_ASSERT("There should be a head already", (currentWordValue & ~kQueueHeadMask) != 0);
		FTL_ASSERT("we should still hold the queue lock", (currentWordValue & kIsQueueLockedBit) == kIsQueueLockedBit);
		m_word.store(currentWordValue & ~kIsQueueLockedBit);
	} else {
		// Make this fiber be the queue-head.
		queueHead = &currentFiber;
		currentFiber.QueueTail = &currentFiber;

		// Release the queue lock and install ourselves as the head. No need for a CAS loop, since
		// we own the queue lock.
		FTL_ASSERT("there shouldn't be a head yet", (currentWordValue & ~kQueueHeadMask) == 0);
		FTL_ASSERT("we should still hold the queue lock", (currentWordValue & kIsQueueLockedBit) == kIsQueueLockedBit);
		uintptr_t newWordValue = currentWordValue;
		newWordValue |= reinterpret_cast<uintptr_t>(queueHead);
		newWordValue &= ~kIsQueueLockedBit;
		m_word.store(newWordValue);
	}

	// At this point everyone who acquires the queue lock will see `currentFiber` on the queue.
	// `currentFiber.FiberIsSwitched` will still be false though, so any any other threads trying
	// to resume this thread will wait for the current fiber to switch away below

	// Now switch
	m_taskScheduler->SwitchToFreeFiber(&currentFiber.FiberIsSwitched);

	FTL_ASSERT("pointers should be nulled after de-queue", currentFiber.Next == nullptr);
	FTL_ASSERT("pointers should be nulled after de-queue", currentFiber.QueueTail == nullptr);

	// We're back
}

} // End of namespace ftl
