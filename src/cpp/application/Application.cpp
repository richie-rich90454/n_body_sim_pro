#include "application/Application.hpp"
#include "application/AppIcon.hpp"

#include "logging/Logger.hpp"

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include <GL/glew.h>

#include <cstdio>
#include <exception>

namespace n_body_sim_pro::application {

namespace {
constexpr int INITIAL_WINDOW_WIDTH = 1600;
constexpr int INITIAL_WINDOW_HEIGHT = 900;
const char* const WINDOW_TITLE = "N-Body Sim Pro - CPU N-Body Simulation Engine";
}

Application::Application()
    : window_(nullptr), gl_context_(nullptr) {}

Application::~Application() {
    shutdown();
}

int Application::run() {
    if (!initialize_sdl() || !initialize_opengl() || !initialize_imgui()) {
        std::fprintf(stderr, "N-Body Sim Pro: initialization failed, exiting.\n");
        return 1;
    }

    Uint64 last_frame_time = SDL_GetPerformanceCounter();
    const Uint64 performance_frequency = SDL_GetPerformanceFrequency();

    bool quit = false;
    while (!quit) {
        process_events();
        if (window_ == nullptr) {
            break;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        const int steps_to_run =
            user_interface_.draw(simulation_, camera_, frame_duration_ms_, frame_rate_);
        ImGui::Render();

        simulate(steps_to_run);
        render_frame();

        const Uint64 current_frame_time = SDL_GetPerformanceCounter();
        const Uint64 elapsed =
            current_frame_time > last_frame_time ? current_frame_time - last_frame_time : 1;
        last_frame_time = current_frame_time;

        const double frame_seconds =
            static_cast<double>(elapsed) / static_cast<double>(performance_frequency);
        frame_duration_ms_ = static_cast<float>(frame_seconds * 1000.0);
        frame_rate_ = frame_duration_ms_ > 0.0f ? 1000.0f / frame_duration_ms_ : 0.0f;
    }

    shutdown();
    return 0;
}

bool Application::initialize_sdl() {
#ifdef N_BODY_SIM_PRO_DEVELOPER_MODE
    n_body_sim_pro_allocation_tracker_set_enabled(1);
    N_BODY_SIM_PRO_LOG(logging::Level::Info, logging::Category::Memory,
               "Allocation tracking enabled (developer mode)");
#endif
    N_BODY_SIM_PRO_LOG(logging::Level::Info, logging::Category::Application,
               "Initializing SDL3 video subsystem");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "N-Body Sim Pro: SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifdef __APPLE__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif

    window_ = SDL_CreateWindow(WINDOW_TITLE, INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT,
                               SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                   SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window_ == nullptr) {
        std::fprintf(stderr, "N-Body Sim Pro: SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }
    apply_window_icon(window_);
    return true;
}

bool Application::initialize_opengl() {
    gl_context_ = SDL_GL_CreateContext(window_);
    if (gl_context_ == nullptr) {
        std::fprintf(stderr, "N-Body Sim Pro: SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_GL_MakeCurrent(window_, gl_context_);
    SDL_GL_SetSwapInterval(1);

    const GLenum glew_status = glewInit();
    if (glew_status != GLEW_OK) {
        std::fprintf(stderr, "N-Body Sim Pro: glewInit failed: %s\n",
                     reinterpret_cast<const char*>(glewGetErrorString(glew_status)));
        return false;
    }

    std::printf("N-Body Sim Pro: OpenGL %s, GLSL %s\n", glGetString(GL_VERSION),
                glGetString(GL_SHADING_LANGUAGE_VERSION));

    renderer_ = std::make_unique<rendering::Renderer>();
    return true;
}

bool Application::initialize_imgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.WindowPadding = ImVec2(10.0f, 8.0f);

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = "n_body_sim_pro_imgui.ini";
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    if (!ImGui_ImplSDL3_InitForOpenGL(window_, gl_context_)) {
        std::fprintf(stderr, "N-Body Sim Pro: ImGui SDL3 backend init failed\n");
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 330 core")) {
        std::fprintf(stderr, "N-Body Sim Pro: ImGui OpenGL3 backend init failed\n");
        return false;
    }
    return true;
}

void Application::process_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        switch (event.type) {
            case SDL_EVENT_QUIT:
                window_ = nullptr;
                return;
            case SDL_EVENT_WINDOW_RESIZED:
                camera_.set_viewport_size(event.window.data1, event.window.data2);
                glViewport(0, 0, event.window.data1, event.window.data2);
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                if (!ImGui::GetIO().WantCaptureMouse) {
                    camera_.zoom(event.wheel.y);
                }
                break;
            default:
                break;
        }
        handle_camera_input(event);
    }
}

void Application::handle_camera_input(SDL_Event& event) {
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }
    const bool left_dragging =
        event.type == SDL_EVENT_MOUSE_MOTION && (event.motion.state & SDL_BUTTON_LMASK);
    const bool middle_dragging =
        event.type == SDL_EVENT_MOUSE_MOTION && (event.motion.state & SDL_BUTTON_MMASK);
    if (left_dragging) {
        camera_.orbit(event.motion.xrel, event.motion.yrel);
    } else if (middle_dragging) {
        camera_.pan(event.motion.xrel, event.motion.yrel);
    }
}

void Application::simulate(int steps) {
    for (int step = 0; step < steps; ++step) {
        simulation_.step();
    }
}

void Application::render_frame() {
    renderer_->clear();

    const rendering::Mat4 view = camera_.view_matrix();
    const rendering::Mat4 projection = camera_.projection_matrix();

    renderer_->upload_particles(simulation_.particle_system().view());
    renderer_->draw_particles(view, projection);

    if (user_interface_.show_trails && simulation_.preset() == N_BODY_SIM_PRO_PRESET_TWO_BODY) {
        const auto& trails = simulation_.trails();
        renderer_->draw_line_strip(trails[0], {0.75f, 0.85f, 1.0f}, view, projection);
        renderer_->draw_line_strip(trails[1], {1.0f, 0.75f, 0.65f}, view, projection);
    }

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window_);
}

void Application::shutdown() {
    renderer_.reset();
    if (gl_context_ != nullptr) {
        SDL_GL_DestroyContext(gl_context_);
        gl_context_ = nullptr;
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

}  // namespace n_body_sim_pro::application
