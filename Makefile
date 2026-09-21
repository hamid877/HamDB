# HamDB — Developer Makefile
#
# Convenience wrapper around CMake + Ninja + clang-tidy.
# All targets delegate to the `build/` directory.
#
# Usage:
#   make configure   – run CMake configuration
#   make build       – compile everything
#   make test        – run CTest
#   make format      – apply clang-format to all C++ sources
#   make lint        – run clang-tidy on all src/ files
#   make check       – build + test + lint (full CI gate)
#   make clean       – remove the build directory

BUILD_DIR  := build
CMAKE_ARGS := -DCMAKE_BUILD_TYPE=Debug \
              -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
              -G Ninja

# Collect all .cpp files under src/ for clang-tidy
CPP_SOURCES := $(shell find src -name '*.cpp')

# Collect all C++ files for clang-format
FORMAT_FILES := $(shell find include src tests examples -name '*.cpp' -o -name '*.hpp')

.PHONY: all configure build format lint test check clean

all: build

## configure: run CMake to initialise the build directory
configure:
	cmake -B $(BUILD_DIR) $(CMAKE_ARGS)

## build: compile all targets
build: configure
	cmake --build $(BUILD_DIR) --parallel

## format: auto-format all C++ files with clang-format
format:
	clang-format -i $(FORMAT_FILES)

## lint: run clang-tidy over every .cpp inside src/
lint: configure
	@echo "Running clang-tidy on $(words $(CPP_SOURCES)) files..."
	@for f in $(CPP_SOURCES); do \
		clang-tidy "$$f" \
			-p $(BUILD_DIR) \
			--header-filter='^$(CURDIR)/(include|src)/.*' \
			-- -std=c++20 -Iinclude \
		|| exit 1; \
	done
	@echo "clang-tidy passed."

## test: run the CTest suite
test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure --parallel

## check: full local CI gate (build → test → lint)
check: build test lint
	@echo "✓ All checks passed."

## clean: remove the build directory
clean:
	rm -rf $(BUILD_DIR)
