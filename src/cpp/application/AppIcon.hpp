#pragma once

struct SDL_Window;

namespace n_body_sim_pro::application {

/*
 * Applies the N-Body Sim Pro brand icon to an SDL3 window. The icon is drawn
 * procedurally (no asset files at runtime) to exactly match the favicon
 * artwork: a dark rounded tile, a tilted orbital ellipse, and three bodies.
 */
void apply_window_icon(SDL_Window* window);

}  // namespace n_body_sim_pro::application
