#include "rendering/Renderer.hpp"

#include <hpcsim/hpcsim.h>

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace hpcsim::rendering {

static const char* const POINT_VERTEX_SHADER = R"GLSL(
#version 330 core
layout(location = 0) in vec3 particle_position;
uniform mat4 view_projection;
uniform float point_size;
void main() {
    gl_Position = view_projection * vec4(particle_position, 1.0);
    gl_PointSize = point_size;
}
)GLSL";

static const char* const POINT_FRAGMENT_SHADER = R"GLSL(
#version 330 core
uniform vec3 point_color;
out vec4 fragment_color;
void main() {
    fragment_color = vec4(point_color, 1.0);
}
)GLSL";

static const char* const LINE_VERTEX_SHADER = R"GLSL(
#version 330 core
layout(location = 0) in vec3 line_position;
uniform mat4 view_projection;
void main() {
    gl_Position = view_projection * vec4(line_position, 1.0);
}
)GLSL";

static const char* const LINE_FRAGMENT_SHADER = R"GLSL(
#version 330 core
uniform vec3 line_color;
out vec4 fragment_color;
void main() {
    fragment_color = vec4(line_color, 1.0);
}
)GLSL";

Renderer::Renderer()
    : point_program_(nullptr),
      line_program_(nullptr),
      point_vao_(0),
      point_vbo_(0),
      line_vao_(0),
      line_vbo_(0),
      point_size_(3.0f),
      uploaded_particle_count_(0) {
    point_program_ = new ShaderProgram(
        compile_program(POINT_VERTEX_SHADER, POINT_FRAGMENT_SHADER));
    line_program_ =
        new ShaderProgram(compile_program(LINE_VERTEX_SHADER, LINE_FRAGMENT_SHADER));

    glGenVertexArrays(1, &point_vao_);
    glGenBuffers(1, &point_vbo_);
    glBindVertexArray(point_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, point_vbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glGenVertexArrays(1, &line_vao_);
    glGenBuffers(1, &line_vbo_);
    glBindVertexArray(line_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, line_vbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_PROGRAM_POINT_SIZE);
}

Renderer::~Renderer() {
    if (point_vbo_ != 0) {
        glDeleteBuffers(1, &point_vbo_);
    }
    if (point_vao_ != 0) {
        glDeleteVertexArrays(1, &point_vao_);
    }
    if (line_vbo_ != 0) {
        glDeleteBuffers(1, &line_vbo_);
    }
    if (line_vao_ != 0) {
        glDeleteVertexArrays(1, &line_vao_);
    }
    delete point_program_;
    delete line_program_;
}

void Renderer::clear() const {
    glClearColor(0.05f, 0.05f, 0.07f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::upload_particles(const HpcsimParticleSystemView& view) {
    const std::size_t particle_count = view.particle_count;
    const GLsizei required_bytes =
        static_cast<GLsizei>(particle_count * 3u * sizeof(float));

    std::vector<float> positions(particle_count * 3u);
    for (std::size_t i = 0; i < particle_count; ++i) {
        positions[i * 3u + 0u] = static_cast<float>(view.positions_x[i]);
        positions[i * 3u + 1u] = static_cast<float>(view.positions_y[i]);
        positions[i * 3u + 2u] = static_cast<float>(view.positions_z[i]);
    }

    glBindVertexArray(point_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, point_vbo_);
    glBufferData(GL_ARRAY_BUFFER, required_bytes, positions.data(), GL_STREAM_DRAW);
    glBindVertexArray(0);

    uploaded_particle_count_ = particle_count;
}

void Renderer::draw_particles(const Mat4& view_matrix,
                              const Mat4& projection_matrix) const {
    if (uploaded_particle_count_ == 0) {
        return;
    }
    const Mat4 view_projection = mat4_multiply(projection_matrix, view_matrix);

    glUseProgram(point_program_->id);
    glUniformMatrix4fv(point_program_->view_projection_location, 1, GL_FALSE,
                       view_projection.m);
    glUniform1f(glGetUniformLocation(point_program_->id, "point_size"), point_size_);
    glUniform3f(glGetUniformLocation(point_program_->id, "point_color"), 0.85f, 0.85f,
                0.95f);

    glBindVertexArray(point_vao_);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(uploaded_particle_count_));
    glBindVertexArray(0);
}

void Renderer::draw_line_strip(const std::vector<Vec3>& points, Vec3 color,
                               const Mat4& view_matrix,
                               const Mat4& projection_matrix) const {
    if (points.size() < 2) {
        return;
    }
    const Mat4 view_projection = mat4_multiply(projection_matrix, view_matrix);
    const GLsizei required_bytes = static_cast<GLsizei>(points.size() * 3u * sizeof(float));

    glBindVertexArray(line_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, line_vbo_);
    glBufferData(GL_ARRAY_BUFFER, required_bytes, points.data(), GL_STREAM_DRAW);

    glUseProgram(line_program_->id);
    glUniformMatrix4fv(line_program_->view_projection_location, 1, GL_FALSE,
                       view_projection.m);
    glUniform3f(glGetUniformLocation(line_program_->id, "line_color"), color.x, color.y,
                color.z);

    glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(points.size()));
    glBindVertexArray(0);
}

GLuint Renderer::compile_shader(GLenum type, const char* source, const char* name) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        char log[1024];
        GLsizei log_length = 0;
        glGetShaderInfoLog(shader, sizeof(log), &log_length, log);
        std::fprintf(stderr, "Renderer: %s shader compile failed:\n%.*s\n", name,
                     log_length > 0 ? log_length : 0, log);
        glDeleteShader(shader);
        throw std::runtime_error(std::string("Renderer: shader compile failed: ") + name);
    }
    return shader;
}

Renderer::ShaderProgram Renderer::compile_program(const char* vertex_source,
                                                  const char* fragment_source) {
    const GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source, "vertex");
    const GLuint fragment_shader =
        compile_shader(GL_FRAGMENT_SHADER, fragment_source, "fragment");

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        char log[1024];
        GLsizei log_length = 0;
        glGetProgramInfoLog(program, sizeof(log), &log_length, log);
        std::fprintf(stderr, "Renderer: program link failed:\n%.*s\n",
                     log_length > 0 ? log_length : 0, log);
        glDeleteProgram(program);
        throw std::runtime_error("Renderer: shader program link failed");
    }

    ShaderProgram result;
    result.id = program;
    result.view_projection_location = glGetUniformLocation(program, "view_projection");
    return result;
}

}  // namespace hpcsim::rendering
