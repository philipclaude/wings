//
//  wings: web interface for graphics applications
//
//  Copyright 2023 - 2026 Philip Claude Caplan
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <set>

#include "../../third_party/tinyobjloader/tiny_obj_loader.h"
#include "../../wings.h"

#define GL_GLEXT_PROTOTYPES
#ifdef __APPLE__
#define __gl_h_
#define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#define GL_SILENCE_DEPRECATION
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

#define GL_CALL(X)                                                           \
  {                                                                          \
    (X);                                                                     \
    GLenum error = glGetError();                                             \
    if (error != GL_NO_ERROR) {                                              \
      const char* message = "";                                              \
      switch (error) {                                                       \
        case GL_INVALID_ENUM:                                                \
          message = "invalid enum";                                          \
          break;                                                             \
        case GL_INVALID_VALUE:                                               \
          message = "invalid value";                                         \
          break;                                                             \
        case GL_INVALID_OPERATION:                                           \
          message = "invalid operation";                                     \
          break;                                                             \
        case GL_INVALID_FRAMEBUFFER_OPERATION:                               \
          message = "invalid framebuffer operation";                         \
          break;                                                             \
        case GL_OUT_OF_MEMORY:                                               \
          message = "out of memory";                                         \
          break;                                                             \
        default:                                                             \
          message = "unknown error";                                         \
      }                                                                      \
      printf("OpenGL error in file %s at line %d: %s\n", __FILE__, __LINE__, \
             message);                                                       \
    }                                                                        \
    assert(error == GL_NO_ERROR);                                            \
  }

struct vec3f : std::array<float, 3> {
  vec3f() : vec3f(0, 0, 0) {}
  vec3f(float x, float y, float z) {
    (*this)[0] = x;
    (*this)[1] = y;
    (*this)[2] = z;
  }
};
struct mat4f {
  mat4f() {
    for (int i = 0; i < 16; i++) data[i] = 0.0;
  }
  float data[16];
  float& operator()(int i, int j) { return data[4 * j + i]; }
  const float& operator()(int i, int j) const { return data[4 * j + i]; }
  void eye() {
    for (int i = 0; i < 16; i++) data[i] = 0.0;
    for (int i = 0; i < 4; i++) (*this)(i, i) = 1.0;
  }
  void print() const {
    const auto& m = *this;
    for (int i = 0; i < 4; i++) {
      std::cout << m(i, 0) << " " << m(i, 1) << " " << m(i, 2) << " " << m(i, 3)
                << std::endl;
    }
  }
};

inline float length(const vec3f& u) {
  return std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
}
inline vec3f cross(const vec3f& u, const vec3f& v) {
  return {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
          u[0] * v[1] - u[1] * v[0]};
}
inline vec3f operator-(const vec3f& u, const vec3f& v) {
  return {u[0] - v[0], u[1] - v[1], u[2] - v[2]};
}
inline vec3f operator+(const vec3f& u, const vec3f& v) {
  return {u[0] + v[0], u[1] + v[1], u[2] + v[2]};
}
inline vec3f operator/(const vec3f& u, const float a) {
  return {u[0] / a, u[1] / a, u[2] / a};
}
inline vec3f operator*(const vec3f& u, const float a) {
  return {u[0] * a, u[1] * a, u[2] * a};
}
inline vec3f operator*(const float a, const vec3f& u) {
  return {u[0] * a, u[1] * a, u[2] * a};
}

inline mat4f operator*(const mat4f& A, const mat4f& B) {
  mat4f C;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      float sum = 0;
      for (int k = 0; k < 4; k++) sum += A(i, k) * B(k, j);
      C(i, j) = sum;
    }
  }
  return C;
}

