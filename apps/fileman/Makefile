# Compatibility entry point; CMake is the single source of build rules.
ROOT := $(abspath ../..)
BUILD_DIR := $(ROOT)/build/apps-cmake
CMAKE ?= cmake
PYTHON ?= python3

ifneq ($(strip $(CROSS)),)
TOOLCHAIN_ARGS := -DLEXXOS_APP_CXX=$(CROSS)-g++ -DLEXXOS_APP_OBJCOPY=$(CROSS)-objcopy
endif

.PHONY: all clean
all:
	$(CMAKE) -S "$(ROOT)" -B "$(BUILD_DIR)" -G Ninja -DLEXXOS_APPS_ONLY=ON -DPython3_EXECUTABLE=$(PYTHON) $(TOOLCHAIN_ARGS)
	$(CMAKE) --build "$(BUILD_DIR)" --target app-fileman

clean:
	$(CMAKE) -E rm -f "$(ROOT)/build/apps/fileman.lexx"
	$(CMAKE) -E rm -rf "$(BUILD_DIR)/apps/fileman/fileman"
