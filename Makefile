BUILDTYPE := Debug
TESTBIN := build/out/$(BUILDTYPE)/test
LIBCANDOR := build/out/$(BUILDTYPE)/libcandor.a
CANBIN := build/out/$(BUILDTYPE)/can

all: $(LIBCANDOR)

build:
	tools/gyp/gyp --generator-output=build --format=make \
		--depth=. candor.gyp test/test.gyp

$(LIBCANDOR): build
	make -C build candor

$(CANBIN): build
	make -C build can

$(TESTBIN): build
	make -C build test

test: $(TESTBIN) $(CANBIN)
	@$(TESTBIN) parser
	@$(TESTBIN) scope
	@$(TESTBIN) functional
	@$(TESTBIN) binary
	@$(TESTBIN) numbers
	@$(TESTBIN) api
	@$(TESTBIN) gc
	@$(CANBIN) test/functional/return.can
	@$(CANBIN) test/functional/basics.can
	@$(CANBIN) test/functional/arrays.can
	@$(CANBIN) test/functional/objects.can
	@$(CANBIN) test/functional/binary.can
	@$(CANBIN) test/functional/while.can
	@$(CANBIN) test/functional/clone.can
	@$(CANBIN) test/functional/functions.can
	@$(CANBIN) test/functional/strings.can
	@$(CANBIN) test/functional/regressions/regr-1.can
	@$(CANBIN) test/functional/regressions/regr-2.can
	@$(CANBIN) test/functional/regressions/regr-3.can

clean:
	rm -rf build

.PHONY: all test
