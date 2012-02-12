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
	CPPFLAGS += -DNDEBUG
endif

ifeq ($(ARCH),)
	ARCH = $(shell sh -c 'uname -m | sed -e "s/i.86/i386/;s/x86_64/x64/;s/amd64/x64/"')
endif

all: dotlang.a

OBJS += src/dotlang.o
OBJS += src/zone.o
OBJS += src/lexer.o
OBJS += src/parser.o
OBJS += src/visitor.o
OBJS += src/compiler.o
OBJS += src/scope.o

ifeq ($(ARCH),i386)
	ifeq ($(OS),Darwin)
		CPPFLAGS += -arch i386
	else
		CPPFLAGS += -m32
	endif
	OBJS += src/ia32/assembler-ia32.o
	OBJS += src/ia32/fullgen-ia32.o
else
	OBJS += src/x64/assembler-x64.o
	OBJS += src/x64/fullgen-x64.o
endif

ifeq ($(OS),Darwin)
	CPPFLAGS += -D__PLATFORM=darwin
else
	CPPFLAGS += -D__PLATFORM=linux
endif

ifeq ($(ARCH),i386)
	CPPFLAGS += -D__ARCH=ia32
else
	CPPFLAGS += -D__ARCH=x64
endif

dotlang.a: $(OBJS)
	$(AR) rcs dotlang.a $(OBJS)

src/%.o: src/%.cc
	$(CXX) $(CPPFLAGS) -Isrc -c $< -o $@

TESTS += test/test-parser
TESTS += test/test-api

test: $(TESTS)
	@test/test-parser
	@test/test-api

test/%: test/%.cc dotlang.a
	$(CXX) $(CPPFLAGS) -Isrc $< -o $@ dotlang.a

clean:
	rm -f $(OBJS) dotlang.a

.PHONY: all test
