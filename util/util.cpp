//
//  wings: web interface for graphics applications
//
//  Copyright 2023 Philip Claude Caplan
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

void GLClipPlane::define(const AABB& aabb) {
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

template <typename T>
void PickableObject::save_points(const Vertices& vertices,
                                 const Topology<T>& topology, uint64_t k) {
  index = k;
  int dim = (vertices.dim() >= 3) ? 3 : vertices.dim();
  points.resize(topology.length(k));
  nodes.resize(points.size());
  for (int j = 0; j < topology.length(k); j++) {
    nodes[j] = topology[k][j];
    for (int d = 0; d < dim; d++) points[j][d] = vertices[nodes[j]][d];
    for (int d = dim; d < 3; d++) points[j][d] = 0.0;  // in case the mesh is 2d
    points[j][3] = 1.0;
  }
}

template <>
PickableObject::PickableObject(const Vertices& vertices,
                               const Topology<Triangle>& topology, uint64_t k,
                               const std::string& n)
    : name(n) {
  save_points(vertices, topology, k);
  triangles = {0, 1, 2};
}

template <>
PickableObject::PickableObject(const Vertices& vertices,
                               const Topology<Quad>& topology, uint64_t k,
                               const std::string& n)
    : name(n) {
  save_points(vertices, topology, k);
  triangles = {0, 1, 2, 0, 2, 3};
}

template <>
PickableObject::PickableObject(const Vertices& vertices,
                               const Topology<Polygon>& topology, uint64_t k,
                               const std::string& n)
    : name(n) {
  save_points(vertices, topology, k);
  triangles.resize(3 * (topology.length(k) - 2));
  int m = 0;
  for (int j = 2; j < topology.length(k); j++) {
    triangles[m++] = 0;
    triangles[m++] = j - 1;
    triangles[m++] = j;
  }
}

template <>
PickableObject::PickableObject(const Vertices& vertices,
                               const Topology<Polyhedron>& topology, uint64_t k,
                               const std::string& n)
    : name(n) {
  index = k;
  int n_faces = topology.length(k);
  const Topology<Polygon>& faces = topology.faces();

  std::unordered_map<uint64_t, uint64_t> point_map;
  point_map.reserve(100);
  triangles.reserve(100);
  for (int j = 0; j < n_faces; j++) {
    uint64_t f = topology[k][j];
    int n_vertices = faces.length(f);
    for (int i = 0; i < n_vertices; i++) {
      uint64_t p = faces[f][i];
      uint64_t q = -1;
      if (point_map.find(p) == point_map.end()) {
        nodes.push_back(p);
        vec4f x;
        for (int d = 0; d < 3; d++) x[d] = vertices[p][d];
        x[3] = 1.0;
        q = points.size();
        points.push_back(x);
        point_map.insert({p, q});
      }
    }

    for (int i = 2; i < n_vertices; i++) {
      uint64_t p0 = point_map.at(faces[f][0]);
      uint64_t p1 = point_map.at(faces[f][i - 1]);
      uint64_t p2 = point_map.at(faces[f][i]);

      triangles.push_back(p0);
      triangles.push_back(p1);
      triangles.push_back(p2);
    }
  }
}

template <>
PickableObject::PickableObject(const Vertices& vertices,
                               const Topology<Tet>& topology, uint64_t k,
                               const std::string& n)
    : name(n) {
  save_points(vertices, topology, k);
  triangles = {1, 2, 3, 2, 0, 3, 0, 1, 3, 2, 1, 0};
}

template <>
PickableObject::PickableObject(const Vertices& vertices,
                               const Topology<Prism>& topology, uint64_t k,
                               const std::string& n)
    : name(n) {
  save_points(vertices, topology, k);
  triangles = {0, 1, 2, 3, 5, 4, 0, 1, 3, 1, 4, 3,
               1, 2, 4, 2, 5, 4, 2, 0, 3, 2, 3, 5};
}

double PickableObject::intersection(int k, const vec3f& eye, const vec3f& ray,
                                    const mat4f& model_matrix) const {
  int i0 = triangles[3 * k];
  int i1 = triangles[3 * k + 1];
  int i2 = triangles[3 * k + 2];

  vec4f q0 = model_matrix * points[i0];
  vec4f q1 = model_matrix * points[i1];
  vec4f q2 = model_matrix * points[i2];

  mat3f A;
  vec3f b;
  for (int d = 0; d < 3; d++) {
    A(d, 0) = q0[d] - q2[d];
    A(d, 1) = q1[d] - q2[d];
    A(d, 2) = -ray[d];
    b[d] = eye[d] - q2[d];
  }
  vec3f c = glm::inverse(A) * b;
  float alpha = c[0], beta = c[1], gamma = 1.0 - alpha - beta;
  float t = c[2];
  if (alpha < 0.0 || alpha > 1.0) return 1e20;
  if (beta < 0.0 || beta > 1.0) return 1e20;
  if (gamma < 0.0 || gamma > 1.0) return 1e20;
  if (t < 0.0) return 1e20;
  return t;
}

double PickableObject::intersection(const vec3f& eye, const vec3f& ray,
                                    const mat4f& model_matrix) const {
  float tmin = 1e20;
  for (int k = 0; k < n_triangles(); k++) {
    float tk = intersection(k, eye, ray, model_matrix);
    if (tk < tmin) tmin = tk;
  }
  return tmin;
}

bool PickableObject::visible(const GLClipPlane& plane) const {
  if (!plane.active) return true;

  vec3f c, n;
  plane.get(c, n);

  for (size_t j = 0; j < points.size(); j++) {
    vec3f p(points[j].data());
    if (dot(p - c, n) < 1e-3) {
      return false;
    }
  }
  return true;
}

}  // namespace wings