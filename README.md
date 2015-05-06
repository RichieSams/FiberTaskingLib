**Fiber Tasking Lib**
====================

This is a library for enabling task-based multi-threading. It allows execution of task graphs with arbitrary dependencies. Dependencies are represented as atomic counters.

Under the covers, the task graph is executed using fibers, which in turn, are run on a pool of worker threads (one thread per CPU core). This allows the scheduler to wait on dependencies without task chaining or context switches. 

This library was created as a proof of concept of the ideas presented by
Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'

[Free GDC Vault Recorded Presentation](http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine)

[Slides](http://twvideo01.ubm-us.net/o1/vault/gdc2015/presentations/Gyrling_Christian_Parallelizing_The_Naughty.pdf)

<br />

##How it works
Honestly, the best explaination is to watch Christian Gyrling's talk. It's free to watch (as of the time of writing) from the GDC vault. His explaination of fibers as well as how they used the fiber system in their game engine is excellent. However, I will try to give a TL;DR; version here

<br />

##What are fibers
A [fiber](https://msdn.microsoft.com/en-us/library/windows/desktop/ms682661%28v=vs.85%29.aspx) consists of a stack and a small storage space for registers. It's a very lightweight execution context that runs inside a thread. You can think of it as a shell of an actual thread. 

Why go though the hassle though? What's the benefit?

The beauty of fibers is that you can switch between them extremely quickly. Ultimately, a switch consists of saving out registers, then swapping the execution pointer and the stack pointer. This is much much faster than a full on thread context switch.

<br />

##How do fibers apply to task-based multithreading?
To answer this question, let's compare to another task-based multithreading library: Intel's [Threading Building Blocks ](https://www.threadingbuildingblocks.org/). TBB is an extremely well polished and successful tasking library. It can handle really complex task graphs and has an excellent scheduler. However, let's imagine a scenario:

Task F starts and does some execution, but now it's waiting on Task B. So, it gets a new task from the scheduler and starts executing that, which then starts another task, and another. In the meantime, Task B finishes and Task F _could_ continue again. However, the only way for Task F to finish executing is for the entire chain to unravel back to it, or to suffer a context switch. 

Now, obviously, this is a contrived example. And as I said above, TBB has an awesome scheduler that works hard to alleviate this problem. That said, fibers can help to eliminate the problem altogether by allowing cheap switching between tasks. This allows us to isolate the execution of one task from another, preventing the 'chaining' effect described above.

<br />

##The Architecture from 10,000 ft
(Christian has some great illustrations on pages 8 - 17 of his slides that help explain the flow of fibers and tasks. I suggest looking at those while you're reading)

**Task Queue** - An 'ordinary' queue for holding the tasks that are waiting to be executed. In the current code, there is only one queue. However, a more sophisticated system might have multiple queues with varying priorities.

**Fiber Pool** - A pool of fibers used for switching to new tasks while the current task is waiting on a dependency. Fibers execute the tasks

**Worker Threads** - 1 per logical CPU core. These run the fibers.

**Waiting Tasks** - A list of the tasks that are waiting for a dependency to be fufilled. Dependencies are represented with atomic counters

<br />

You create a task by calling TaskScheduler::AddTasks()

```C++
Task tasks[10];
for (uint i = 0; i < 10; ++i) {
    tasks[i] = {MyFunctionPointer, myFunctionArg};
}

std::shared_ptr<AtomicCounter> counter = taskScheduler.AddTasks(10, tasks);
```

Tasks can be created on the stack. They're just a simple struct with a function pointer and an optional void \*arg to be passed to the function:

```C++
struct Task {
    TaskFunction Function;
    void *ArgData;
};
```

The tasks get added to the queue, and other threads (or the current one, when it finished the current task) can start executing them when they get popped off the queue.

<br />

Every time you add a _group_ of tasks, the task scheduler returns a pointer to an AtomicCounter. The value of the atomic counter will be equal to the number of tasks queued. Every time a task finishes, the counter will be atomically decremented. You can use this functionality to create depencendies between tasks. You do that with the function

```C++
void TaskScheduler::WaitForCounter(std::shared_ptr<AtomicCounter> &counter, int value);
```

This is where fibers come into play. If the counter == value, the function trivially returns. If not, the scheduler will move the current fiber into the **Waiting Tasks** and grab a new fiber from the **Fiber Pool**. The new fiber pops a new task from the **Task Queue** and starts execution with that.

<br />

But what about the task we stored in **Waiting Tasks**? When will it finish being executed? 

Before a fiber tries to pop a task off the **Task Queue**, it iterates through the **Waiting Tasks** and checks if any dependencies have been met. If so, it will return itself<sup>[\[1\]](https://github.com/RichieSams/FiberTaskingLib/blob/master/source/fiber_tasking_lib/task_scheduler.h#L117)</sup> to the **Fiber Pool** and switch to the fiber that is ready. The fiber will continue execution right where it left off

<br />

##Memory
Memory, or rather ownership of memory, becomes _hairy_ when multi-tasking comes into the picture. Christian presents a few ideas on how to elegantly handle memory within the task system. Namely, linear allocators backed by a tagged page-based allocator. I'm still in the process of implementing all the ideas presented, but I hope to update this once it is.

<br />

##Request for Criticism
This implementation was something I created because I thought Christian's presentation was really interesting and I wanted to explore it myself. I realize that the code is not the best (especially the scheduler, or lack thereof). That said, I would love to hear your critiques of how I could make it better. I will continue to work on this project and improve it as best as possible.





