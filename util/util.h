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
#pragma once

#include <fmt/format.h>

#include <memory>
#include <vector>

#include "glm.h"
#include "log.h"

namespace wings {

class Vertices;
template <typename T>
class Topology;
class ShaderProgram;

static inline mat4f get_basis_projector(int d) {
  mat4f p;
  int row = 0;
  for (int i = 0; i < 4; i++) {
    if (i == d) continue;
    p(row, i) = 1;
    row++;
  }
  return p;
}

static inline mat4f get_basis_unprojector(int d) {
  mat4f p;
  int col = 0;
  for (int i = 0; i < 4; i++) {
    if (i == d) continue;
    p(i, col) = 1;
    col++;
  }
  return p;
}

template <int dim>
struct Ray {
  typedef vec<dim, float> vecr;
  Ray(vecr p, vecr r) {
    origin = p;
    direction = unit_vector(r);
  }

  Ray(vec3f p, vec3f r, const mat4f& m) {
    vec4f rh = {r[0], r[1], r[2], 0};
    vec3f dir = unit_vector((m * rh).xyz());
    direction[dim - 1] = 0;
    for (int i = 0; i < 3; i++) direction[i] = dir[i];
    vec4f ph = {p[0], p[1], p[2], 1};
    p = (m * ph).xyz();
    for (int i = 0; i < 3; i++) origin[i] = p[i];
  }

  Ray(vec3f p, vec3f r, const mat4f& m, int hyperplane_dim,
      float hyperplane_distance)
      : Ray(p, r, m) {
    assert(dim == 4);
    mat4f q = get_basis_unprojector(hyperplane_dim);
    origin = q * origin;
    direction = q * direction;
    origin[hyperplane_dim] = hyperplane_distance;
  }

  vecr origin;
  vecr direction;
};

template <int dim>
class AABB {
  typedef vec<dim, float> vecb;

 public:
  AABB() {}
  AABB(const vecb& min, const vecb& max) {
    min_ = min;
    max_ = max;
    for (int d = 0; d < dim; d++) {
      if (max_[d] - min_[d] < 1e-4f) {
        min_[d] -= 1e-4f;
        max_[d] += 1e-4f;
      }
    }
  }

  AABB(const AABB& boxl, const AABB& boxr) {
    for (int d = 0; d < dim; d++) {
      min_[d] = std::min(boxl.min()[d], boxr.min()[d]);
      max_[d] = std::max(boxl.max()[d], boxr.max()[d]);
      if (max_[d] - min_[d] < 1e-4f) {
        min_[d] -= 1e-4f;
        max_[d] += 1e-4f;
      }
    }
  }

  bool intersect(const Ray<dim>& ray, float tmin, float tmax) const {
    for (int d = 0; d < dim; ++d) {
      float inv_d = 1.0f / ray.direction[d];
      if (ray.direction[d] == 0.0) continue;
      const auto tld = std::min((min_[d] - ray.origin[d]) * inv_d,
                                (max_[d] - ray.origin[d]) * inv_d);
      const auto tud = std::max((min_[d] - ray.origin[d]) * inv_d,
                                (max_[d] - ray.origin[d]) * inv_d);
      tmin = std::max(tld, tmin);
      tmax = std::min(tud, tmax);
      if (tmax <= tmin) return false;
    }
    return true;
  }

  void print() const {
    LOGF("Box: {}, {}, {} -> {}, {}, {}", min_[0], min_[1], min_[2], max_[0],
         max_[1], max_[2]);
  }

  const auto& min() const { return min_; }
  const auto& max() const { return max_; }

  auto& min() { return min_; }
  auto& max() { return max_; }

 private:
  vecb min_;
  vecb max_;
};

struct GLClipPlane;

struct PickableObject {
  template <typename T>
  PickableObject(const Vertices& vertices, const Topology<T>& topology,
                 uint64_t k, const std::string& name);

  template <typename T>
  void save_points(const Vertices& vertices, const Topology<T>& topology,
                   uint64_t k);

  double intersection(const vec3f& point, const vec3f& ray,
                      const mat4f& model_matrix) const;
  double intersection(int k, const vec3f& point, const vec3f& ray,
                      const mat4f& model_matrix) const;

  int n_triangles() const { return triangles.size() / 3; }

  bool visible(const GLClipPlane& plane) const;

  std::string name;
  std::vector<vec4f> points;
  std::vector<uint64_t> triangles;
  std::vector<uint64_t> nodes;
  uint64_t index;
};

struct GLClipPlane {
  GLClipPlane();
  ~GLClipPlane();

  void initialize();

  void define(const AABB<3>& aabb);

  void get(vec3f& point, vec3f& normal) const;

  void update();

  void draw(const mat4f& model_matrix, const mat4f& view_matrix,
            const mat4f& perspective_matrix);

  vec3f length;
  bool visible;
  float distance;
  bool active;

  vec3f center;
  vec3f coordinates[4];
  float direction;
  int dimension;
  mat4f transformation;

  int vertex_array;
  int buffer;
  std::shared_ptr<ShaderProgram> shader;
};

}  // namespace wings