# Host-side test runner for everything in lib/core.
#
# The firmware itself is built with PlatformIO (`pio run`), but lib/core is
# deliberately free of Arduino and hardware headers so it can be compiled and
# tested with nothing but a C++17 compiler. `make test` is what CI runs and what
# you should run before pushing.

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Werror -Ilib/core
BUILD    := build/test

CORE_SRC := $(wildcard lib/core/*.cpp)
SUITES   := $(notdir $(patsubst %/,%,$(wildcard test/test_*/)))
BINS     := $(addprefix $(BUILD)/,$(SUITES))

.PHONY: test clean preview
test: $(BINS)
	@echo
	@fail=0; for b in $(BINS); do ./$$b || fail=1; done; \
	 echo; if [ $$fail -eq 0 ]; then echo "all suites passed"; \
	 else echo "SUITE FAILURES"; fi; exit $$fail

# One explicit rule per suite: a pattern rule cannot carry two stems, and each
# suite lives in test/<name>/<name>.cpp.
define SUITE_RULE
$(BUILD)/$(1): test/$(1)/$(1).cpp $$(CORE_SRC)
	@mkdir -p $$(BUILD)
	@$$(CXX) $$(CXXFLAGS) -o $$@ $$< $$(CORE_SRC)
endef
$(foreach s,$(SUITES),$(eval $(call SUITE_RULE,$(s))))

clean:
	@rm -rf build

# Host renderer for the globe, sharing lib/core with the firmware. Writes PPM;
# see tools/ppm2png.py to look at the result.
preview: $(BUILD)/../preview

$(BUILD)/../preview: tools/preview.cpp $(CORE_SRC)
	@mkdir -p build
	@$(CXX) $(CXXFLAGS) -o build/preview $< $(CORE_SRC)
	@echo "build/preview"
