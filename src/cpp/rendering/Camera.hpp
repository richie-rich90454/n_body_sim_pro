#pragma once

#include "rendering/matrix.hpp"

namespace n_body_sim_pro::rendering {

/*
 * Orbit camera: the camera sits on a sphere around a target point, at a
 * distance, oriented by yaw (about the world-up axis) and pitch. Mouse input
 * rotates the view; scrolling changes the distance.
 */

class Camera final {
public:
    Camera();

    void set_viewport_size(int width, int height);

    /* Controls. `delta_x`/`delta_y` are in pixels of mouse drag. */
    void orbit(float delta_x, float delta_y);
    void pan(float delta_x, float delta_y);
    void zoom(float scroll_delta);
    void reset();

    void set_target(Vec3 target) { target_ = target; }
    Vec3 target() const { return target_; }

    Mat4 view_matrix() const;
    Mat4 projection_matrix() const;

    Vec3 eye_position() const;

private:
    Vec3 target_;
    float yaw_;
    float pitch_;
    float distance_;

    int viewport_width_;
    int viewport_height_;

    static constexpr float FIELD_OF_VIEW_Y_DEGREES = 45.0f;
    static constexpr float NEAR_PLANE = 0.01f;
    static constexpr float FAR_PLANE = 100000.0f;
};

}  // namespace n_body_sim_pro::rendering
