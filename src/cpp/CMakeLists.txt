# C++20 application and orchestration layer.

set(N_BODY_SIM_PRO_DEAR_IMGUI_SOURCE_DIR ${dear_imgui_SOURCE_DIR})

# ---------------------------------------------------------------------------
# Dear ImGui (vendored by FetchContent) with SDL3 + OpenGL3 backends.
# ---------------------------------------------------------------------------
add_library(dear_imgui STATIC
    ${N_BODY_SIM_PRO_DEAR_IMGUI_SOURCE_DIR}/imgui.cpp
    ${N_BODY_SIM_PRO_DEAR_IMGUI_SOURCE_DIR}/imgui_draw.cpp
    ${N_BODY_SIM_PRO_DEAR_IMGUI_SOURCE_DIR}/imgui_tables.cpp
    ${N_BODY_SIM_PRO_DEAR_IMGUI_SOURCE_DIR}/imgui_widgets.cpp
    ${N_BODY_SIM_PRO_DEAR_IMGUI_SOURCE_DIR}/imgui_demo.cpp
    ${N_BODY_SIM_PRO_DEAR_IMGUI_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${N_BODY_SIM_PRO_DEAR_IMGUI_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp)

target_include_directories(dear_imgui PUBLIC
    ${N_BODY_SIM_PRO_DEAR_IMGUI_SOURCE_DIR}
    ${N_BODY_SIM_PRO_DEAR_IMGUI_SOURCE_DIR}/backends)

target_compile_definitions(dear_imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLEW)

find_package(GLEW REQUIRED)
find_package(SDL3 3.0 REQUIRED)

if(WIN32)
    set(N_BODY_SIM_PRO_OPENGL_LIBRARY opengl32)
else()
    set(N_BODY_SIM_PRO_OPENGL_LIBRARY ${CMAKE_DL_LIBS})
endif()

target_link_libraries(dear_imgui PUBLIC
    SDL3::SDL3
    GLEW::glew_s
    ${N_BODY_SIM_PRO_OPENGL_LIBRARY})

# ---------------------------------------------------------------------------
# N-Body Sim Pro C++ application layer.
# ---------------------------------------------------------------------------
add_library(${N_BODY_SIM_PRO_CPP_LIBRARY} STATIC
    application/AppIcon.cpp
    application/Application.cpp
    benchmark/HeadlessRunner.cpp
    logging/Logger.cpp
    rendering/Camera.cpp
    rendering/Renderer.cpp
    simulation/ParticleSystem.cpp
    simulation/SimulationController.cpp
    ui/UserInterface.cpp)

target_include_directories(${N_BODY_SIM_PRO_CPP_LIBRARY} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${N_BODY_SIM_PRO_CPP_LIBRARY} PUBLIC
    ${N_BODY_SIM_PRO_C_LIBRARY}
    dear_imgui)

target_compile_definitions(${N_BODY_SIM_PRO_CPP_LIBRARY} PUBLIC GLEW_STATIC)

if(N_BODY_SIM_PRO_ENABLE_DEVELOPER_MODE)
    target_compile_definitions(${N_BODY_SIM_PRO_CPP_LIBRARY} PUBLIC N_BODY_SIM_PRO_DEVELOPER_MODE)
endif()

target_compile_features(${N_BODY_SIM_PRO_CPP_LIBRARY} PUBLIC cxx_std_20)

n_body_sim_pro_apply_warnings(${N_BODY_SIM_PRO_CPP_LIBRARY})