namespace glm {

inline mat4f lookat(const vec3f& eye, const vec3f& center, const vec3f& up) {
  mat4f m;

  vec3f g = eye - center;
  vec3f w = g / length(g);
  vec3f u = cross(up, w);
  u = u / length(u);
  vec3f v = cross(w, u);

  m(0, 0) = u[0];
  m(1, 0) = v[0];
  m(2, 0) = w[0];
  m(0, 1) = u[1];
  m(1, 1) = v[1];
  m(2, 1) = w[1];
  m(0, 2) = u[2];
  m(1, 2) = v[2];
  m(2, 2) = w[2];

  m(0, 3) = -eye[0] * u[0] - eye[1] * u[1] - eye[2] * u[2];
  m(1, 3) = -eye[0] * v[0] - eye[1] * v[1] - eye[2] * v[2];
  m(2, 3) = -eye[0] * w[0] - eye[1] * w[1] - eye[2] * w[2];
  m(3, 3) = 1.0;

  return m;
}

inline mat4f perspective(float fov, float aspect, float n, float f) {
  mat4f m;
  m.eye();
  float a = 1.0 / tan(fov / 2.0);

  m(0, 0) = a / aspect;
  m(1, 1) = a;
  m(2, 2) = (f + n) / (n - f);
  m(2, 3) = 2 * f * n / (n - f);
  m(3, 2) = -1.0;
  m(3, 3) = 0.0;
  return m;
}

inline mat4f rotation(double X, double Y) {
  float X2 = X * X, Y2 = Y * Y;
  float q = 1 + X2 + Y2;
  float s = 1 - X2 - Y2;
  float r2 = 1 / (q * q), s2 = s * s;
  float A = (s2 + 4 * (Y2 - X2)) * r2;
  float B = -8 * X * Y * r2;
  float C = 4 * s * X * r2;
  float D = (s2 + 4 * (X2 - Y2)) * r2;
  float E = 4 * s * Y * r2;
  float F = (s2 - 4 * (X2 + Y2)) * r2;

  mat4f R;  // initializes to zero
  R(0, 0) = A;
  R(1, 0) = B;
  R(2, 0) = C;
  R(0, 1) = B;
  R(1, 1) = D;
  R(2, 1) = E;
  R(0, 2) = -C;
  R(1, 2) = -E;
  R(2, 2) = F;
  R(3, 3) = 1;
  return R;
}

inline mat4f rotate(const mat4f& a, float angle, const vec3f& axis) {
  mat4f m;
  float x = axis[0], y = axis[1], z = axis[2];
  float len = 1.0 / length(axis);

  x *= len;
  y *= len;
  z *= len;

  float s = sin(angle), c = cos(angle), t = 1 - c;

  float b00 = x * x * t + c;
  float b01 = y * x * t + z * s;
  float b02 = z * x * t - y * s;
  float b10 = x * y * t - z * s;
  float b11 = y * y * t + c;
  float b12 = z * y * t + x * s;
  float b20 = x * z * t + y * s;
  float b21 = y * z * t - x * s;
  float b22 = z * z * t + c;

  float a00 = a(0, 0);
  float a01 = a(1, 0);
  float a02 = a(2, 0);
  float a03 = a(3, 0);
  float a10 = a(0, 1);
  float a11 = a(1, 1);
  float a12 = a(2, 1);
  float a13 = a(3, 1);
  float a20 = a(0, 2);
  float a21 = a(1, 2);
  float a22 = a(2, 2);
  float a23 = a(3, 2);

  m(0, 0) = a00 * b00 + a10 * b01 + a20 * b02;
  m(1, 0) = a01 * b00 + a11 * b01 + a21 * b02;
  m(2, 0) = a02 * b00 + a12 * b01 + a22 * b02;
  m(3, 0) = a03 * b00 + a13 * b01 + a23 * b02;
  m(0, 1) = a00 * b10 + a10 * b11 + a20 * b12;
  m(1, 1) = a01 * b10 + a11 * b11 + a21 * b12;
  m(2, 1) = a02 * b10 + a12 * b11 + a22 * b12;
  m(3, 1) = a03 * b10 + a13 * b11 + a23 * b12;
  m(0, 2) = a00 * b20 + a10 * b21 + a20 * b22;
  m(1, 2) = a01 * b20 + a11 * b21 + a21 * b22;
  m(2, 2) = a02 * b20 + a12 * b21 + a22 * b22;
  m(3, 2) = a03 * b20 + a13 * b21 + a23 * b22;

  m(0, 3) = a(0, 3);
  m(1, 3) = a(1, 3);
  m(2, 3) = a(2, 3);
  m(3, 3) = a(3, 3);

  return m;
}

inline mat4f translation(double dx, double dy) {
  mat4f T;
  T.eye();

  // compute the transformation in screen space
  vec3f t;
  t[0] = float(dx);
  t[1] = float(dy);
  t[2] = 0.0f;

  T(0, 3) = T(0, 0) * t[0] + T(0, 1) * t[1] + T(0, 2) * t[2] + T(0, 3);
  T(1, 3) = T(1, 0) * t[0] + T(1, 1) * t[1] + T(1, 2) * t[2] + T(1, 3);
  T(2, 3) = T(2, 0) * t[0] + T(2, 1) * t[1] + T(2, 2) * t[2] + T(2, 3);
  T(3, 3) = T(3, 0) * t[0] + T(3, 1) * t[1] + T(3, 2) * t[2] + T(3, 3);

  return T;
}

inline mat4f translate(const mat4f& a, const vec3f& t) {
  mat4f m;
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++) m(i, j) = a(i, j);

