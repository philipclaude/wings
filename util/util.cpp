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
#include "util.h"

#include "clip.h"
#include "mesh.h"
#include "opengl.h"
#include "shader.h"

namespace wings {

template <>
void AABB<3>::print() const {
  LOGF("Box: {}, {}, {} -> {}, {}, {}", min_[0], min_[1], min_[2], max_[0],
       max_[1], max_[2]);
}

template <>
void AABB<4>::print() const {
  LOGF("Box: {}, {}, {} {} -> {}, {}, {} {}", min_[0], min_[1], min_[2],
       min_[3], max_[0], max_[1], max_[2], max_[3]);
}

GLClipPlane::GLClipPlane()
    : length(-1.0f), visible(false), distance(0.0f), active(false) {
  transformation.eye();
  shader = std::make_unique<ShaderProgram>();
}

GLClipPlane::~GLClipPlane() {
  if (vertex_array >= 0) {
    GLuint vao = vertex_array;
    glDeleteVertexArrays(1, &vao);
  }
}

void GLClipPlane::initialize() {
  shader->compile(detail::clip_vs, detail::clip_fs, detail::clip_gs, {}, {});
  GLuint vao, vbo;
  GL_CALL(glGenVertexArrays(1, &vao));
  GL_CALL(glGenBuffers(1, &vbo));
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, vbo));
  GL_CALL(
      glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat), &length, GL_STATIC_DRAW));
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));

  vertex_array = vao;
  buffer = vbo;
}

void GLClipPlane::define(const AABB<3>& aabb) {
  center = 0.5f * (aabb.min() + aabb.max());
  vec3f dims = 1.2f * (aabb.max() - aabb.min());
  float a = std::max(dims[0], std::max(dims[1], dims[2]));

  vec3f u, v;
  if (dimension == 2) {
    u = {1, 0, 0};
    v = {0, 1, 0};
  } else if (dimension == 1) {
    u = {1, 0, 0};
    v = {0, 0, 1};
  } else if (dimension == 0) {
    u = {0, 1, 0};
    v = {0, 0, 1};
  } else
    NOT_POSSIBLE;

  coordinates[0] = 0.5f * a * (u - v);
  coordinates[1] = 0.5f * a * (u + v);
  coordinates[2] = -0.5f * a * (u - v);
  coordinates[3] = -0.5f * a * (u + v);

  transformation.eye();
}

void GLClipPlane::get(vec3f& point, vec3f& normal) const {
  vec4f p = {coordinates[0][0], coordinates[0][1], coordinates[0][2], 1.0f};
  p = transformation * p;  // any point is fine

  vec3f u = coordinates[1] - coordinates[0];
  vec3f v = coordinates[2] - coordinates[0];
  normal = unit_vector(cross(u, v));

  vec4f n = {normal[0], normal[1], normal[2], 0.0f};
  n = glm::inverse(glm::transpose(transformation)) * n;

  point = {p[0], p[1], p[2]};
  normal = {n[0], n[1], n[2]};
  normal = normal * -1.0f;
}

void GLClipPlane::update() {
  // translation from the origin to the clip center,
  // the range in the frontend code is in % of the box length
  vec3f t = center;
  t[dimension] += 0.01f * distance * length[dimension];

  // axis of rotation, depending on the clip normal direction
  vec3f axis;
  if (dimension == 0)
    axis = {1, 0, 0};
  else if (dimension == 1)
    axis = {0, 0, 1};
  else if (dimension == 2)
    axis = {0, 1, 0};
  else
    NOT_POSSIBLE;

  // the transformation of the clip in world space is a rotation, followed by
  // a translation
  mat4f identity;
  identity.eye();
  transformation =
      glm::translate(identity, t) * glm::rotate(identity, M_PI / 2.0, axis);
}

void GLClipPlane::draw(const mat4f& model_matrix, const mat4f& view_matrix,
                       const mat4f& perspective_matrix) {
  shader->use();

  mat4f model_view_matrix = view_matrix * model_matrix * transformation;
  mat4f mvp_matrix = perspective_matrix * model_view_matrix;

  shader->set_uniform("u_ModelViewMatrix", model_view_matrix);
  shader->set_uniform("u_ModelViewProjectionMatrix", mvp_matrix);

  shader->set_uniform("u_x0", coordinates[0]);
  shader->set_uniform("u_x1", coordinates[1]);
  shader->set_uniform("u_x2", coordinates[2]);
  shader->set_uniform("u_x3", coordinates[3]);

  // disable culling when drawing the clipping plane, but save the original
  // state
  // GLboolean culling;
  // GL_CALL(glGetBooleanv(GL_CULL_FACE, &culling));
  // glDisable(GL_CULL_FACE);

  GL_CALL(glBindVertexArray(vertex_array));
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, buffer));
  GL_CALL(glDrawArrays(GL_POINTS, 0, 1));

  // reset the culling state
  // if (culling == GL_TRUE) glEnable(GL_CULL_FACE);
}

}  // namespace wings