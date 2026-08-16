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
option(N_BODY_SIM_PRO_ENABLE_OPENMP "Enable OpenMP shared-memory parallelism" ON)
option(N_BODY_SIM_PRO_ENABLE_MPI "Enable MPI distributed execution when an MPI implementation is found" ON)
option(N_BODY_SIM_PRO_ENABLE_DEVELOPER_MODE "Enable developer instrumentation (allocation tracking, telemetry)" ON)
option(N_BODY_SIM_PRO_BUILD_TESTS "Build the test suite" ON)
option(N_BODY_SIM_PRO_BUILD_BENCHMARKS "Build the benchmark suite" ON)
option(N_BODY_SIM_PRO_BUILD_APPLICATION "Build the interactive SDL3/OpenGL/ImGui application" ON)
option(N_BODY_SIM_PRO_ENABLE_SANITIZERS "Enable AddressSanitizer/UndefinedBehaviorSanitizer" OFF)
option(N_BODY_SIM_PRO_ENABLE_LTO "Enable link-time optimization in Release builds" OFF)

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
# Aggressive compiler warnings for N-Body Sim Pro code.
#
# Applied per-target (not globally) so third-party libraries such as Dear
# ImGui are not compiled with N-Body Sim Pro's strict flag set.
# ---------------------------------------------------------------------------
function(n_body_sim_pro_apply_warnings target)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        return()
    endif()
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Wcast-qual
        -Wwrite-strings)
    target_compile_options(${target} PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes>
        $<$<COMPILE_LANGUAGE:C>:-Wmissing-prototypes>
        $<$<COMPILE_LANGUAGE:C>:-Wold-style-definition>
        $<$<COMPILE_LANGUAGE:C>:-Wvla>
        $<$<COMPILE_LANGUAGE:C>:-Wdouble-promotion>)
    if(N_BODY_SIM_PRO_ENABLE_SANITIZERS)
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

if(N_BODY_SIM_PRO_ENABLE_LTO AND CMAKE_BUILD_TYPE STREQUAL "Release")
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
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(dear_imgui)

# Static GLEW (GLEW::glew_s). Prefer a system package that exports the static
# target; distributions that ship only the shared library (e.g. Homebrew)
# cannot provide GLEW::glew_s, so build static GLEW from source in that case.
find_package(GLEW QUIET)
if(NOT TARGET GLEW::glew_s)
    message(STATUS "System GLEW lacks the static GLEW::glew_s target; building static GLEW from source")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
    set(BUILD_UTILS OFF CACHE BOOL "Build GLEW utilities" FORCE)
    FetchContent_Declare(
        glew
        GIT_REPOSITORY https://github.com/nigels-com/glew.git
        GIT_TAG glew-2.3.1
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR build/cmake)
    FetchContent_MakeAvailable(glew)
    add_library(GLEW::glew_s ALIAS glew_s)
endif()

# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------
set(N_BODY_SIM_PRO_C_LIBRARY n_body_sim_pro_c)
set(N_BODY_SIM_PRO_CPP_LIBRARY n_body_sim_pro_cpp)
set(N_BODY_SIM_PRO_EXECUTABLE n_body_sim_pro)

add_subdirectory(src)

if(N_BODY_SIM_PRO_BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    enable_testing()
    add_subdirectory(tests)
endif()

if(N_BODY_SIM_PRO_BUILD_BENCHMARKS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks)
    add_subdirectory(benchmarks)
endif()