  m(0, 3) = a(0, 0) * t[0] + a(0, 1) * t[1] + a(0, 2) * t[2] + a(0, 3);
  m(1, 3) = a(1, 0) * t[0] + a(1, 1) * t[1] + a(1, 2) * t[2] + a(1, 3);
  m(2, 3) = a(2, 0) * t[0] + a(2, 1) * t[1] + a(2, 2) * t[2] + a(2, 3);
  m(3, 3) = a(3, 0) * t[0] + a(3, 1) * t[1] + a(3, 2) * t[2] + a(3, 3);

  return m;
}
}  // namespace glm

inline std::string get_base_dir(const std::string& filepath) {
  if (filepath.find_last_of("/\\") != std::string::npos)
    return filepath.substr(0, filepath.find_last_of("/\\"));
  return "";
}

void load_obj(const std::string& filename, std::vector<float>& points,
              std::vector<unsigned int>& triangles) {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;

  std::string base_dir = get_base_dir(filename.c_str());
  if (base_dir.empty()) base_dir = ".";
  base_dir += "/";

  std::string warn;
  std::string err;
  bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                              filename.c_str(), base_dir.c_str(), false);
  if (!warn.empty()) std::cout << "WARNING: " << warn << std::endl;
  if (!err.empty()) std::cout << "ERROR: " << err << std::endl;
  assert(err.empty());
  if (!ret) exit(1);

  points.reserve(attrib.vertices.size());
  for (size_t v = 0; v < attrib.vertices.size(); v++)
    points.push_back(attrib.vertices[v]);

  // read the shapes
  std::vector<int> indices;
  for (size_t i = 0; i < shapes.size(); i++) {
    size_t index_offset = 0;

    // for each face
    for (size_t f = 0; f < shapes[i].mesh.num_face_vertices.size(); f++) {
      size_t fnum = shapes[i].mesh.num_face_vertices[f];
      indices.resize(fnum);
      for (size_t j = 0; j < fnum; j++)
        indices[j] = shapes[i].mesh.indices[index_offset + j].vertex_index;

      assert(fnum == 3);
      for (int j = 0; j < 3; j++) triangles.push_back(indices[j]);
      index_offset += fnum;
    }
  }  // loop over shape
}

