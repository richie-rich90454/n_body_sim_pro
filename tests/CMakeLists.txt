# Test suite. Each test is its own executable registered with CTest.
# Tests link against the C engine only; the GUI is never required.

set(N_BODY_SIM_PRO_TESTS
    unit/allocator_test
    unit/particle_system_test
    numerical/gravity_test
    numerical/diagnostics_test
    numerical/openmp_equivalence_test
    numerical/simd_equivalence_test
    numerical/barnes_hut_test
    generation/random_test
    generation/presets_test
    threading/numa_test
    regression/two_body_test
    regression/checkpoint_test)

foreach(test_target IN LISTS N_BODY_SIM_PRO_TESTS)
    get_filename_component(test_name ${test_target} NAME)
    add_executable(${test_name} ${test_target}.c)
    target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${test_name} PRIVATE ${N_BODY_SIM_PRO_C_LIBRARY})
    if(NOT WIN32)
        target_link_libraries(${test_name} PRIVATE m)
    endif()
    add_test(NAME ${test_name} COMMAND ${test_name})
    n_body_sim_pro_apply_warnings(${test_name})
endforeach()

# Distributed test: only when the engine was built with MPI.
if(N_BODY_SIM_PRO_MPI_ENABLED)
    find_program(N_BODY_SIM_PRO_MPIEXEC mpiexec
        HINTS "$ENV{USERPROFILE}/AppData/Local/Programs/Microsoft MPI/Bin")
    if(N_BODY_SIM_PRO_MPIEXEC)
        add_executable(distributed_barnes_hut_test
            integration/distributed_barnes_hut_test.c)
        target_include_directories(distributed_barnes_hut_test PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR})
        target_link_libraries(distributed_barnes_hut_test PRIVATE ${N_BODY_SIM_PRO_C_LIBRARY})
        n_body_sim_pro_apply_warnings(distributed_barnes_hut_test)
        add_test(NAME distributed_barnes_hut_test
            COMMAND ${N_BODY_SIM_PRO_MPIEXEC} -n 2 $<TARGET_FILE:distributed_barnes_hut_test>)
        set_tests_properties(distributed_barnes_hut_test PROPERTIES
            ENVIRONMENT "PATH=C:/msys64/ucrt64/bin;$ENV{PATH}")
        # On this Windows host the MS-MPI runtime DLL is not in a system
        # directory; place the x64 runtime next to the test binary so the
        # MPI child processes can load it. The tools directory holds the x86
        # runtime (for the x86 mpiexec/smpd), so an x64 copy is kept apart.
        if(WIN32 AND EXISTS "$ENV{LOCALAPPDATA}/Microsoft MPI/x64/msmpi.dll")
            add_custom_command(TARGET distributed_barnes_hut_test POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$ENV{LOCALAPPDATA}/Microsoft MPI/x64/msmpi.dll"
                    $<TARGET_FILE_DIR:distributed_barnes_hut_test>/msmpi.dll)
        endif()
    endif()
endif()
