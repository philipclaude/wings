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

#include <functional>
#include <map>

#include "array2d.h"
#include "glm.h"
#include "types.h"

namespace wings {

class Mesh;
template <typename T>
class Topology;

template <typename T>
int n_basis(int order);

template <>
inline int n_basis<Line>(int p) {
  return (p + 1);
}

template <>
inline int n_basis<Triangle>(int p) {
  return (p + 1) * (p + 2) / 2;
}

template <>
inline int n_basis<Quad>(int p) {
  return (p + 1) * (p + 1);
}

template <>
inline int n_basis<Tet>(int p) {
  return (p + 1) * (p + 2) * (p + 3) / 6;
}

template <>
inline int n_basis<Prism>(int p) {
  ASSERT(p <= 1);
  return 1;
}

template <>
inline int n_basis<Pyramid>(int p) {
  ASSERT(p <= 1);
  return 1;
}

template <>
inline int n_basis<Polygon>(int p) {
  ASSERT(p <= 1);
  return 1;
}

template <>
inline int n_basis<Polyhedron>(int p) {
  ASSERT(p <= 1);
  return 1;
}

template <typename T>
struct ReferenceElement {
  typedef vec<T::dimension + 1, coord_t> BaryCoord_t;
  ReferenceElement(int order = 0) { set_order(order); }
  void set_order(int order);
  std::vector<BaryCoord_t> nodes;
};

template <>
inline void ReferenceElement<Line>::set_order(int order) {
  nodes.resize(n_basis<Line>(order));
  if (order == 0)
    nodes[0] = {1.0 / 2.0, 1.0 / 2.0};
  else if (order == 1) {
    nodes[0] = {0, 1};
    nodes[1] = {1, 0};
  } else
    NOT_IMPLEMENTED;
}

template <>
inline void ReferenceElement<Triangle>::set_order(int order) {
  nodes.resize(n_basis<Triangle>(order));
  if (order == 0)
    nodes[0] = {1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0};
  else if (order == 1) {
    nodes[0] = {0, 0, 1};
    nodes[1] = {1, 0, 0};
    nodes[2] = {0, 1, 0};
  } else
    NOT_IMPLEMENTED;
}

template <>
inline void ReferenceElement<Quad>::set_order(int order) {
  nodes.resize(n_basis<Quad>(order));
  if (order == 0)
    nodes[0] = {0.5, 0.5, 0.0};
  else if (order == 1) {
    nodes[0] = {0, 0, 1};
    nodes[1] = {1, 0, 0};
    nodes[2] = {1, 1, 0};
    nodes[3] = {0, 1, 0};
  } else
    NOT_IMPLEMENTED;
}

template <>
inline void ReferenceElement<Tet>::set_order(int order) {
  nodes.resize(n_basis<Tet>(order));
  if (order == 0)
    nodes[0] = {1.0 / 4.0, 1.0 / 4.0, 1.0 / 4.0, 1.0 / 4.0};
  else if (order == 1) {
    nodes[0] = {0, 0, 0, 1};
    nodes[1] = {1, 0, 0, 0};
    nodes[2] = {0, 1, 0, 0};
    nodes[3] = {0, 0, 1, 0};
  } else
    NOT_IMPLEMENTED;
}

template <>
inline void ReferenceElement<Polygon>::set_order(int order) {
  nodes.resize(n_basis<Polygon>(order));
  if (order == 0) nodes[0] = {0, 0, 0};
  // else NOT_IMPLEMENTED;
}

template <>
inline void ReferenceElement<Prism>::set_order(int order) {
  nodes.resize(n_basis<Prism>(order));
  if (order == 0) nodes[0] = {0, 0, 0.5, 0.5};
  // else NOT_IMPLEMENTED;
}

template <>
inline void ReferenceElement<Pyramid>::set_order(int order) {
  nodes.resize(n_basis<Pyramid>(order));
  if (order == 0) nodes[0] = {0, 0, 0.5, 0.5};
  // else NOT_IMPLEMENTED;
}

template <>
inline void ReferenceElement<Polyhedron>::set_order(int order) {
  nodes.resize(n_basis<Polygon>(order));
  if (order == 0) nodes[0] = {0, 0, 0, 0};
  // else NOT_IMPLEMENTED;
}

template <typename T>
class ElementField : public array2d<coord_t> {
 public:
  ElementField() : ElementField(0, 1) {}

  ElementField(int order, int ranks)
      : array2d<coord_t>(ranks * n_basis<T>(order)),
        order_(order),
        ranks_(ranks) {}

