#pragma once

#include "rendering/matrix.hpp"

#include <n_body_sim_pro/n_body_sim_pro.h>

#include <GL/glew.h>

#include <cstddef>
#include <vector>

namespace n_body_sim_pro::rendering {

/*
 * OpenGL renderer for particle positions.
 *
 * Physics is CPU-only; the renderer only visualizes a snapshot of the
 * simulation state. Particles are drawn as GL_POINTS from a single
 * interleaved position buffer that is re-uploaded each frame. Optional line
 * strips draw trajectories and debug overlays.
 */

class Renderer final {
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void upload_particles(const NBodySimProParticleSystemView& view);
    void draw_particles(const Mat4& view_matrix, const Mat4& projection_matrix) const;

    void draw_line_strip(const std::vector<Vec3>& points, Vec3 color, const Mat4& view_matrix,
                         const Mat4& projection_matrix) const;

    /* Clears the framebuffer to a dark technical background. */
    void clear() const;

    void set_particle_point_size(float size) { point_size_ = size; }
    float particle_point_size() const { return point_size_; }

    std::size_t uploaded_particle_count() const { return uploaded_particle_count_; }

private:
    struct ShaderProgram {
        GLuint id;
        GLint view_projection_location;
    };
    static ShaderProgram compile_program(const char* vertex_source,
                                         const char* fragment_source);
    static GLuint compile_shader(GLenum type, const char* source, const char* name);

    ShaderProgram* point_program_;
    ShaderProgram* line_program_;

    GLuint point_vao_;
    GLuint point_vbo_;

    GLuint line_vao_;
    GLuint line_vbo_;

    float point_size_;
    std::size_t uploaded_particle_count_;
};

}  // namespace n_body_sim_pro::rendering
