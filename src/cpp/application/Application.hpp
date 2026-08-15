#pragma once

#include "rendering/Camera.hpp"
#include "rendering/Renderer.hpp"
#include "simulation/SimulationController.hpp"
#include "ui/UserInterface.hpp"

#include <SDL3/SDL.h>

#include <memory>

namespace n_body_sim_pro::application {

/*
 * Application lifecycle: SDL3 window, OpenGL core context, Dear ImGui,
 * and the main loop that drives simulation, rendering, and the UI.
 *
 * Owns nothing of the physics engine; it only glues the layers together.
 */

class Application final {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int run();

private:
    bool initialize_sdl();
    bool initialize_opengl();
    bool initialize_imgui();
    void shutdown();

    void process_events();
    void handle_camera_input(SDL_Event& event);
    void simulate(int steps);
    void render_frame();

    SDL_Window* window_;
    SDL_GLContext gl_context_;

    SimulationController simulation_;
    rendering::Camera camera_;
    std::unique_ptr<rendering::Renderer> renderer_;
    ui::UserInterface user_interface_;

    float frame_duration_ms_ = 0.0f;
    float frame_rate_ = 0.0f;
};

}  // namespace n_body_sim_pro::application
