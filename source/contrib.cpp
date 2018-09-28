#include "ftl/atomic_counter.h"
#include "ftl/task_scheduler.h"

namespace ftl {

	void AtomicCounter::addTask(std::unique_ptr<BoundTrampolineBase> bound_trampoline) {
		std::lock_guard<std::mutex> lock(m_boundTrampolinesLock);
		m_boundTrampolines.emplace_back(std::move(bound_trampoline));
		m_taskScheduler->AddTask(*m_boundTrampolines.back(), this);
	}

} //namespace ftl