void load_icosahedron(std::vector<float>& points,
                      std::vector<unsigned int>& triangles) {
  float t = (1.0 + std::sqrt(5.0)) / 2.0;

  float coordinates[12][3] = {{t, 1, 0}, {-t, 1, 0}, {t, -1, 0}, {-t, -1, 0},
                              {1, 0, t}, {1, 0, -t}, {-1, 0, t}, {-1, 0, -t},
                              {0, t, 1}, {0, -t, 1}, {0, t, -1}, {0, -t, -1}};
  points.resize(36);
  for (int i = 0; i < 12; i++) {
    for (int d = 0; d < 3; d++)
      points[3 * i + d] = coordinates[i][d] / std::sqrt(1 + t * t);
  }

  unsigned int tris[20][3] = {
      {0, 8, 4},   // 0
      {0, 5, 10},  // 1
      {2, 4, 9},   // 2
      {2, 11, 5},  // 3
      {1, 6, 8},   // 4
      {1, 10, 7},  // 5
      {3, 9, 6},   // 6
      {3, 7, 11},  // 7
      {0, 10, 8},  // 8
      {1, 8, 10},  // 9
      {2, 9, 11},  // 10
      {3, 11, 9},  // 11
      {4, 2, 0},   // 12
      {5, 0, 2},   // 13
      {6, 1, 3},   // 14
      {7, 3, 1},   // 15
      {8, 6, 4},   // 16
      {9, 4, 6},   // 17
      {10, 5, 7},  // 18
      {11, 7, 5}   // 19
  };

  triangles.resize(60);
  for (int i = 0; i < 20; i++)
    for (int d = 0; d < 3; d++) triangles[3 * i + d] = tris[i][d];
}

static const char* vs = R"(
#version 330 core
uniform mat4 u_ModelMatrix;
uniform mat4 u_ViewMatrix;
uniform mat4 u_ProjectionMatrix;
layout(location = 0) in vec3 a_Position;
void main() {
    gl_Position = u_ProjectionMatrix * u_ViewMatrix * u_ModelMatrix * vec4(a_Position, 1.0);
})";

const char* fs = R"(
#version 330 core
out vec4 fragColor;
uniform int u_edges;
void main() {
    fragColor = (1 - u_edges) * vec4(.8, .8, .8, 0.0) + u_edges *  vec4(0, 0, 0, 0);
})";

class Mesh {
 public:
  void calculate_edges() {
    std::set<std::pair<unsigned int, unsigned int>> e;
    edges_.reserve(triangles_.size() * 2);
    for (size_t k = 0; k < triangles_.size() / 3; k++) {
      for (int i = 0; i < 3; i++) {
        auto p = triangles_[3 * k + i];
        auto q = triangles_[3 * k + ((i == 2) ? 0 : i + 1)];
        if (q < p) std::swap(p, q);
        auto it = e.find({p, q});
        if (it == e.end()) {
          e.insert({p, q});
          edges_.push_back(p);
          edges_.push_back(q);
        }
      }
    }
  }

  const auto& points() const { return points_; }
  auto& points() { return points_; }
  const auto& triangles() const { return triangles_; }
  auto& triangles() { return triangles_; }
  const auto& edges() const { return edges_; }

  const auto& coordinate(int64_t k, int d) const { return points_[3 * k + d]; }

  int64_t n_points() const { return points_.size() / 3; }

 private:
  std::vector<float> points_;
  std::vector<unsigned int> triangles_;
  std::vector<unsigned int> edges_;
};

namespace wings {
class MeshScene : public Scene {
  struct ClientView {
    mat4f model_matrix;
    mat4f view_matrix;
    mat4f projection_matrix;
    mat4f center_translation, inverse_center_translation;
    vec3f center, eye;
    double x{0}, y{0};
    GLuint vertex_array;
    bool draw_edges{true};
    bool draw_triangles{true};
    glCanvas canvas{800, 600};
  };

 public:
  MeshScene(const Mesh& mesh) : mesh_(mesh) {
    context_ = RenderingContext::create(RenderingContextType::kOpenGL);
    context_->print();
    setup();
  }
  void setup();
  bool render(const ClientInput& input, int client_idx, std::string*);
  void onconnect();

 private:
  const Mesh& mesh_;

