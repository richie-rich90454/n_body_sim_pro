cmake_minimum_required(VERSION 3.28)

project(n_body_sim_pro
    VERSION 0.1.0
    DESCRIPTION "Native CPU HPC simulation and visualization engine for large-scale gravitational N-body systems"
    LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Language standards
#   C   : C17  for numerical/HPC kernels and low-level systems
#   C++ : C++20 for the application, orchestration, visualization, and UI
# ---------------------------------------------------------------------------
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(HPCSIM_ENABLE_OPENMP "Enable OpenMP shared-memory parallelism" ON)
option(HPCSIM_ENABLE_DEVELOPER_MODE "Enable developer instrumentation (allocation tracking, telemetry)" ON)
option(HPCSIM_BUILD_TESTS "Build the test suite" ON)
option(HPCSIM_BUILD_BENCHMARKS "Build the benchmark suite" ON)
option(HPCSIM_BUILD_APPLICATION "Build the interactive SDL3/OpenGL/ImGui application" ON)
option(HPCSIM_ENABLE_SANITIZERS "Enable AddressSanitizer/UndefinedBehaviorSanitizer" OFF)
option(HPCSIM_ENABLE_LTO "Enable link-time optimization in Release builds" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Output layout
# ---------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ---------------------------------------------------------------------------
# Aggressive compiler warnings (Clang and GCC; MSVC is secondary support)
# ---------------------------------------------------------------------------
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    foreach(flag
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Wcast-qual
        -Wwrite-strings
        -Wstrict-prototypes
        -Wmissing-prototypes
        -Wold-style-definition
        -Wvla
        -Wdouble-promotion)
        list(APPEND HPCSIM_BASE_WARNING_FLAGS ${flag})
    endforeach()
    add_compile_options(${HPCSIM_BASE_WARNING_FLAGS})
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU" AND HPCSIM_ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

if(HPCSIM_ENABLE_LTO AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

# ---------------------------------------------------------------------------
# Third-party dependencies
# ---------------------------------------------------------------------------
find_package(SDL3 3.0 REQUIRED)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(
    dear_imgui
    GIT_REPOSITORY git@github.com:ocornut/imgui.git
    GIT_TAG v1.91.9
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(dear_imgui)

# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------
add_subdirectory(src)

if(HPCSIM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(HPCSIM_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
