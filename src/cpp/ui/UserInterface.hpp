#pragma once

#include "rendering/Camera.hpp"
#include "simulation/SimulationController.hpp"

namespace n_body_sim_pro::ui {

/*
 * Dear ImGui panels for the technical interface.
 *
 * The UI only reads and drives the simulation controller and camera; it
 * never touches physics state directly.
 */

class UserInterface final {
public:
    /* Draw all panels; `frame_duration_ms` and `frame_rate` come from the
     * application loop. Returns the number of simulation steps the frame
     * should run (respecting time scale). */
    int draw(SimulationController& simulation, rendering::Camera& camera,
             float frame_duration_ms, float frame_rate);

    bool show_trails = true;

private:
    void draw_simulation_panel(SimulationController& simulation);
    void draw_numerics_panel(SimulationController& simulation);
    void draw_performance_panel(SimulationController& simulation, float frame_duration_ms,
                                float frame_rate);
    void draw_memory_panel();
    void draw_developer_console();

    float steps_per_frame_ = 1.0f;
};

}  // namespace n_body_sim_pro::ui
