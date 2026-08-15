# C17 numerical/HPC engine. Public API headers live in include/hpcsim.

add_library(${HPCSIM_C_LIBRARY} STATIC
    core/status.c
    core/particle_system.c
    memory/allocator.c
    memory/allocation_tracker.c
    memory/memory_estimate.c
    physics/gravity.c
    physics/gravity_openmp.c
    physics/integrator.c
    diagnostics/numerics.c
    generation/random.c
    generation/presets.c
    barnes_hut/barnes_hut.c
    checkpoint/checkpoint.c
    simd/cpu.c
    simd/backend.c
    simd/force_avx2.c
    simd/barnes_hut_avx2.c
    threading/threading.c)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i686" AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    set_source_files_properties(simd/force_avx2.c simd/barnes_hut_avx2.c
        PROPERTIES COMPILE_OPTIONS "-mavx2;-mfma")
elseif(MSVC)
    set_source_files_properties(simd/force_avx2.c simd/barnes_hut_avx2.c
        PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
endif()

target_include_directories(${HPCSIM_C_LIBRARY} PUBLIC
    ${CMAKE_SOURCE_DIR}/include)

target_compile_features(${HPCSIM_C_LIBRARY} PUBLIC c_std_17)

hpcsim_apply_warnings(${HPCSIM_C_LIBRARY})

if(HPCSIM_ENABLE_OPENMP)
    find_package(OpenMP)
    if(OpenMP_C_FOUND)
        target_link_libraries(${HPCSIM_C_LIBRARY} PUBLIC OpenMP::OpenMP_C)
        target_compile_definitions(${HPCSIM_C_LIBRARY} PUBLIC HPCSIM_HAVE_OPENMP)
    endif()
endif()
