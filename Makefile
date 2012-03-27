BUILDTYPE := Debug
TESTBIN := build/out/$(BUILDTYPE)/test

all: $(LIBCANDOR)

build:
	tools/gyp/gyp --generator-output=build --format=make \
		--depth=. candor.gyp test/test.gyp

libcandor.a: build
	make -C build candor
	ln -sf build/out/$(BUILDTYPE)/libcandor.a libcandor.a

can: build
	make -C build can
	ln -sf build/out/$(BUILDTYPE)/can can

test-runner: build
	make -C build test
	ln -sf build/out/$(BUILDTYPE)/test test-runner

test: test-runner can
	@./test-runner parser
	@./test-runner scope
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

clean:
	rm -rf build

.PHONY: all test
