add_subdirectory(c)
add_subdirectory(cpp)

if(HPCSIM_BUILD_APPLICATION)
    add_executable(${HPCSIM_EXECUTABLE} main.cpp)
    target_link_libraries(${HPCSIM_EXECUTABLE}
        PRIVATE
            ${HPCSIM_CPP_LIBRARY}
            ${HPCSIM_C_LIBRARY})
    set_target_properties(${HPCSIM_EXECUTABLE} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON)
    hpcsim_apply_warnings(${HPCSIM_EXECUTABLE})
endif()
