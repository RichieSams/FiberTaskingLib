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

#include "fiber_tasking_lib/task_scheduler.h"
#include "fiber_tasking_lib/tagged_heap_backed_linear_allocator.h"

#include "maze10x10.h"

#include <cstdio>
#include <gtest/gtest.h>


struct MazeType {
	MazeType(char *data, int width, int height)
		: Data(data),
		  Width(width),
		  Height(height) {
	}

	char *Data;
	int Width;
	int Height;
};

void PrintMaze(MazeType &maze) {
	for (int y = 0; y < maze.Height; ++y) {
		for (int x = 0; x < maze.Width; ++x) {
			printf("%c", *(maze.Data + (y * maze.Width + x)));
		}
		printf("\n");
	}
}

struct BranchArgs {
	BranchArgs(MazeType *maze, int currX, int currY, FiberTaskingLib::AtomicCounter *completed, FiberTaskingLib::TaggedHeapBackedLinearAllocator *allocator)
		: Maze(maze),
		  CurrX(currX),
		  CurrY(currY),
		  Completed(completed),
		  Allocator(allocator) {
	}

	MazeType *Maze;
	int CurrX;
	int CurrY;
	FiberTaskingLib::AtomicCounter *Completed;
	FiberTaskingLib::TaggedHeapBackedLinearAllocator *Allocator;
};

// Forward declare
TASK_ENTRY_POINT(CheckBranch);

bool CheckDirection(char *areaToCheck, MazeType *maze, int newX, int newY, FiberTaskingLib::AtomicCounter *completed, FiberTaskingLib::TaggedHeapBackedLinearAllocator *allocator, FiberTaskingLib::TaskScheduler *taskScheduler) {
	if (*areaToCheck == 'E') {
		// We found the end
		completed->store(1);
		return true;
	}
	
	if (*areaToCheck == ' ') {
		*areaToCheck = '*';

		BranchArgs *newBranchArgs = new(allocator->allocate(sizeof(BranchArgs))) BranchArgs(maze, newX, newY, completed, allocator);

		FiberTaskingLib::Task newBranch = {CheckBranch, newBranchArgs};
		taskScheduler->AddTask(newBranch);
	}

	return false;
}

TASK_ENTRY_POINT(CheckBranch) {
	BranchArgs *branchArgs = (BranchArgs *)arg;

	// Check right
	if (branchArgs->CurrX + 1 < branchArgs->Maze->Width) {
		char *spaceToCheck = branchArgs->Maze->Data + (branchArgs->CurrY * branchArgs->Maze->Width + branchArgs->CurrX + 1);
		if (CheckDirection(spaceToCheck, branchArgs->Maze, branchArgs->CurrX + 1, branchArgs->CurrY, branchArgs->Completed, branchArgs->Allocator, g_taskScheduler)) {
			return;
		}
	}

	// Check left
	if (branchArgs->CurrX - 1 >= 0) {
		char *spaceToCheck = branchArgs->Maze->Data + (branchArgs->CurrY * branchArgs->Maze->Width + branchArgs->CurrX - 1);
		if (CheckDirection(spaceToCheck, branchArgs->Maze, branchArgs->CurrX - 1, branchArgs->CurrY, branchArgs->Completed, branchArgs->Allocator, g_taskScheduler)) {
			return;
		}
	}

	// Check up
	if (branchArgs->CurrY - 1 >= 0) {
		char *spaceToCheck = branchArgs->Maze->Data + ((branchArgs->CurrY - 1) * branchArgs->Maze->Width + branchArgs->CurrX);
		if (CheckDirection(spaceToCheck, branchArgs->Maze, branchArgs->CurrX, branchArgs->CurrY - 1, branchArgs->Completed, branchArgs->Allocator, g_taskScheduler)) {
			return;
		}
	}

	// Check down
	if (branchArgs->CurrY + 1 < branchArgs->Maze->Height) {
		char *spaceToCheck = branchArgs->Maze->Data + ((branchArgs->CurrY + 1) * branchArgs->Maze->Width + branchArgs->CurrX);
		if (CheckDirection(spaceToCheck, branchArgs->Maze, branchArgs->CurrX, branchArgs->CurrY + 1, branchArgs->Completed, branchArgs->Allocator, g_taskScheduler)) {
			return;
		}
	}
}


TEST(FunctionalTests, Maze10x10) {
	FiberTaskingLib::TaskScheduler *taskScheduler = new FiberTaskingLib::TaskScheduler();
	taskScheduler->Initialize(110);

	FiberTaskingLib::TaggedHeap *taggedHeap = new FiberTaskingLib::TaggedHeap(2097152);
	FiberTaskingLib::TaggedHeapBackedLinearAllocator *allocator = new FiberTaskingLib::TaggedHeapBackedLinearAllocator();
	allocator->init(taggedHeap, 1234);

	std::shared_ptr<FiberTaskingLib::AtomicCounter> completed = std::make_shared<FiberTaskingLib::AtomicCounter>(0u);

	char *mazeData = reinterpret_cast<char *>(allocator->allocate(sizeof(char) * 21 * 21));
	memcpy(mazeData, kMaze10x10, 21 * 21);

	MazeType *maze10x10 = new(allocator->allocate(sizeof(MazeType)))  MazeType(mazeData, 21, 21);
	BranchArgs *startBranch = new(allocator->allocate(sizeof(BranchArgs))) BranchArgs(maze10x10, 0u, 1u, completed.get(), allocator);

	FiberTaskingLib::Task task = {CheckBranch, startBranch};
	taskScheduler->AddTask(task);

	taskScheduler->WaitForCounter(completed, 1);
	taskScheduler->Quit();
	
	// Cleanup
	allocator->destroy();
	delete allocator;
	delete taggedHeap;
	delete taskScheduler;
}