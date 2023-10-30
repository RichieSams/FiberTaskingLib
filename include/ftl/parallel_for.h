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

#include "ftl/task_scheduler.h"
#include "ftl/wait_group.h"

#include <type_traits>
#if defined(FTL_CPP_17)
#	include <iterator>
#endif

namespace ftl {

template <typename T>
using ParallelForTaskFunction = void(TaskScheduler *taskScheduler, T *value);

template <typename ItrType, typename Callable>
void ParallelFor(TaskScheduler *taskScheduler, ItrType begin, ItrType end, size_t batchSize, Callable &&func, TaskPriority priority) {
	struct ParallelForArg {
		ItrType Start = ItrType();
		size_t Count = 0;
		typename std::remove_reference<Callable>::type *Function = nullptr;
	};

	const size_t dataSize = static_cast<size_t>(std::distance(begin, end));

	const size_t numBatches = (dataSize + (batchSize - 1)) / batchSize;
	ParallelForArg *internalArgs = new ParallelForArg[numBatches];

	size_t remaining = dataSize;
	WaitGroup wg(taskScheduler);
	ItrType current = begin;
	for (size_t i = 0; i < numBatches; ++i) {
		const size_t count = remaining < batchSize ? remaining : batchSize;

		ParallelForArg *wrapperArgs = &internalArgs[i];
		wrapperArgs->Start = current;
		wrapperArgs->Count = count;
		wrapperArgs->Function = &func;

		remaining -= count;
		current += static_cast<typename std::iterator_traits<ItrType>::difference_type>(count);

		Task wrapperTask{};
		wrapperTask.ArgData = wrapperArgs;
		wrapperTask.Function = [](TaskScheduler *ts, void *arg_) {
			ParallelForArg *argData = static_cast<ParallelForArg *>(arg_);
			size_t j = 0;
			ItrType iter = argData->Start;
			for (; j < argData->Count; ++j, ++iter) {
				(*argData->Function)(ts, &(*iter));
			}
		};
		taskScheduler->AddTask(wrapperTask, priority, &wg);
	}

	wg.Wait();
	delete[] internalArgs;
}

template <typename T, typename Callable>
void ParallelFor(TaskScheduler *taskScheduler, T *data, size_t dataSize, size_t batchSize, Callable &&func, TaskPriority priority) {
	ParallelFor(taskScheduler, data, data + dataSize, batchSize, func, priority);
}

template <typename Iterable, typename Callable>
void ParallelFor(TaskScheduler *taskScheduler, Iterable iterable, size_t batchSize, Callable &&func, TaskPriority priority) {
	ParallelFor(taskScheduler, iterable.begin(), iterable.end(), batchSize, func, priority);
}

} // End of namespace ftl
