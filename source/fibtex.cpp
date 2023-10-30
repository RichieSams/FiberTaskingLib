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
 * This class is heavily inspired / derived from WebKit's WordLock class
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

#include "ftl/fibtex.h"

#include "ftl/assert.h"
#include "ftl/task_scheduler.h"
#include "ftl/thread_abstraction.h"
#include "task_scheduler_internal.h"

namespace ftl {

Fibtex::Fibtex(TaskScheduler *taskScheduler)
        : m_ableToSpin(taskScheduler->GetThreadCount() > 1),
          m_taskScheduler(taskScheduler),
          m_word(0) {
}

void Fibtex::LockSlow(bool pinToCurrentThread) {
	unsigned spinCount = 0;

	// This magic number turns out to be optimal based on past JikesRVM experiments.
	static constexpr unsigned spinLimit = 40;

	while (true) {
		uintptr_t currentWordValue = m_word.load();

		if ((currentWordValue & kIsLockedBit) == 0) {
			// It's not possible for someone to hold the queue lock while the lock itself is no longer
			// held, since we will only attempt to acquire the queue lock when the lock is held and
			// the queue lock prevents unlock.
			FTL_ASSERT("queue lock should not be held while overall lock is *not* held", (currentWordValue & kIsQueueLockedBit) == 0);

			if (std::atomic_compare_exchange_weak(&m_word, &currentWordValue, currentWordValue | kIsLockedBit)) {
				// Success! We acquired the lock.
				return;
			}
		}

		// If there is no queue and we haven't spun too much, we can just try to spin around again.
		if (m_ableToSpin && (currentWordValue & ~kQueueHeadMask) == 0 && spinCount < spinLimit) {
			spinCount++;
			YieldThread();
			continue;
		}

		// Need to put ourselves on the queue. Create the queue if one does not exist. This requries
		// owning the queue for a little bit. The lock that controls the queue is itself a spinlock.

		// Reload the current word value, since some time may have passed.
		currentWordValue = m_word.load();

		// We proceed only if the queue lock is not held, the WordLock is held, and we succeed in
		// acquiring the queue lock.
		if ((currentWordValue & kIsQueueLockedBit) == kIsQueueLockedBit || (currentWordValue & kIsLockedBit) == 0 || !std::atomic_compare_exchange_weak(&m_word, &currentWordValue, currentWordValue | kIsQueueLockedBit)) {
			YieldThread();
			continue;
		}

		// We own the queue. Nobody can enqueue or dequeue until we're done. Also, it's not possible
		// to release the Fibtex while we hold the queue lock.

		WaitingFiberBundle currentFiber{};
		m_taskScheduler->InitWaitingFiberBundle(&currentFiber, pinToCurrentThread);

		WaitingFiberBundle *queueHead = reinterpret_cast<WaitingFiberBundle *>(currentWordValue & ~kQueueHeadMask);
		if (queueHead != nullptr) {
			// Put this fiber at the end of the queue.
			queueHead->QueueTail->Next = &currentFiber;
			queueHead->QueueTail = &currentFiber;

			// Release the queue lock.
			currentWordValue = m_word.load();
			FTL_ASSERT("there should already be a head pointer", (currentWordValue & ~kQueueHeadMask) != 0);
			FTL_ASSERT("we should still hold everything", (currentWordValue & kIsQueueLockedBit) == kIsQueueLockedBit);
			FTL_ASSERT("we should still hold everything", (currentWordValue & kIsLockedBit) == kIsLockedBit);
			m_word.store(currentWordValue & ~kIsQueueLockedBit);
		} else {
			// Make this fiber be the queue-head.
			queueHead = &currentFiber;
			currentFiber.QueueTail = &currentFiber;

			// Release the queue lock and install ourselves as the head. No need for a CAS loop, since
			// we own the queue lock.
			currentWordValue = m_word.load();
			FTL_ASSERT("there shouldn't be any head pointer", (currentWordValue & ~kQueueHeadMask) == 0);
			FTL_ASSERT("we should still hold everything", (currentWordValue & kIsQueueLockedBit) == kIsQueueLockedBit);
			FTL_ASSERT("we should still hold everything", (currentWordValue & kIsLockedBit) == kIsLockedBit);
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
		// Now we can loop around and try to acquire the lock again.
	}
}

void Fibtex::UnlockSlow() {
	// The fast path can fail either because of spurious weak CAS failure, or because someone put a
	// fiber on the queue, or the queue lock is held. If the queue lock is held, it can only be
	// because someone *will* enqueue a thread onto the queue.

	// Acquire the queue lock, or release the lock. This loop handles both lock release in case the
	// fast path's weak CAS spuriously failed and it handles queue lock acquisition if there is
	// actually something interesting on the queue.
	while (true) {
		uintptr_t currentWordValue = m_word.load();

		FTL_ASSERT("Calling unlock when we're not locked is undefined", (currentWordValue & kIsLockedBit) == kIsLockedBit);

		// No waiters in the queue
		if (currentWordValue == kIsLockedBit) {
			uintptr_t expected = kIsLockedBit;
			if (std::atomic_compare_exchange_weak(&m_word, &expected, 0)) {
				// The fast path's weak CAS had spuriously failed, and now we succeeded. The lock is
				// unlocked and we're done!
				return;
			}
			// Loop around and try again.
			YieldThread();
			continue;
		}

		// Another thread has the queue lock held
		if ((currentWordValue & kIsQueueLockedBit) == kIsQueueLockedBit) {
			YieldThread();
			continue;
		}

		// If it wasn't just a spurious weak CAS failure and if the queue lock is not held, then there
		// must be an entry on the queue.
		FTL_ASSERT("there should be a WaitingFiberBundle in the queue", (currentWordValue & ~kQueueHeadMask) != 0);

		// Attempt to get the queue lock
		if (std::atomic_compare_exchange_weak(&m_word, &currentWordValue, currentWordValue | kIsQueueLockedBit)) {
			break;
		}
	}

	uintptr_t currentWordValue = m_word.load();

	// After we acquire the queue lock, the main lock must still be held and the queue must be
	// non-empty. The queue must be non-empty since only the lockSlow() method could have held the
	// queue lock and if it did then it only releases it after putting something on the queue.
	FTL_ASSERT("both locks should be held", (currentWordValue & kIsLockedBit) == kIsLockedBit);
	FTL_ASSERT("both locks should be held", (currentWordValue & kIsQueueLockedBit) == kIsQueueLockedBit);
	WaitingFiberBundle *queueHead = reinterpret_cast<WaitingFiberBundle *>(currentWordValue & ~kQueueHeadMask);
	FTL_ASSERT("there should be a WaitingFiberBundle in the queue", queueHead != nullptr);

	WaitingFiberBundle *newQueueHead = queueHead->Next;
	// Either this was the only thread on the queue, in which case we delete the queue, or there
	// are still more threads on the queue, in which case we create a new queue head.
	if (newQueueHead != nullptr) {
		newQueueHead->QueueTail = queueHead->QueueTail;
	}

	// Change the queue head, possibly removing it if newQueueHead is null. No need for a CAS loop,
	// since we hold the queue lock and the lock itself so nothing about the lock can change right
	// now.
	currentWordValue = m_word.load();
	FTL_ASSERT("both locks should be held", (currentWordValue & kIsLockedBit) == kIsLockedBit);
	FTL_ASSERT("both locks should be held", (currentWordValue & kIsQueueLockedBit) == kIsQueueLockedBit);
	FTL_ASSERT("the head should still be what we last saw", (currentWordValue & ~kQueueHeadMask) == reinterpret_cast<uintptr_t>(queueHead));

	uintptr_t newWordValue = currentWordValue;
	newWordValue &= ~kIsLockedBit;                             // Release the main lock.
	newWordValue &= ~kIsQueueLockedBit;                        // Release the queue lock.
	newWordValue &= kQueueHeadMask;                            // Clear out the old queue head.
	newWordValue |= reinterpret_cast<uintptr_t>(newQueueHead); // Install new queue head.
	m_word.store(newWordValue);

	// Now the lock is available for acquisition. But we just have to wake up the old queue head.
	// After that, we're done!

	queueHead->Next = nullptr;
	queueHead->QueueTail = nullptr;

	m_taskScheduler->AddReadyFiber(queueHead);

	// The old queue head can now contend for the lock again. We're done!
}

} // End of namespace ftl
