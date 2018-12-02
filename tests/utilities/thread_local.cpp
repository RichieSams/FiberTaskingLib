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

#include "ftl/atomic_counter.h"
#include "ftl/task_scheduler.h"
#include "ftl/thread_local.h"

#include <gtest/gtest.h>
#include <numeric>

namespace ThreadLocalTests {
	ftl::ThreadLocal<std::size_t>& single_init_singleton(ftl::TaskScheduler* scheduler) {
		static ftl::ThreadLocal<std::size_t> counter(scheduler);

		return counter;
	}

	void single_init(ftl::TaskScheduler* scheduler, void*) {
		*single_init_singleton(scheduler) += 1;
	}

	static std::atomic<std::size_t> side_effect_count = 0;
	ftl::ThreadLocal<std::size_t>& side_effect_singleton(ftl::TaskScheduler* scheduler) {
		static ftl::ThreadLocal<std::size_t> counter(scheduler, []() { return side_effect_count++; });

		return counter;
	}

	void side_effect(ftl::TaskScheduler* scheduler, void*) {
		*side_effect_singleton(scheduler);
	}

	void main_task(ftl::TaskScheduler* scheduler, void* data) {
		// Single Init
		std::vector<ftl::Task> single_init_tasks(scheduler->GetThreadCount(), ftl::Task{ single_init, nullptr });

		ftl::AtomicCounter ac(scheduler);
		scheduler->AddTasks(single_init_tasks.size(), single_init_tasks.data(), &ac);
		scheduler->WaitForCounter(&ac, 0);

		auto single_init_vals = single_init_singleton(scheduler).getAllValues();

		ASSERT_EQ(scheduler->GetThreadCount(), std::accumulate(single_init_vals.begin(), single_init_vals.end(), std::size_t{0}));

		// Side Effects
		std::vector<ftl::Task> side_effect_task(10000, ftl::Task{ side_effect, nullptr });

		scheduler->AddTasks(side_effect_task.size(), side_effect_task.data(), &ac);
		scheduler->WaitForCounter(&ac, 0);

		auto side_effect_vals = side_effect_singleton(scheduler).getAllValues();

		ASSERT_LE(side_effect_count, scheduler->GetThreadCount());
		ASSERT_EQ(side_effect_count - 1, *std::max_element(side_effect_vals.begin(), side_effect_vals.end()));
	}
}


TEST(FunctionalTests, ThreadLocal) {
	ftl::TaskScheduler taskScheduler;
	taskScheduler.Run(400, ThreadLocalTests::main_task);
}