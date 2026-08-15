add_subdirectory(c)
add_subdirectory(cpp)

if(N_BODY_SIM_PRO_BUILD_APPLICATION)
    add_executable(${N_BODY_SIM_PRO_EXECUTABLE} main.cpp)
    target_link_libraries(${N_BODY_SIM_PRO_EXECUTABLE}
        PRIVATE
            ${N_BODY_SIM_PRO_CPP_LIBRARY}
            ${N_BODY_SIM_PRO_C_LIBRARY})
    set_target_properties(${N_BODY_SIM_PRO_EXECUTABLE} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON)
    n_body_sim_pro_apply_warnings(${N_BODY_SIM_PRO_EXECUTABLE})
endif()
