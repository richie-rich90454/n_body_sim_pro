# C17 numerical/HPC engine. Public API headers live in include/n_body_sim_pro.

add_library(${N_BODY_SIM_PRO_C_LIBRARY} STATIC
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
    generation/presets_parallel.c
    barnes_hut/barnes_hut.c
    checkpoint/checkpoint.c
    simd/cpu.c
    simd/backend.c
    simd/force_avx2.c
    simd/barnes_hut_avx2.c
    threading/threading.c
    threading/numa.c
    mpi/mpi_runtime.c
    mpi/distributed_barnes_hut.c)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i686" AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    set_source_files_properties(simd/force_avx2.c simd/barnes_hut_avx2.c
        PROPERTIES COMPILE_OPTIONS "-mavx2;-mfma")
elseif(MSVC)
    set_source_files_properties(simd/force_avx2.c simd/barnes_hut_avx2.c
        PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
endif()

target_include_directories(${N_BODY_SIM_PRO_C_LIBRARY} PUBLIC
    ${CMAKE_SOURCE_DIR}/include)

target_compile_features(${N_BODY_SIM_PRO_C_LIBRARY} PUBLIC c_std_17)

n_body_sim_pro_apply_warnings(${N_BODY_SIM_PRO_C_LIBRARY})

if(N_BODY_SIM_PRO_ENABLE_OPENMP)
    find_package(OpenMP)
    if(OpenMP_C_FOUND)
        target_link_libraries(${N_BODY_SIM_PRO_C_LIBRARY} PUBLIC OpenMP::OpenMP_C)
        target_compile_definitions(${N_BODY_SIM_PRO_C_LIBRARY} PUBLIC N_BODY_SIM_PRO_HAVE_OPENMP)
    endif()
endif()

if(N_BODY_SIM_PRO_ENABLE_MPI)
    find_package(MPI QUIET COMPONENTS C)
    if(MPI_C_FOUND)
        target_compile_definitions(${N_BODY_SIM_PRO_C_LIBRARY} PUBLIC N_BODY_SIM_PRO_HAVE_MPI)
        target_include_directories(${N_BODY_SIM_PRO_C_LIBRARY} PUBLIC ${MPI_C_INCLUDE_DIRS})
        if(MPI_C_LINK_FLAGS)
            target_link_libraries(${N_BODY_SIM_PRO_C_LIBRARY} PUBLIC ${MPI_C_LINK_FLAGS})
        else()
            target_link_libraries(${N_BODY_SIM_PRO_C_LIBRARY} PUBLIC ${MPI_C_LIBRARIES})
        endif()
        set(N_BODY_SIM_PRO_MPI_ENABLED ON CACHE INTERNAL "MPI available")
    else()
        # MSYS2 ships MS-MPI as an import library without mpicc discovery;
        # link it directly as a fallback.
        find_library(N_BODY_SIM_PRO_MSMPI_LIBRARY msmpi)
        if(N_BODY_SIM_PRO_MSMPI_LIBRARY)
            target_compile_definitions(${N_BODY_SIM_PRO_C_LIBRARY} PUBLIC N_BODY_SIM_PRO_HAVE_MPI)
            target_link_libraries(${N_BODY_SIM_PRO_C_LIBRARY} PUBLIC ${N_BODY_SIM_PRO_MSMPI_LIBRARY})
            set(N_BODY_SIM_PRO_MPI_ENABLED ON CACHE INTERNAL "MPI available")
        endif()
    endif()
endif()

# ---------------------------------------------------------------------------
# Windows runtime DLLs. All executables share one output directory
# (CMAKE_RUNTIME_OUTPUT_DIRECTORY); copy the toolchain runtime, SDL3, and
# MS-MPI beside them so the binaries run regardless of PATH (IDE launches,
# fresh shells, mpiexec child processes).
# ---------------------------------------------------------------------------
if(WIN32)
    get_filename_component(N_BODY_SIM_PRO_TOOLCHAIN_BIN ${CMAKE_C_COMPILER} DIRECTORY)
    foreach(_dll IN ITEMS
            libgcc_s_seh-1.dll
            libstdc++-6.dll
            libwinpthread-1.dll
            libgomp-1.dll
            libiconv-2.dll)
        if(EXISTS ${N_BODY_SIM_PRO_TOOLCHAIN_BIN}/${_dll})
            add_custom_command(TARGET ${N_BODY_SIM_PRO_C_LIBRARY} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${N_BODY_SIM_PRO_TOOLCHAIN_BIN}/${_dll}
                    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${_dll})
        endif()
    endforeach()
    add_custom_command(TARGET ${N_BODY_SIM_PRO_C_LIBRARY} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/SDL3.dll)
    if(EXISTS "$ENV{LOCALAPPDATA}/Microsoft MPI/x64/msmpi.dll")
        add_custom_command(TARGET ${N_BODY_SIM_PRO_C_LIBRARY} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$ENV{LOCALAPPDATA}/Microsoft MPI/x64/msmpi.dll"
                ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/msmpi.dll)
    endif()
endif()
