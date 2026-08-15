# C17 numerical/HPC engine. Public API headers live in include/hpcsim.

add_library(${HPCSIM_C_LIBRARY} STATIC
    core/status.c
    core/particle_system.c
    memory/allocator.c
    physics/gravity.c
    physics/integrator.c
    diagnostics/numerics.c)

target_include_directories(${HPCSIM_C_LIBRARY} PUBLIC
    ${CMAKE_SOURCE_DIR}/include)

target_compile_features(${HPCSIM_C_LIBRARY} PUBLIC c_std_17)
