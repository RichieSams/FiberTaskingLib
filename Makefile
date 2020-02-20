ifeq ($(OS),Windows_NT)
	detected_OS := Windows
	export SHELL=cmd
else
	detected_OS := $(shell uname -s)
endif


export COMPILER?=gcc
export VERSION?=6

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

DOCKER_IMAGE=richiesams/docker_$(COMPILER):$(VERSION)

.PHONY: pull_image generate_linux generate_linux_native build_linux test_linux clean_linux generate_osx build_osx test_osx clean_osx generate_windows build_windows test_windows clean_windows

pull_image:
	docker pull $(DOCKER_IMAGE)

generate_linux:
	docker run --rm -v $(CURDIR):/app -w /app $(DOCKER_IMAGE) make COMPILER=$(COMPILER) VERSION=$(VERSION) FIBER_GUARD_PAGES=$(FIBER_GUARD_PAGES) CPP_17=$(CPP_17) WERROR=$(WERROR) CMAKE_EXTRA_ARGS=$(CMAKE_EXTRA_ARGS) generate_linux_native

generate_linux_native:
	mkdir -p build_linux
	cmake --version
	(cd build_linux && exec cmake $(FIBER_STACK_CMAKE_ARGS) $(CPP_17_CMAKE_ARGS) -FTL_WERROR=$(WERROR) $(CMAKE_EXTRA_ARGS) ../)

build_linux:
	docker run --rm -v $(CURDIR):/app -w /app $(DOCKER_IMAGE) make -C build_linux

test_linux:
	docker run --rm -v $(CURDIR):/app -w /app $(DOCKER_IMAGE) /bin/sh -c "(cd build_linux/tests && exec ./ftl-test)"

clean_linux:
	docker run --rm -v $(CURDIR):/app -w /app $(DOCKER_IMAGE) rm -rf build_linux


generate_osx:
	mkdir -p build_osx
	cmake --version
	(cd build_osx && exec cmake $(FIBER_STACK_CMAKE_ARGS) $(CPP_17_CMAKE_ARGS) -FTL_WERROR=$(WERROR) $(CMAKE_EXTRA_ARGS) ../)

build_osx:
	make -C build_osx

test_osx:
	(cd build_osx/tests && exec ./ftl-test)

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
	docker run --rm -v $(CURDIR):/app -w /app richiesams/clang-tools-extra:8 /bin/bash tools/format.sh

lint:
	docker run --rm -v $(CURDIR):/app -w /app richiesams/clang-tools-extra:latest /bin/bash tools/lint-clang.sh
