#include "ui/UserInterface.hpp"

#include <imgui.h>

namespace hpcsim::ui {

int UserInterface::draw(SimulationController& simulation, rendering::Camera& camera,
                        float frame_duration_ms, float frame_rate) {
    int steps_to_run = 0;
    if (simulation.running) {
        steps_to_run = static_cast<int>(steps_per_frame_);
    }

    if (ImGui::Begin("HPCSim", nullptr,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
        draw_simulation_panel(simulation);
        draw_camera_panel(camera);
        draw_performance_panel(frame_duration_ms, frame_rate);
    }
    ImGui::End();

    if (ImGui::Begin("Orbits", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Checkbox("Show trails", &show_trails);
        const auto& trails = simulation.trails();
        for (std::size_t body = 0; body < trails.size(); ++body) {
            ImGui::Text("Body %zu samples: %zu", body, trails[body].size());
        }
    }
    ImGui::End();

    return steps_to_run;
}

void UserInterface::draw_simulation_panel(SimulationController& simulation) {
    if (ImGui::CollapsingHeader("Simulation", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Particles    : %zu", simulation.particle_system().particle_count());
        ImGui::Text("Simulation t : %.4f", simulation.simulation_time);

        if (ImGui::Button(simulation.running ? "Pause" : "Play")) {
            simulation.running = !simulation.running;
        }
        ImGui::SameLine();
        if (ImGui::Button("Step")) {
            simulation.step();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            simulation.initialize_two_body();
        }

        const char* integrator_names[] = {"Euler", "Leapfrog", "Velocity Verlet"};
        int integrator_index = static_cast<int>(simulation.integrator);
        if (ImGui::Combo("Integrator", &integrator_index, integrator_names, 3)) {
            simulation.integrator = static_cast<HpcsimIntegratorType>(integrator_index);
        }

        ImGui::SliderFloat("Steps / frame", &steps_per_frame_, 0.0f, 200.0f, "%.1f");
        float timestep = static_cast<float>(simulation.timestep);
        if (ImGui::SliderFloat("Timestep", &timestep, 1.0e-4f, 0.05f, "%.4f")) {
            simulation.timestep = timestep;
        }
        float gravitational_constant =
            static_cast<float>(simulation.gravitational_constant());
        if (ImGui::SliderFloat("G", &gravitational_constant, 0.1f, 10.0f, "%.2f")) {
            simulation.set_gravitational_constant(gravitational_constant);
        }
    }
}

void UserInterface::draw_performance_panel(float frame_duration_ms, float frame_rate) {
    if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Frame time : %.2f ms", frame_duration_ms);
        ImGui::Text("Frame rate : %.1f FPS", frame_rate);
    }
}

void UserInterface::draw_camera_panel(rendering::Camera& camera) {
    if (ImGui::CollapsingHeader("Camera")) {
        const rendering::Vec3 target = camera.target();
        ImGui::Text("Target   : (%.2f, %.2f, %.2f)", target.x, target.y, target.z);
        if (ImGui::Button("Reset view")) {
            camera.reset();
        }
    }
}

}  // namespace hpcsim::ui