  int program;
  GLuint point_buffer;
  GLuint triangle_buffer;
  GLuint edge_buffer;
  std::vector<ClientView> view_;
};

void MeshScene::setup() {
  context_->make_context_current();

  // write points
  GL_CALL(glGenBuffers(1, &point_buffer));
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, point_buffer));
  GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(float) * mesh_.points().size(),
                       mesh_.points().data(), GL_STATIC_DRAW));
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));

  // write indices
  GL_CALL(glGenBuffers(1, &triangle_buffer));
  GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangle_buffer));
  GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                       sizeof(unsigned int) * mesh_.triangles().size(),
                       mesh_.triangles().data(), GL_STATIC_DRAW));
  GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

  GL_CALL(glGenBuffers(1, &edge_buffer));
  GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, edge_buffer));
  GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                       sizeof(unsigned int) * mesh_.edges().size(),
                       mesh_.edges().data(), GL_STATIC_DRAW));
  GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

  // create shader program
  program = glCreateProgram();
  assert(program >= 0);

  auto check_compile = [&](GLuint shader) {
    int result;
    GL_CALL(glGetShaderiv(shader, GL_COMPILE_STATUS, &result));
    if (result == GL_FALSE) {
      int length = 0;
      GL_CALL(glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length));
      if (length > 0) {
        std::string log(length, '\0');
        int written = 0;
        GL_CALL(glGetShaderInfoLog(shader, length, &written, &log[0]));
        std::cout << log << std::endl;
      }
    }
    assert(result == GL_TRUE);
  };

  GLuint vs_shader = glCreateShader(GL_VERTEX_SHADER);
  GL_CALL(glShaderSource(vs_shader, 1, &vs, NULL));
  GL_CALL(glCompileShader(vs_shader));
  check_compile(vs_shader);
  GL_CALL(glAttachShader(program, vs_shader));

  GLuint fs_shader = glCreateShader(GL_FRAGMENT_SHADER);
  GL_CALL(glShaderSource(fs_shader, 1, &fs, NULL));
  GL_CALL(glCompileShader(fs_shader));
  check_compile(fs_shader);
  GL_CALL(glAttachShader(program, fs_shader));

  GL_CALL(glLinkProgram(program));
}

void MeshScene::onconnect() {
  // set up the view
  view_.emplace_back();
  ClientView& view = view_.back();
  view.eye = {0, 0, 0};
  view.center = {0, 0, 0};
  vec3f xmin{1e20, 1e20, 1e20}, xmax = {-1e20, -1e20, -1e20};
  for (int i = 0; i < mesh_.n_points(); i++) {
    for (int d = 0; d < 3; d++) {
      float x = mesh_.coordinate(i, d);
      view.center[d] += x;
      if (x > xmax[d]) xmax[d] = x;
      if (x < xmin[d]) xmin[d] = x;
    }
  }
  view.center = view.center / (mesh_.n_points());
  view.center_translation.eye();
  view.inverse_center_translation.eye();
  for (int d = 0; d < 3; d++) {
    view.center_translation(d, 3) = view.center[d];
    view.inverse_center_translation(d, 3) = -view.center[d];
  }

  float d = std::fabs(xmin[1] - xmax[1]);
  vec3f dir{0, 0, 1};
  view.eye = view.center + 1.05f * d * dir;
  view.center = view.eye - 2.1f * d * dir;

  view.model_matrix.eye();
  vec3f up{0, 1, 0};
  view.view_matrix = glm::lookat(view.eye, view.center, up);
  view.projection_matrix = glm::perspective(
      45.0f, float(view.canvas.width) / float(view.canvas.height), 0.1f,
      1000.0f);

  GL_CALL(glGenVertexArrays(1, &view.vertex_array));
}

