# Test suite. Each test is its own executable registered with CTest.
# Tests link against the C engine only; the GUI is never required.

set(HPCSIM_TESTS
    unit/allocator_test
    unit/particle_system_test
    numerical/gravity_test
    numerical/diagnostics_test
    generation/random_test
    generation/presets_test
    regression/two_body_test)

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
