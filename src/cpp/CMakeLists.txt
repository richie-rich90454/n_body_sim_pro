# C++20 application and orchestration layer.

set(HPCSIM_DEAR_IMGUI_SOURCE_DIR ${dear_imgui_SOURCE_DIR})

# ---------------------------------------------------------------------------
# Dear ImGui (vendored by FetchContent) with SDL3 + OpenGL3 backends.
# ---------------------------------------------------------------------------
add_library(dear_imgui STATIC
    ${HPCSIM_DEAR_IMGUI_SOURCE_DIR}/imgui.cpp
    ${HPCSIM_DEAR_IMGUI_SOURCE_DIR}/imgui_draw.cpp
    ${HPCSIM_DEAR_IMGUI_SOURCE_DIR}/imgui_tables.cpp
    ${HPCSIM_DEAR_IMGUI_SOURCE_DIR}/imgui_widgets.cpp
    ${HPCSIM_DEAR_IMGUI_SOURCE_DIR}/imgui_demo.cpp
    ${HPCSIM_DEAR_IMGUI_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${HPCSIM_DEAR_IMGUI_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp)

target_include_directories(dear_imgui PUBLIC
    ${HPCSIM_DEAR_IMGUI_SOURCE_DIR}
    ${HPCSIM_DEAR_IMGUI_SOURCE_DIR}/backends)

target_compile_definitions(dear_imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLEW)

find_package(GLEW REQUIRED)
find_package(SDL3 3.0 REQUIRED)

if(WIN32)
    set(HPCSIM_OPENGL_LIBRARY opengl32)
else()
    set(HPCSIM_OPENGL_LIBRARY ${CMAKE_DL_LIBS})
endif()

target_link_libraries(dear_imgui PUBLIC
    SDL3::SDL3
    GLEW::glew_s
    ${HPCSIM_OPENGL_LIBRARY})

# ---------------------------------------------------------------------------
# HPCSim C++ application layer.
# ---------------------------------------------------------------------------
add_library(${HPCSIM_CPP_LIBRARY} STATIC
    application/Application.cpp
    rendering/Camera.cpp
    rendering/Renderer.cpp
    simulation/ParticleSystem.cpp
    simulation/SimulationController.cpp
    ui/UserInterface.cpp)

target_include_directories(${HPCSIM_CPP_LIBRARY} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${HPCSIM_CPP_LIBRARY} PUBLIC
    ${HPCSIM_C_LIBRARY}
    dear_imgui)

target_compile_definitions(${HPCSIM_CPP_LIBRARY} PUBLIC GLEW_STATIC)

target_compile_features(${HPCSIM_CPP_LIBRARY} PUBLIC cxx_std_20)

hpcsim_apply_warnings(${HPCSIM_CPP_LIBRARY})
