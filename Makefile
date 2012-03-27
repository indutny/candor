BUILDTYPE := Debug
TESTBIN := build/test/out/$(BUILDTYPE)/test
LIBCANDOR := build/candor/out/$(BUILDTYPE)/libcandor.a
CANBIN := build/candor/out/$(BUILDTYPE)/can

all: candor

candor $(LIBCANDOR) $(CANBIN):
	tools/gyp/gyp --generator-output=build/candor --format=make \
		--depth=. candor.gyp
	make -C build/candor

$(TESTBIN):
	tools/gyp/gyp --generator-output=build/test --format=make \
		--depth=. test/test.gyp
	make -C build/test

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
	rm -f $(OBJS) build

.PHONY: all candor test
