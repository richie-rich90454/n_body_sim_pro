#pragma once

#include <cmath>

namespace n_body_sim_pro::rendering {

/*
 * Minimal column-major 4x4 matrix helpers for camera and projection math.
 *
 * Matrices are stored column-major exactly as OpenGL expects them, and are
 * float precision (the renderer consumes them directly).
 */

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Mat4 {
    float m[16];
};

inline Mat4 mat4_identity() {
    Mat4 result{};
    result.m[0] = 1.0f;
    result.m[5] = 1.0f;
    result.m[10] = 1.0f;
    result.m[15] = 1.0f;
    return result;
}

inline Vec3 vec3_sub(Vec3 a, Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 vec3_cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline Vec3 vec3_normalize(Vec3 v) {
    const float length_squared = v.x * v.x + v.y * v.y + v.z * v.z;
    if (length_squared == 0.0f) {
        return {0.0f, 0.0f, 0.0f};
    }
    const float inverse_length = 1.0f / std::sqrt(length_squared);
    return {v.x * inverse_length, v.y * inverse_length, v.z * inverse_length};
}

inline float vec3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Mat4 mat4_multiply(const Mat4& left, const Mat4& right) {
    Mat4 result{};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += left.m[k * 4 + row] * right.m[column * 4 + k];
            }
            result.m[column * 4 + row] = sum;
        }
    }
    return result;
}

inline Mat4 mat4_perspective(float field_of_view_y_radians, float aspect_ratio,
                             float near_plane, float far_plane) {
    Mat4 result{};
    const float f = 1.0f / std::tan(field_of_view_y_radians * 0.5f);
    result.m[0] = f / aspect_ratio;
    result.m[5] = f;
    result.m[10] = (far_plane + near_plane) / (near_plane - far_plane);
    result.m[11] = -1.0f;
    result.m[14] = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
    return result;
}

inline Mat4 mat4_look_at(Vec3 eye, Vec3 center, Vec3 up) {
    const Vec3 forward = vec3_normalize(vec3_sub(center, eye));
    const Vec3 side = vec3_normalize(vec3_cross(forward, up));
    const Vec3 camera_up = vec3_cross(side, forward);

    Mat4 result{};
    result.m[0] = side.x;
    result.m[1] = camera_up.x;
    result.m[2] = -forward.x;
    result.m[3] = 0.0f;
    result.m[4] = side.y;
    result.m[5] = camera_up.y;
    result.m[6] = -forward.y;
    result.m[7] = 0.0f;
    result.m[8] = side.z;
    result.m[9] = camera_up.z;
    result.m[10] = -forward.z;
    result.m[11] = 0.0f;
    result.m[12] = -vec3_dot(side, eye);
    result.m[13] = -vec3_dot(camera_up, eye);
    result.m[14] = vec3_dot(forward, eye);
    result.m[15] = 1.0f;
    return result;
}

}  // namespace n_body_sim_pro::rendering