  int m() const { return n_basis<T>(order_); }

  int order() const { return order_; }
  int ranks() const { return ranks_; }
  using array2d<coord_t>::stride;

  void set_size(int ranks, int order) {
    order_ = order;
    ranks_ = ranks;
    array2d<coord_t>::set_stride(ranks * n_basis<T>(order));
    reference_.set_order(order);
    ASSERT(array2d<coord_t>::n() == 0);
  }

 private:
  int order_;
  int ranks_;
  ReferenceElement<T> reference_;
};

template <>
class ElementField<Polyhedron> : public array2d<coord_t> {
 public:
  ElementField() : ElementField(0, 1) {}

  ElementField(int order, int ranks)
      : array2d<coord_t>(ranks * n_basis<Polyhedron>(order)),
        faces_(order, ranks),
        order_(order),
        ranks_(ranks) {}

  int m() const { return n_basis<Polyhedron>(order_); }

  int order() const { return order_; }
  int ranks() const { return ranks_; }
  using array2d<coord_t>::stride;

  void set_size(int ranks, int order) {
    order_ = order;
    ranks_ = ranks;
    array2d<coord_t>::set_stride(ranks * n_basis<Polyhedron>(order));
    reference_.set_order(order);
    ASSERT(array2d<coord_t>::n() == 0);
    faces_.set_size(ranks, order);
  }

  ElementField<Polygon>& faces() { return faces_; }
  const ElementField<Polygon>& faces() const { return faces_; }

 private:
  ElementField<Polygon> faces_;
  int order_;
  int ranks_;
  ReferenceElement<Polyhedron> reference_;
};

class Field {
 public:
  Field() : Field(0, 1) {}
  Field(int ranks) : Field(ranks, 0) {}
  Field(int ranks, int order) : ranks_(ranks), order_(order) {}

  int ranks() const { return ranks_; }
  int order() const { return order_; }

  void set(int ranks, int order) {
    ranks_ = ranks;
    order_ = order;
    lines_.set_size(ranks, order);
    triangles_.set_size(ranks, order);
    quads_.set_size(ranks, order);
    polygons_.set_size(ranks, order);
    prisms_.set_size(ranks, order);
    pyramids_.set_size(ranks, order);
    tetrahedra_.set_size(ranks, order);
  }
  void initialize(const Mesh& mesh);
  void evaluate(const Mesh& mesh,
                std::function<void(const coord_t*, coord_t*)> f);

  const ElementField<Line>& lines() const { return lines_; }
  const ElementField<Triangle>& triangles() const { return triangles_; }
  const ElementField<Quad>& quads() const { return quads_; }
  const ElementField<Polygon>& polygons() const { return polygons_; }
  const ElementField<Prism>& prisms() const { return prisms_; }
  const ElementField<Pyramid>& pyramids() const { return pyramids_; }
  const ElementField<Tet>& tetrahedra() const { return tetrahedra_; }
  const ElementField<Polyhedron>& polyhedra() const { return polyhedra_; }

  ElementField<Line>& lines() { return lines_; }
  ElementField<Triangle>& triangles() { return triangles_; }
  ElementField<Quad>& quads() { return quads_; }
  ElementField<Polygon>& polygons() { return polygons_; }
  ElementField<Prism>& prisms() { return prisms_; }
  ElementField<Pyramid>& pyramids() { return pyramids_; }
  ElementField<Tet>& tetrahedra() { return tetrahedra_; }
  ElementField<Polyhedron>& polyhedra() { return polyhedra_; }

  template <typename T>
  const ElementField<T>& get() const;

  template <typename T>
  ElementField<T>& get();

 private:
  int ranks_;
  int order_;
  ElementField<Line> lines_;
  ElementField<Triangle> triangles_;
  ElementField<Quad> quads_;
  ElementField<Polygon> polygons_;
  ElementField<Tet> tetrahedra_;
  ElementField<Prism> prisms_;
  ElementField<Pyramid> pyramids_;
  ElementField<Polyhedron> polyhedra_;
};

class FieldLibrary {
 public:
  FieldLibrary() {}

  void set_defaults(const Mesh& mesh);

  std::map<std::string, Field>& fields() { return fields_; }
  const std::map<std::string, Field>& fields() const { return fields_; }
  void add(const std::string& name, int ranks, int order) {
    fields_.emplace(name, ranks);
    fields_[name].set(ranks, order);
  }

 private:
  std::map<std::string, Field> fields_;
};

}  // namespace wings
