CPPFLAGS += -Wall -Wextra -Wno-unused-parameter
CPPFLAGS += -fPIC -Iinclude
CPPFLAGS += -fno-strict-aliasing
CPPFLAGS += -g

ifeq ($(shell sh -c 'uname -s 2>/dev/null'),Darwin)
	OS = Darwin
else
	OS = Linux
endif

ifeq ($(MODE),release)
	CPPFLAGS += -O3
	LIBCPPFLAGS += -DNDEBUG
endif

ifeq ($(ARCH),)
	ARCH = $(shell sh -c 'uname -m | sed -e "s/i.86/ia32/;s/x86_64/x64/;s/amd64/x64/"')
endif

all: candor.a

DEPS += src/utils.h

OBJS += src/api.o
OBJS += src/zone.o
OBJS += src/lexer.o
OBJS += src/visitor.o
OBJS += src/parser.o
OBJS += src/scope.o
OBJS += src/cpu.o
OBJS += src/code-space.o
OBJS += src/gc.o
OBJS += src/heap.o
OBJS += src/source-map.o
OBJS += src/runtime.o

ifeq ($(ARCH),ia32)
	ifeq ($(OS),Darwin)
		CPPFLAGS += -arch i386
	else
		CPPFLAGS += -m32
	endif
	OBJS += src/ia32/assembler-ia32.o
	OBJS += src/ia32/macroassembler-ia32.o
	OBJS += src/ia32/stubs-ia32.o
	OBJS += src/ia32/fullgen-ia32.o

	CPPFLAGS += -D__ARCH=ia32
else
	OBJS += src/x64/assembler-x64.o
	OBJS += src/x64/macroassembler-x64.o
	OBJS += src/x64/stubs-x64.o
	OBJS += src/x64/fullgen-x64.o

	CPPFLAGS += -D__ARCH=x64
endif

ifeq ($(OS),Darwin)
	CPPFLAGS += -D__PLATFORM=darwin
else
	CPPFLAGS += -D__PLATFORM=linux
endif

candor.a: $(OBJS)
	$(AR) rcs candor.a $(OBJS)

src/%.o: src/%.cc src/%.h $(DEPS)
	$(CXX) $(LIBCPPFLAGS) $(CPPFLAGS) -Isrc -c $< -o $@

TESTS += test/test-parser
TESTS += test/test-scope
TESTS += test/test-functional
TESTS += test/test-binary
TESTS += test/test-numbers
TESTS += test/test-api
TESTS += test/test-gc

test: candor.a can $(TESTS)
	@test/test-parser
	@test/test-scope
	@test/test-functional
	@test/test-binary
	@test/test-numbers
	@test/test-api
	@test/test-gc
	@./can test/functional/return.can
	@./can test/functional/basics.can
	@./can test/functional/arrays.can
	@./can test/functional/objects.can
	@./can test/functional/binary.can
	@./can test/functional/while.can
	@./can test/functional/clone.can
	@./can test/functional/functions.can
	@./can test/functional/strings.can
	@./can test/functional/regressions/regr-1.can
	@./can test/functional/regressions/regr-2.can
	@./can test/functional/regressions/regr-3.can

test/%: test/%.cc candor.a
	$(CXX) $(CPPFLAGS) -Isrc $< -o $@ candor.a

can: src/can.cc candor.a
	$(CXX) $(CPPFLAGS) -Isrc $< -o $@ candor.a


clean:
	rm -f $(OBJS) candor.a

.PHONY: all test
