#include "ui/UserInterface.hpp"

#include "logging/Logger.hpp"

#include <imgui.h>

#include <cstddef>
#include <cstdint>

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
        draw_numerics_panel(simulation);
        draw_performance_panel(simulation, frame_duration_ms, frame_rate);
        draw_memory_panel();
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

    draw_developer_console();

    return steps_to_run;
}

void UserInterface::draw_simulation_panel(SimulationController& simulation) {
    if (!ImGui::CollapsingHeader("Simulation", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    ImGui::Text("Particles    : %zu", simulation.particle_system().particle_count());
    ImGui::Text("Simulation t : %.4f", simulation.simulation_time);
    ImGui::Text("Timestep     : %.4f", simulation.timestep);

    if (ImGui::Button(simulation.running ? "Pause" : "Play")) {
        simulation.running = !simulation.running;
    }
    ImGui::SameLine();
    if (ImGui::Button("Step")) {
        simulation.step();
    }

    static int preset_index = 0;
    const char* preset_names[] = {
        "Two Body",         "Random Cloud",   "Solar System",     "Open Cluster",
        "Globular Cluster", "Spiral Galaxy",  "Elliptical Galaxy", "Galaxy Collision",
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

    static int algorithm_index = 0;
    const char* algorithm_names[] = {"All-pairs (OpenMP)", "All-pairs (single thread)",
                                     "Barnes-Hut"};
    if (ImGui::Combo("Algorithm", &algorithm_index, algorithm_names, 3)) {
        simulation.barnes_hut_enabled = algorithm_index == 2;
        simulation.use_parallel_forces = algorithm_index != 1;
    }
    if (simulation.barnes_hut_enabled) {
        float theta = static_cast<float>(simulation.barnes_hut_theta);
        if (ImGui::SliderFloat("BH theta", &theta, 0.1f, 2.0f, "%.2f")) {
            simulation.barnes_hut_theta = theta;
        }
        HpcsimBarnesHutStats stats;
        if (simulation.barnes_hut_stats(&stats)) {
            ImGui::Text("Tree nodes    : %zu", stats.node_count);
            ImGui::Text("Leaves        : %zu", stats.leaf_count);
            ImGui::Text("Internal nodes: %zu", stats.internal_node_count);
            ImGui::Text("Max depth     : %zu", stats.maximum_depth);
            ImGui::Text("Approximations: %zu", stats.accepted_approximations);
            ImGui::Text("Exact interacts: %zu", stats.exact_interactions);
        }
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
    float gravitational_constant = static_cast<float>(simulation.gravitational_constant());
    if (ImGui::SliderFloat("G", &gravitational_constant, 0.1f, 10.0f, "%.2f")) {
        simulation.set_gravitational_constant(gravitational_constant);
    }

    ImGui::Separator();

    if (hpcsim_threading_openmp_available()) {
        static int thread_count_index = 0;
        const int available_threads = hpcsim_threading_available_thread_count();
        const char* thread_labels[] = {"Auto", "1", "2", "4", "8", "16", "32", "64"};
        const int thread_values[] = {0, 1, 2, 4, 8, 16, 32, 64};
        if (ImGui::Combo("OpenMP threads", &thread_count_index, thread_labels,
                         static_cast<int>(sizeof(thread_labels) / sizeof(thread_labels[0])))) {
            hpcsim_threading_set_thread_count(thread_values[thread_count_index]);
        }
        ImGui::Text("Available threads : %d", available_threads);
    } else {
        ImGui::TextDisabled("OpenMP not available in this build");
    }
}

void UserInterface::draw_numerics_panel(SimulationController& simulation) {
    if (!ImGui::CollapsingHeader("Numerics", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    const auto& diagnostics = simulation.numerical_diagnostics();
    ImGui::Text("Kinetic energy : %.6e", diagnostics.kinetic_energy);
    ImGui::Text("Momentum error : %.6e", diagnostics.momentum_error);
    ImGui::Text("COM offset     : %.6e", diagnostics.center_of_mass_offset);
    if (diagnostics.energy_available) {
        ImGui::Text("Energy drift   : %.6e", diagnostics.energy_drift);
    } else {
        ImGui::Text("Energy drift   : N/A (N > %llu, O(N^2) potential)",
                    (unsigned long long)SimulationController::ENERGY_TRACK_MAX_PARTICLES);
    }
}

void UserInterface::draw_performance_panel(SimulationController& simulation,
                                           float frame_duration_ms, float frame_rate) {
    if (!ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    ImGui::Text("Frame time    : %.2f ms", frame_duration_ms);
    ImGui::Text("Frame rate    : %.1f FPS", frame_rate);
    ImGui::Text("Step          : %.3f ms", simulation.last_step_ms());
    if (simulation.barnes_hut_enabled) {
        ImGui::Text("Tree build    : %.3f ms", simulation.last_tree_build_ms());
        ImGui::Text("Force eval    : %.3f ms", simulation.last_force_evaluation_ms());
    } else {
        ImGui::Text("Force (step)  : %.3f ms", simulation.last_force_evaluation_ms());
    }
    const HpcsimCpuFeatures cpu_features = hpcsim_cpu_detect_features();
    ImGui::Text("SIMD backend  : %s",
                hpcsim_simd_backend_string(simulation.simd_backend()));
    ImGui::Text("CPU           : %s", hpcsim_cpu_brand_string());
    ImGui::Text("AVX2          : %s",
                cpu_features.has_avx2 ? "available" : "unavailable");
    ImGui::Text("AVX-512       : %s",
                cpu_features.has_avx512_foundation ? "available" : "unavailable");
    ImGui::Text("NEON          : %s",
                cpu_features.has_neon ? "available" : "unavailable");
}

void UserInterface::draw_memory_panel() {
    if (!ImGui::CollapsingHeader("Memory")) {
        return;
    }
    HpcsimAllocationSummary summary;
    if (hpcsim_allocation_tracker_poll(&summary) != 0) {
        ImGui::TextDisabled("Allocation tracking unavailable");
        return;
    }
    ImGui::Text("Live allocations : %zu", summary.live_allocations);
    ImGui::Text("Total allocations: %zu", summary.total_allocations);
    ImGui::Text("Total frees      : %zu", summary.total_deallocations);
    ImGui::Text("Live memory      : %.2f MiB",
                (double)summary.live_bytes / (1024.0 * 1024.0));
    ImGui::Text("Peak memory      : %.2f MiB",
                (double)summary.peak_bytes / (1024.0 * 1024.0));
    ImGui::Text("Allocation rate  : %.1f /s", summary.allocation_rate_per_second);
    ImGui::Text("Deallocation rate: %.1f /s", summary.deallocation_rate_per_second);

    const char* category_names[] = {"Particles", "Octree", "Thread workspaces",
                                    "Temporary", "Checkpoint", "Renderer", "UI",
                                    "Other"};
    if (ImGui::TreeNode("By category")) {
        for (int category = 0; category < HPCSIM_ALLOCATION_CATEGORY_COUNT; ++category) {
            ImGui::Text("%-18s: %6zu allocs, %10.2f MiB", category_names[category],
                        summary.live_allocations_by_category[category],
                        (double)summary.live_bytes_by_category[category] / (1024.0 * 1024.0));
        }
        ImGui::TreePop();
    }
}

void UserInterface::draw_developer_console() {
    if (!ImGui::Begin("Developer Console", nullptr,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    static bool auto_scroll = true;
    ImGui::Checkbox("Auto-scroll", &auto_scroll);
    ImGui::SameLine();
    static int filter_level = 0;
    ImGui::SetNextItemWidth(160.0f);
    ImGui::Combo("Min level", &filter_level, "ERROR\0WARN\0INFO\0DEBUG\0TRACE\0PERF\0INST\0",
                 7);
    ImGui::Separator();

    std::vector<logging::Record> records;
    logging::Logger::instance().copy_records(records);
    ImGui::BeginChild("console_scroll", ImVec2(0, 0), ImGuiChildFlags_Borders);
    for (const auto& record : records) {
        const int record_level = static_cast<int>(record.level);
        if (record_level < filter_level) {
            continue;
        }
        ImU32 color = ImGui::GetColorU32(ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        switch (record.level) {
            case logging::Level::Error:
                color = ImGui::GetColorU32(ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                break;
            case logging::Level::Warning:
                color = ImGui::GetColorU32(ImVec4(1.0f, 0.8f, 0.3f, 1.0f));
                break;
            case logging::Level::Performance:
                color = ImGui::GetColorU32(ImVec4(0.4f, 0.9f, 0.6f, 1.0f));
                break;
            case logging::Level::Instrumentation:
                color = ImGui::GetColorU32(ImVec4(0.5f, 0.7f, 1.0f, 1.0f));
                break;
            default:
                break;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::Text("[%07.3f] [%s] [%s] %s", record.timestamp_seconds,
                    logging::level_name(record.level),
                    logging::category_name(record.category), record.message.c_str());
        ImGui::PopStyleColor();
    }
    if (auto_scroll) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    ImGui::End();
}

}  // namespace hpcsim::ui
