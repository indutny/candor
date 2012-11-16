BUILDTYPE ?= Debug
JOBS ?= 1
ARCH ?= x64

all: libcandor.a can

build:
	./candor_gyp -f make -Dosx_arch=$(ARCH)

libcandor.a: build
	$(MAKE) -j $(JOBS) -C out candor
	ln -sf out/$(BUILDTYPE)/libcandor.a libcandor.a

can: build
	$(MAKE) -j $(JOBS) -C out can
	ln -sf out/$(BUILDTYPE)/can can

test-runner: build
	$(MAKE) -j $(JOBS) -C out test
	ln -sf out/$(BUILDTYPE)/test test-runner

test: test-runner can
	@./test-runner splaytree
	@./test-runner parser
	@./test-runner scope
#	@./test-runner fullgen
#	@./test-runner hir
#	@./test-runner lir
	@./test-runner functional
	@./test-runner binary
	@./test-runner numbers
	@./test-runner api
	@./test-runner gc
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
	@./can test/functional/regressions/regr-4.can

clean:
	-rm -rf out
	-rm libcandor.a can test-runner

.PHONY: clean all build test libcandor.a can test-runner
