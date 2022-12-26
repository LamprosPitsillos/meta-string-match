MAIN_CPP_INCLUDES := meta_string_match.h

BINARY_NAME := test_program

CPP_STD := c++20
OPTIMIZATION_LEVEL := O3
USE_WALL := true
ifeq ($(USE_WALL), true)
POSSIBLE_WALL := -Wall
else
undefine POSSIBLE_WALL
endif

CLANG_PREAMBLE := clang++-15 -std=$(CPP_STD) -$(OPTIMIZATION_LEVEL) $(POSSIBLE_WALL) -fno-exceptions

.PHONY: all unoptimized clean

all: test/bin/$(BINARY_NAME)

unoptimized:
	$(MAKE) OPTIMIZATION_LEVEL:=O0

test/bin/$(BINARY_NAME): test/bin/main.o
	$(CLANG_PREAMBLE) -o test/bin/$(BINARY_NAME) test/bin/main.o

test/bin/main.o: test/main.cpp $(MAIN_CPP_INCLUDES) test/bin/.dirstamp
	$(CLANG_PREAMBLE) -c -I. -o test/bin/main.o test/main.cpp

test/bin/.dirstamp: test/.dirstamp
	mkdir -p test/bin
	touch test/bin/.dirstamp

test/.dirstamp:
	mkdir -p test
	touch test/.dirstamp

clean:
	git clean -fdx
