ifeq ($(OS),Windows_NT)
export SHELL=cmd
endif

FIBER_GUARD_PAGES?=
CPP_17?=
WERROR?=
CMAKE_EXTRA_ARGS?=

CMAKE_OVERRIDE_ARGS=

ifneq ($(FIBER_GUARD_PAGES),)
CMAKE_OVERRIDE_ARGS+=-DFTL_FIBER_STACK_GUARD_PAGES=$(FIBER_GUARD_PAGES)
endif
ifneq ($(CPP_17),)
CMAKE_OVERRIDE_ARGS+=-DFTL_CPP_17=$(CPP_17)
endif
ifneq ($(WERROR),)
CMAKE_OVERRIDE_ARGS+=-DFTL_WERROR=$(WERROR)
endif

CMAKE_OVERRIDE_ARGS+=$(CMAKE_EXTRA_ARGS)

ifeq ($(OS),Windows_NT)
CMAKE_PRESET?=Win_x64_Debug
else
CMAKE_PRESET?=Unix_x64_Debug
endif


.PHONY: build

all: generate build clean


#################################
# Building
#################################

generate:
	cmake --version
	cmake --preset=$(CMAKE_PRESET) .

build: generate
	cmake --build --preset=$(CMAKE_PRESET) -j


#################################
# Testing
#################################

test:
ifeq ($(OS),Windows_NT)
	cmd /c "cd build\Debug\tests && ftl-test.exe"
	cmd /c "cd build\RelWithDebInfo\tests && ftl-test.exe"
else
	/bin/sh -c "(cd build/Debug/tests && exec ./ftl-test)"
	/bin/sh -c "(cd build/RelWithDebInfo/tests && exec ./ftl-test)"
endif

benchmark:
ifeq ($(OS),Windows_NT)
	cmd /c "cd build\Debug\benchmarks && ftl-benchmark.exe"
	cmd /c "cd build\RelWithDebInfo\benchmarks && ftl-benchmark.exe"
else
	/bin/sh -c "(cd build/Debug/benchmarks && exec ./ftl-benchmark)"
	/bin/sh -c "(cd build/RelWithDebInfo/benchmarks && exec ./ftl-benchmark)"
endif

valgrind_run:
	/bin/sh -c "(cd build/Valgrind/tests && exec valgrind --leak-check=yes --log-file=memcheck_output.txt ./ftl-test)"


#################################
# Miscellaneous
#################################

clean:
ifeq ($(OS),Windows_NT)
	if exist build del /s /q build > nul
	if exist build rmdir /s /q build > nul
else
	rm -rf build
endif

format:
	docker run --rm -v $(CURDIR):/app -w /app quay.io/richiesams/clang-tools-extra:latest /usr/bin/python3 tools/run-clang-format.py -r --clang-format-executable clang-format-15 -i benchmarks examples include source tests

format_check:
	docker run --rm -v $(CURDIR):/app -w /app quay.io/richiesams/clang-tools-extra:latest /usr/bin/python3 tools/run-clang-format.py -r --clang-format-executable clang-format-15 benchmarks examples include source tests

lint:
	docker run --rm -v $(CURDIR):/app -w /app quay.io/richiesams/clang-tools-extra:latest /usr/bin/python3 tools/run-clang-tidy.py