bool MeshScene::render(const ClientInput& input, int client_idx,
                       std::string* msg) {
  (void)(msg);  // ignore unused-but-set-parameter warning
  ClientView& view = view_[client_idx];
  msg = nullptr;
  bool updated = false;
  switch (input.type) {
    case InputType::MouseMotion: {
      if (input.dragging) {
        double dx = (view.x - input.x) / view.canvas.width;
        double dy = -(view.y - input.y) / view.canvas.height;
        mat4f R = view.center_translation * glm::rotation(dx, dy) *
                  view.inverse_center_translation;
        view.model_matrix = R * view.model_matrix;
        updated = true;
      }
      view.x = input.x;
      view.y = input.y;
      break;
    }
    case InputType::KeyValueInt: {
      if (input.key == 'Q') {
        quality_ = input.ivalue;
        updated = true;
      } else if (input.key == 'e') {
        view.draw_edges = !view.draw_edges;
        updated = true;
      } else if (input.key == 't') {
        view.draw_triangles = !view.draw_triangles;
        updated = true;
      } else if (input.key == 'W') {
        int w = input.ivalue;
        int h = 0.75 * w;
        view.canvas.resize(w, h);
        view.projection_matrix =
            glm::perspective(45.0, float(w) / float(h), 0.1f, 10000.0f);
        // save the width and height for the scene to write the image
        width_ = w;
        height_ = h;
        // the wings rendering context is currently in the render section.
        // there should be another image request after the size is updated
        updated = false;
      }
      break;
    }
    case InputType::Scroll: {
      vec3f direction = view.eye - view.center;
      view.eye = view.center + direction * 1.0f * input.fvalue;
      vec3f up{0, 1, 0};
      view.view_matrix = glm::lookat(view.eye, view.center, up);
      updated = true;
      break;
    }
    default:
      break;
  }
  if (!updated) return false;

  //  write shader uniforms
  GLint location;
  GL_CALL(glUseProgram(program));
  location = glGetUniformLocation(program, "u_ModelMatrix");
  if (location >= 0)
    GL_CALL(glUniformMatrix4fv(location, 1, GL_FALSE, view.model_matrix.data));
  location = glGetUniformLocation(program, "u_ViewMatrix");
  if (location >= 0)
    GL_CALL(glUniformMatrix4fv(location, 1, GL_FALSE, view.view_matrix.data));
  location = glGetUniformLocation(program, "u_ProjectionMatrix");
  if (location >= 0)
    GL_CALL(
        glUniformMatrix4fv(location, 1, GL_FALSE, view.projection_matrix.data));

  // bind which attributes we want to draw
  GL_CALL(glBindVertexArray(view.vertex_array));
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, point_buffer));
  GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0));
  GL_CALL(glEnableVertexAttribArray(0));
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));

  // draw
  GL_CALL(glViewport(0, 0, view.canvas.width, view.canvas.height));
  GL_CALL(glClearColor(1.0f, 1.0f, 1.0f, 1.0f));
  GL_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
  glEnable(GL_DEPTH_TEST);

  // glEnable(GL_BLEND);
  // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
  glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);

  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(1.0, 1.5);

  location = glGetUniformLocation(program, "u_edges");
  if (view.draw_triangles) {
    glUniform1i(location, 0);
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangle_buffer));
    GL_CALL(glDrawElements(GL_TRIANGLES, mesh_.triangles().size(),
                           GL_UNSIGNED_INT, 0));
  }

  if (view.draw_edges) {
    glUniform1i(location, 1);
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, edge_buffer));
    GL_CALL(glDrawElements(GL_LINES, mesh_.edges().size(), GL_UNSIGNED_INT, 0));
  }

  view.canvas.bind();
  channels_ = 3;
  GLsizei stride = channels_ * view.canvas.width;
  stride += (stride % 4) ? (4 - stride % 4) : 0;
  pixels_.resize(stride * view.canvas.height);
  GL_CALL(glPixelStorei(GL_PACK_ALIGNMENT, 4));
  // GL_CALL(glReadBuffer(GL_BACK));
  GL_CALL(glReadPixels(0, 0, view.canvas.width, view.canvas.height, GL_RGB,
                       GL_UNSIGNED_BYTE, pixels_.data()));
  glFinish();

  return true;
}

}  // namespace wings

int main(int argc, const char** argv) {
  int tcp_port = -1;
  int ws_port = 7681;
  if (argc > 2) ws_port = std::atoi(argv[2]);
  if (argc > 3) tcp_port = std::atoi(argv[3]);

  Mesh mesh;
  if (argc == 1)
    load_icosahedron(mesh.points(), mesh.triangles());
  else
    load_obj(argv[1], mesh.points(), mesh.triangles());
  mesh.calculate_edges();

  wings::MeshScene scene(mesh);
  wings::RenderingServer renderer(scene, ws_port);
  if (tcp_port > 0) renderer.start("../../example/index.html", tcp_port);

  return 0;
}
