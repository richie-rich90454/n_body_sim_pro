#include "ui/UserInterface.hpp"

#include <imgui.h>

#include <cstddef>

namespace hpcsim::ui {

namespace {
constexpr std::size_t PARTICLE_COUNT_OPTIONS = 5;
const std::size_t PARTICLE_COUNTS[PARTICLE_COUNT_OPTIONS] = {1024, 4096, 16384, 65536,
                                                             262144};
}

int UserInterface::draw(SimulationController& simulation, rendering::Camera& camera,
                        float frame_duration_ms, float frame_rate) {
    int steps_to_run = 0;
    if (simulation.running) {
        steps_to_run = static_cast<int>(steps_per_frame_);
    }

    if (ImGui::Begin("HPCSim", nullptr,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
        draw_simulation_panel(simulation);
        if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Frame time : %.2f ms", frame_duration_ms);
            ImGui::Text("Frame rate : %.1f FPS", frame_rate);
        }
        if (ImGui::CollapsingHeader("Camera")) {
            const rendering::Vec3 target = camera.target();
            ImGui::Text("Target   : (%.2f, %.2f, %.2f)", target.x, target.y, target.z);
            if (ImGui::Button("Reset view")) {
                camera.reset();
            }
        }
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
    if (!ImGui::CollapsingHeader("Simulation", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    ImGui::Text("Particles    : %zu", simulation.particle_system().particle_count());
    ImGui::Text("Simulation t : %.4f", simulation.simulation_time);

    if (ImGui::Button(simulation.running ? "Pause" : "Play")) {
        simulation.running = !simulation.running;
    }
    ImGui::SameLine();
    if (ImGui::Button("Step")) {
        simulation.step();
    }

    static int preset_index = 0;
    const char* preset_names[] = {
        "Two Body",       "Random Cloud",   "Solar System", "Open Cluster",
        "Globular Cluster", "Spiral Galaxy", "Elliptical Galaxy", "Galaxy Collision",
        "Triple Galaxy"};
    static const HpcsimSimulationPreset preset_values[] = {
        HPCSIM_PRESET_TWO_BODY,      HPCSIM_PRESET_RANDOM_CLOUD,
        HPCSIM_PRESET_SOLAR_SYSTEM,  HPCSIM_PRESET_OPEN_CLUSTER,
        HPCSIM_PRESET_GLOBULAR_CLUSTER, HPCSIM_PRESET_SPIRAL_GALAXY,
        HPCSIM_PRESET_ELLIPTICAL_GALAXY, HPCSIM_PRESET_GALAXY_COLLISION,
        HPCSIM_PRESET_TRIPLE_GALAXY};
    constexpr int preset_count = 9;

    static int particle_count_index = 1;
    static int random_seed = 1;
    const char* particle_count_names[PARTICLE_COUNT_OPTIONS] = {
        "1,024", "4,096", "16,384", "65,536", "262,144"};

    if (ImGui::Combo("Preset", &preset_index, preset_names, preset_count)) {
        if (preset_values[preset_index] == HPCSIM_PRESET_TWO_BODY) {
            particle_count_index = 0;
        }
    }
    ImGui::Combo("Particles", &particle_count_index, particle_count_names,
                 static_cast<int>(PARTICLE_COUNT_OPTIONS));
    ImGui::Text("Particles: %zu", PARTICLE_COUNTS[particle_count_index]);
    ImGui::InputInt("Seed", &random_seed);
    if (ImGui::Button("Regenerate")) {
        simulation.apply_preset(preset_values[preset_index],
                                PARTICLE_COUNTS[particle_count_index],
                                static_cast<std::uint32_t>(random_seed));
        preset_index = 0;
        for (int i = 0; i < preset_count; ++i) {
            if (preset_values[i] == simulation.preset()) {
                preset_index = i;
                break;
            }
        }
    }

    ImGui::Separator();

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
    float gravitational_constant = static_cast<float>(simulation.gravitational_constant());
    if (ImGui::SliderFloat("G", &gravitational_constant, 0.1f, 10.0f, "%.2f")) {
        simulation.set_gravitational_constant(gravitational_constant);
    }
}

}  // namespace hpcsim::ui
