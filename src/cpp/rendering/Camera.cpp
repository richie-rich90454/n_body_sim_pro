#include "rendering/Camera.hpp"

#include <algorithm>
#include <numbers>

namespace hpcsim::rendering {

Camera::Camera() : target_{0.0f, 0.0f, 0.0f},
                   yaw_(0.0f),
                   pitch_(0.3f),
                   distance_(4.0f),
                   viewport_width_(1),
                   viewport_height_(1) {}

void Camera::set_viewport_size(int width, int height) {
    viewport_width_ = width > 0 ? width : 1;
    viewport_height_ = height > 0 ? height : 1;
}

void Camera::orbit(float delta_x, float delta_y) {
    constexpr float rotation_speed = 0.005f;
    yaw_ -= delta_x * rotation_speed;
    pitch_ -= delta_y * rotation_speed;
    pitch_ = std::clamp(pitch_, -1.55f, 1.55f);
}

void Camera::pan(float delta_x, float delta_y) {
    const Vec3 forward = vec3_normalize(vec3_sub(target_, eye_position()));
    const Vec3 side = vec3_normalize(vec3_cross(forward, Vec3{0.0f, 1.0f, 0.0f}));
    const Vec3 up = vec3_cross(side, forward);
    const float scale = 0.002f * distance_;

    target_.x += (-side.x * delta_x + up.x * delta_y) * scale;
    target_.y += (-side.y * delta_x + up.y * delta_y) * scale;
    target_.z += (-side.z * delta_x + up.z * delta_y) * scale;
}

void Camera::zoom(float scroll_delta) {
    constexpr float zoom_factor = 0.92f;
    if (scroll_delta > 0.0f) {
        distance_ *= zoom_factor;
    } else if (scroll_delta < 0.0f) {
        distance_ /= zoom_factor;
    }
    distance_ = std::clamp(distance_, 0.01f, 1.0e6f);
}

void Camera::reset() {
    yaw_ = 0.0f;
    pitch_ = 0.3f;
    distance_ = 4.0f;
    target_ = {0.0f, 0.0f, 0.0f};
}

Vec3 Camera::eye_position() const {
    const float horizontal = std::cos(pitch_);
    return {target_.x + distance_ * horizontal * std::sin(yaw_),
            target_.y + distance_ * std::sin(pitch_),
            target_.z + distance_ * horizontal * std::cos(yaw_)};
}

Mat4 Camera::view_matrix() const {
    return mat4_look_at(eye_position(), target_, Vec3{0.0f, 1.0f, 0.0f});
}

Mat4 Camera::projection_matrix() const {
    const float aspect =
        static_cast<float>(viewport_width_) / static_cast<float>(viewport_height_);
    const float field_of_view_y_radians =
        FIELD_OF_VIEW_Y_DEGREES * std::numbers::pi_v<float> / 180.0f;
    return mat4_perspective(field_of_view_y_radians, aspect, NEAR_PLANE, FAR_PLANE);
}

}  // namespace hpcsim::rendering
