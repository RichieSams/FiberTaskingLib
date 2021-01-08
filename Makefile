ifeq ($(OS),Windows_NT)
	detected_OS := Windows
	export SHELL=cmd
else
	detected_OS := $(shell uname -s)
endif


export COMPILER?=gcc
export VERSION?=9

ifeq ($(COMPILER),gcc)

export CC=gcc
export CXX=g++

else
ifeq ($(COMPILER),clang)

export CC=clang
export CXX=clang++

endif
endif

FIBER_GUARD_PAGES?=1
CPP_17?=0
WERROR?=1
CMAKE_EXTRA_ARGS?=

ifeq ($(FIBER_GUARD_PAGES),1)
FIBER_STACK_CMAKE_ARGS= -DFTL_FIBER_STACK_GUARD_PAGES=1
else
FIBER_STACK_CMAKE_ARGS=
endif

ifeq ($(CPP_17),1)
CPP_17_CMAKE_ARGS= -DFTL_CPP_17=1
else
CPP_17_CMAKE_ARGS=
endif

CMAKE_GEN_NAME?=Visual Studio 15 2017 Win64
CMAKE_ARCH_ARG?=

DOCKER_IMAGE=quay.io/richiesams/docker_$(COMPILER):$(VERSION)

.PHONY: pull_image generate_linux generate_linux_native build_linux test_linux clean_linux generate_osx build_osx test_osx clean_osx generate_windows build_windows test_windows clean_windows valgrind_linux_build valgrind_linux_build_native valgrind_linux_run

all: generate_linux build_linux

generate_linux:
	docker run --rm -v $(CURDIR):/app -w /app $(DOCKER_IMAGE) make COMPILER=$(COMPILER) VERSION=$(VERSION) FIBER_GUARD_PAGES=$(FIBER_GUARD_PAGES) CPP_17=$(CPP_17) WERROR=$(WERROR) CMAKE_EXTRA_ARGS=$(CMAKE_EXTRA_ARGS) generate_linux_native

generate_linux_native:
	mkdir -p build_linux/debug
	mkdir -p build_linux/release
	cmake --version
	cmake $(FIBER_STACK_CMAKE_ARGS) $(CPP_17_CMAKE_ARGS) -DFTL_WERROR=$(WERROR) $(CMAKE_EXTRA_ARGS) -DCMAKE_BUILD_TYPE=Debug -Bbuild_linux/debug .
	cmake $(FIBER_STACK_CMAKE_ARGS) $(CPP_17_CMAKE_ARGS) -DFTL_WERROR=$(WERROR) $(CMAKE_EXTRA_ARGS) -DCMAKE_BUILD_TYPE=Release -Bbuild_linux/release .

build_linux:
	docker run --rm -v $(CURDIR):/app -w /app $(DOCKER_IMAGE) make -j -C build_linux/debug
	docker run --rm -v $(CURDIR):/app -w /app $(DOCKER_IMAGE) make -j -C build_linux/release

test_linux:
	docker run --rm -v $(CURDIR):/app -w /app $(DOCKER_IMAGE) /bin/sh -c "(cd build_linux/debug/tests && exec ./ftl-test)"
	docker run --rm -v $(CURDIR):/app -w /app $(DOCKER_IMAGE) /bin/sh -c "(cd build_linux/release/tests && exec ./ftl-test)"

clean_linux:
	docker run --rm -v $(CURDIR):/app -w /app $(DOCKER_IMAGE) rm -rf build_linux

valgrind_linux_build:
	docker run --rm -v $(CURDIR):/app -w /app $(DOCKER_IMAGE) make COMPILER=$(COMPILER) VERSION=$(VERSION) FIBER_GUARD_PAGES=$(FIBER_GUARD_PAGES) CPP_17=$(CPP_17) WERROR=$(WERROR) CMAKE_EXTRA_ARGS=$(CMAKE_EXTRA_ARGS) valgrind_linux_build_native

valgrind_linux_build_native:
	mkdir -p build_linux_debug
	cmake $(FIBER_STACK_CMAKE_ARGS) $(CPP_17_CMAKE_ARGS) -DFTL_WERROR=$(WERROR) $(CMAKE_EXTRA_ARGS) -DFTL_VALGRIND=1 -Bbuild_linux_debug -DCMAKE_BUILD_TYPE=Debug .
	make -j -C build_linux_debug

valgrind_linux_run:
	docker run --rm -v $(CURDIR):/app -w /app $(DOCKER_IMAGE) /bin/sh -c "(cd build_linux_debug/tests && exec valgrind --leak-check=yes --log-file=memcheck_output.txt ./ftl-test)"


generate_osx:
	mkdir -p build_linux/debug
	mkdir -p build_linux/release
	cmake --version
	cmake $(FIBER_STACK_CMAKE_ARGS) $(CPP_17_CMAKE_ARGS) -DFTL_WERROR=$(WERROR) $(CMAKE_EXTRA_ARGS) -DCMAKE_BUILD_TYPE=Debug -Bbuild_osx/debug .
	cmake $(FIBER_STACK_CMAKE_ARGS) $(CPP_17_CMAKE_ARGS) -DFTL_WERROR=$(WERROR) $(CMAKE_EXTRA_ARGS) -DCMAKE_BUILD_TYPE=Release -Bbuild_osx/release .

build_osx:
	make -j -C build_osx/debug
	make -j -C build_osx/release

test_osx:
	(cd build_osx/debug/tests && exec ./ftl-test)
	(cd build_osx/release/tests && exec ./ftl-test)

clean_osx:
	rm -rf build_osx


generate_windows:
	if not exist build_windows mkdir build_windows
	cmake --version
	cmake -G "$(CMAKE_GEN_NAME)" $(CMAKE_ARCH_ARG) -Bbuild_windows .

build_windows:
	cmake --build build_windows --config Debug -- /v:m /m
	cmake --build build_windows --config Release -- /v:m /m

test_windows:
	cd build_windows\tests\Debug & ftl-test.exe
	cd build_windows\tests\Release & ftl-test.exe

clean_windows:
	if exist rmdir /s /q build_windows

format:
	docker run --rm -v $(CURDIR):/app -w /app quay.io/richiesams/clang-tools-extra:latest /usr/bin/python3 tools/run-clang-format.py -r --clang-format-executable clang-format-8 -i benchmarks examples include source tests

format_check:
	docker run --rm -v $(CURDIR):/app -w /app quay.io/richiesams/clang-tools-extra:latest /usr/bin/python3 tools/run-clang-format.py -r --clang-format-executable clang-format-8 benchmarks examples include source tests

lint:
	docker run --rm -v $(CURDIR):/app -w /app quay.io/richiesams/clang-tools-extra:latest /usr/bin/python3 tools/run-clang-tidy.py
