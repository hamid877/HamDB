cmake_minimum_required(VERSION 3.20)
project(HamDB VERSION 0.1.0 LANGUAGES CXX)

# ── Language standard ──────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Global compile options ─────────────────────────────────────────────────────
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

# ── Include paths ──────────────────────────────────────────────────────────────
include_directories(${PROJECT_SOURCE_DIR}/include)

# ── GoogleTest ─────────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# Prevent overriding parent project's compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# ── Library targets ────────────────────────────────────────────────────────────
add_subdirectory(src/common)
add_subdirectory(src/storage)

# ── hamdb_core convenience interface ──────────────────────────────────────────
add_library(hamdb_core INTERFACE)
target_link_libraries(hamdb_core INTERFACE
    hamdb_common
    hamdb_storage
)
target_include_directories(hamdb_core INTERFACE
    ${PROJECT_SOURCE_DIR}/include
)

# ── Tests ──────────────────────────────────────────────────────────────────────
add_subdirectory(tests/storage)

# ── Examples ──────────────────────────────────────────────────────────────────
add_subdirectory(examples)

# ── Doxygen (optional) ────────────────────────────────────────────────────────
find_package(Doxygen OPTIONAL_COMPONENTS dot)
if(DOXYGEN_FOUND)
    set(DOXYGEN_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/docs)
    set(DOXYGEN_RECURSIVE YES)
    set(DOXYGEN_GENERATE_HTML YES)
    set(DOXYGEN_GENERATE_LATEX NO)
    doxygen_add_docs(docs
        ${PROJECT_SOURCE_DIR}/include
        COMMENT "Generating API documentation with Doxygen"
    )
endif()
