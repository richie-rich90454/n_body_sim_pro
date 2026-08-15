# Test suite. Each test is its own executable registered with CTest.
# Tests link against the C engine only; the GUI is never required.

set(HPCSIM_TESTS
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

foreach(test_target IN LISTS HPCSIM_TESTS)
    get_filename_component(test_name ${test_target} NAME)
    add_executable(${test_name} ${test_target}.c)
    target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${test_name} PRIVATE ${HPCSIM_C_LIBRARY})
    if(NOT WIN32)
        target_link_libraries(${test_name} PRIVATE m)
    endif()
    add_test(NAME ${test_name} COMMAND ${test_name})
    hpcsim_apply_warnings(${test_name})
endforeach()

# Distributed test: only when the engine was built with MPI.
if(HPCSIM_MPI_ENABLED)
    find_program(HPCSIM_MPIEXEC mpiexec
        HINTS "$ENV{USERPROFILE}/AppData/Local/Programs/Microsoft MPI/Bin")
    if(HPCSIM_MPIEXEC)
        add_executable(distributed_barnes_hut_test
            integration/distributed_barnes_hut_test.c)
        target_include_directories(distributed_barnes_hut_test PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR})
        target_link_libraries(distributed_barnes_hut_test PRIVATE ${HPCSIM_C_LIBRARY})
        hpcsim_apply_warnings(distributed_barnes_hut_test)
        add_test(NAME distributed_barnes_hut_test
            COMMAND ${HPCSIM_MPIEXEC} -n 2 $<TARGET_FILE:distributed_barnes_hut_test>)
        set_tests_properties(distributed_barnes_hut_test PROPERTIES
            ENVIRONMENT "PATH=C:/msys64/ucrt64/bin;$ENV{PATH}")
        # On this Windows host the MS-MPI runtime DLL is not in a system
        # directory; place the x64 runtime next to the test binary so the
        # MPI child processes can load it.
        if(WIN32 AND EXISTS "$ENV{USERPROFILE}/AppData/Local/Programs/Microsoft MPI/Bin/msmpi.dll")
            add_custom_command(TARGET distributed_barnes_hut_test POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$ENV{USERPROFILE}/AppData/Local/Programs/Microsoft MPI/Bin/msmpi.dll"
                    $<TARGET_FILE_DIR:distributed_barnes_hut_test>/msmpi.dll)
        endif()
    endif()
endif()
