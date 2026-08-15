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

    # Windows: embed the application icon into the executable.
    if(WIN32)
        set(N_BODY_SIM_PRO_ICON_PATH
            "${CMAKE_CURRENT_SOURCE_DIR}/../resources/n_body_sim_pro.ico")
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../resources/app_icon.rc.in
                       ${CMAKE_CURRENT_BINARY_DIR}/app_icon.rc @ONLY)
        target_sources(${N_BODY_SIM_PRO_EXECUTABLE} PRIVATE
            ${CMAKE_CURRENT_BINARY_DIR}/app_icon.rc)
    endif()

    # macOS: build as a .app bundle carrying the brand icon.
    if(APPLE)
        set_target_properties(${N_BODY_SIM_PRO_EXECUTABLE} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.nbodysimpro.app"
            MACOSX_BUNDLE_BUNDLE_NAME "N-Body Sim Pro"
            MACOSX_BUNDLE_EXECUTABLE_NAME "N-Body Sim Pro"
            MACOSX_BUNDLE_ICON_FILE "n_body_sim_pro.icns"
            RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../resources/n_body_sim_pro.icns")
    endif()
endif()
