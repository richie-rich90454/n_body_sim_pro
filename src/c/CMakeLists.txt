# C17 numerical/HPC engine. Public API headers live in include/hpcsim.

add_library(${HPCSIM_C_LIBRARY} STATIC
    core/status.c
    core/particle_system.c
    memory/allocator.c
    physics/gravity.c
    physics/gravity_openmp.c
    physics/integrator.c
    diagnostics/numerics.c
    generation/random.c
    generation/presets.c
    threading/threading.c)

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
