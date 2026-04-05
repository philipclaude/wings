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
#pragma once

#include <array>
#include <unordered_map>

#include "array2d.h"
#include "field.h"
#include "types.h"

namespace wings {

static constexpr uint32_t kNotOnGeometry = std::numeric_limits<uint32_t>::max();

class TopologyBase : public array2d<index_t> {
 public:
  using array2d<index_t>::n;
  using array2d<index_t>::length;

 protected:
  TopologyBase(int stride) : array2d<index_t>(stride) {}

 public:
  void allocate(size_t n) {
    array2d<index_t>::allocate(n);
    group_.resize(n);
  }

  void reserve(int64_t m) {
    array2d<index_t>::reserve(m);
    group_.reserve(m);
  }

  template <typename R>
  void add(const R* x, int m = -1) {
    (m < 0) ? array2d<index_t>::template add<R>(x)
            : array2d<index_t>::template add<R>(x, m);
    group_.push_back(-1);
  }

  int group(index_t k) const {
    ASSERT(k < n());
    return group_[k];
  }

  void set_group(index_t k, int32_t value) {
    ASSERT(k < n());
    group_[k] = value;
  }

  const auto& groups() const { return group_; }

 protected:
  std::vector<int32_t> group_;
};

template <typename T>
class Topology : public TopologyBase {
 public:
  using TopologyBase::length;
  using TopologyBase::n;
  using type = T;

  Topology() : TopologyBase(T::n_vertices) {}

  void append_edges(std::vector<Edge>& edges) const;
  void flip_orientation();
};

template <>
class Topology<Polyhedron> : public TopologyBase {
 public:
  Topology() : TopologyBase(-1), orientation_(-1) {}

  void reserve(int n) { array2d<index_t>::reserve(n); }

  template <typename R, typename S>
  void add(const R* x, const S* s, int n) {
    TopologyBase::template add<R>(x, n);
    orientation_.add<S>(s, n);
    group_.push_back(-1);
  }

  void append_edges(std::vector<Edge>& edges) const;

  const Topology<Polygon>& faces() const { return faces_; }
  Topology<Polygon>& faces() { return faces_; }

  const array2d<short>& orientation() const { return orientation_; }

 private:
  array2d<short> orientation_;
  Topology<Polygon> faces_;
  std::vector<int> group_;  // -1 if interior, >= 0 for boundary
};

class Vertices : public array2d<coord_t> {
 public:
  static constexpr int max_dim = 4;
  Vertices(int dim) : array2d<coord_t>(dim) { ASSERT(dim <= max_dim); }

  void reserve(size_t n_vertices) {
    array2d<coord_t>::reserve(n_vertices);
    group_.reserve(n_vertices);
  }

  void allocate(size_t n_vertices) {
    array2d<double>::allocate(n_vertices);
    group_.resize(n_vertices);
  }

  int dim() const { return array2d<coord_t>::stride(); }
  void set_dim(int dim) { array2d<coord_t>::set_stride(dim); }

  template <typename R>
  void add(const R* x) {
    array2d<coord_t>::template add<R>(x);
    group_.push_back(-1);
  }

  void set_group(size_t k, int value) {
    ASSERT(k < n()) << fmt::format("Access {} out of {}.", k, n());
    group_[k] = value;
  }

  const std::vector<int32_t>& group() const { return group_; }
  std::vector<int32_t>& group() { return group_; }
  int32_t group(size_t k) const {
    ASSERT(k < n());
    return group_[k];
  }

  const auto& groups() const { return group_; }

  void print() const;

  void free() {
    array2d<coord_t>::free();
    decltype(group_)().swap(group_);
  }

  void clear() {
    array2d<coord_t>::clear();
    group_.clear();
  }

 private:
  std::vector<int32_t> group_;
};

class Mesh {
 public:
  Mesh(int dim) : vertices_(dim) {}

  Topology<Line>& lines() { return lines_; }
  const Topology<Line>& lines() const { return lines_; }

  Topology<Triangle>& triangles() { return triangles_; }
  const Topology<Triangle>& triangles() const { return triangles_; }

  Topology<Quad>& quads() { return quads_; }
  const Topology<Quad>& quads() const { return quads_; }

  Topology<Tet>& tetrahedra() { return tetrahedra_; }
  const Topology<Tet>& tetrahedra() const { return tetrahedra_; }

  Topology<Prism>& prisms() { return prisms_; }
  const Topology<Prism>& prisms() const { return prisms_; }

  Topology<Pyramid>& pyramids() { return pyramids_; }
  const Topology<Pyramid>& pyramids() const { return pyramids_; }

  Topology<Polygon>& polygons() { return polygons_; }
  const Topology<Polygon>& polygons() const { return polygons_; }

  Topology<Polyhedron>& polyhedra() { return polyhedra_; }
  const Topology<Polyhedron>& polyhedra() const { return polyhedra_; }

  auto& pentatopes() { return pentatopes_; }
  const auto& pentatopes() const { return pentatopes_; }

  Vertices& vertices() { return vertices_; }
  const Vertices& vertices() const { return vertices_; }

  void get_edges(std::vector<Edge>& edges) const;

  template <typename T>
  const Topology<T>& get() const;

  template <typename T>
  Topology<T>& get();

  const FieldLibrary& fields() const { return fields_; }
  FieldLibrary& fields() { return fields_; }

  int get_surface_connected_components(std::vector<int>& components) const;

 protected:
  Vertices vertices_;
  Topology<Line> lines_;
  Topology<Triangle> triangles_;
  Topology<Quad> quads_;
  Topology<Tet> tetrahedra_;
  Topology<Prism> prisms_;
  Topology<Pyramid> pyramids_;
  Topology<Polygon> polygons_;
  Topology<Polyhedron> polyhedra_;
  Topology<Pentatope> pentatopes_;

  FieldLibrary fields_;
};

}  // namespace wings
