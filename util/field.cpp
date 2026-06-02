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
#include "field.h"

#include "mesh.h"

namespace wings {

#define UNUSED(X) (void)(X)

template <typename T>
static void allocate(const Topology<T>& topology, ElementField<T>& field) {
  ASSERT(field.n() == 0);
  ASSERT(n_basis<T>(field.order()) * field.ranks() == field.stride());
  field.set_layout(topology.layout());
  if (topology.stride() < 0) field.set_stride(-1);

  field.reserve(topology.n());
  std::vector<double> data;
  for (index_t k = 0; k < topology.n(); k++) {
    data.resize(topology.length(k), 0.0);
    // for (int j = 0; j < topology.length(k); j++) field[k][j] = 0.0;
    field.add(data.data(), data.size());
  }
}

void Field::initialize(const Mesh& mesh) {
  if (mesh.lines().n() > 0) allocate(mesh.lines(), lines_);
  if (mesh.triangles().n() > 0) allocate(mesh.triangles(), triangles_);
  if (mesh.quads().n() > 0) allocate(mesh.quads(), quads_);
  if (mesh.polygons().n() > 0) allocate(mesh.polygons(), polygons_);
  if (mesh.tetrahedra().n() > 0) allocate(mesh.tetrahedra(), tetrahedra_);
  if (mesh.prisms().n() > 0) allocate(mesh.prisms(), prisms_);
  if (mesh.pyramids().n() > 0) allocate(mesh.pyramids(), pyramids_);
  if (mesh.polyhedra().n() > 0) allocate(mesh.polyhedra(), polyhedra_);
  if (mesh.polyhedra().faces().n() > 0)
    allocate(mesh.polyhedra().faces(), polyhedra_.faces());
}

template <typename T>
void evaluate_basis(int order, const coord_t* x, coord_t* phi);
template <typename T>
void polytope_correction(index_t n, std::vector<coord_t>& phi) {
  UNUSED(n);
  UNUSED(phi);
  return;
}

template <>
void polytope_correction<Polygon>(index_t n, std::vector<coord_t>& phi) {
  phi.resize(n, 1.0 / n);
}

template <>
void polytope_correction<Polyhedron>(index_t n, std::vector<coord_t>& phi) {
  // extract vrep...will need more mesh info so will need to refactor
  phi.resize(n, 1.0 / n);
}

template <>
void polytope_correction<Prism>(index_t n, std::vector<coord_t>& phi) {
  UNUSED(n);
  phi.resize(6, 1.0 / 6.0);
}

template <>
void evaluate_basis<Line>(int order, const coord_t* x, coord_t* phi) {
  if (order == 0)
    phi[0] = 1.0;
  else if (order == 1) {
    phi[0] = 1.0 - x[0];
    phi[1] = x[0];
  } else
    NOT_IMPLEMENTED;
}

template <>
void evaluate_basis<Triangle>(int order, const coord_t* x, coord_t* phi) {
  if (order == 0)
    phi[0] = 1.0;
  else if (order == 1) {
    phi[0] = 1.0 - x[0] - x[1];
    phi[1] = x[0];
    phi[2] = x[1];
  } else
    NOT_IMPLEMENTED;
}

template <>
void evaluate_basis<Quad>(int order, const coord_t* x, coord_t* phi) {
  if (order == 0)
    phi[0] = 1.0;
  else if (order == 1) {
    phi[0] = (1.0 - x[0]) * (1.0 - x[1]);
    phi[1] = x[0] * (1.0 - x[1]);
    phi[2] = x[0] * x[1];
    phi[3] = (1.0 - x[0]) * x[1];
  } else
    NOT_IMPLEMENTED;
}

template <>
void evaluate_basis<Tet>(int order, const coord_t* x, coord_t* phi) {
  if (order == 0)
    phi[0] = 1.0;
  else if (order == 1) {
    phi[0] = 1.0 - x[0] - x[1] - x[2];
    phi[1] = x[0];
    phi[2] = x[1];
    phi[3] = x[2];
  } else
    NOT_IMPLEMENTED;
}

template <>
void evaluate_basis<Polygon>(int order, const coord_t* x, coord_t* phi) {
  UNUSED(x);
  if (order == 0)
    phi[0] = 1.0;
  else if (order == 1)
    return;
  else
    NOT_IMPLEMENTED;
}

template <>
void evaluate_basis<Prism>(int order, const coord_t* x, coord_t* phi) {
  UNUSED(x);
  if (order == 0)
    phi[0] = 1.0;
  else if (order == 1)
    return;
  else
    NOT_IMPLEMENTED;
}

template <>
void evaluate_basis<Pyramid>(int order, const coord_t* x, coord_t* phi) {
  UNUSED(x);
  if (order == 0)
    phi[0] = 1.0;
  else if (order == 1)
    return;
  else
    NOT_IMPLEMENTED;
}

template <>
void evaluate_basis<Polyhedron>(int order, const coord_t* x, coord_t* phi) {
  UNUSED(x);
  if (order == 0)
    phi[0] = 1.0;
  else if (order == 1)
    return;
  else
    NOT_IMPLEMENTED;
}

template <typename T>
static void evaluate_topology(
    const Vertices& vertices, const Topology<T>& topology,
    ElementField<T>& field, std::function<void(const coord_t* x, coord_t*)> f) {
  ReferenceElement<T> element;
  element.set_order(field.order());
  // using vec = typename ReferenceElement<T>::vec;

  ASSERT(n_basis<T>(field.order()) == int(element.nodes.size()));

  std::vector<coord_t> q(field.ranks());
  // for now only linear elements are supported
  std::vector<coord_t> phi(n_basis<T>(1));
  ASSERT(topology.n() == field.n());
  for (index_t k = 0; k < field.n(); k++) {
    for (size_t j = 0; j < element.nodes.size(); j++) {
      // evaluate the element basis functions at this reference coordinate
      // (only linear elements are currently supported, hence the 1)
      evaluate_basis<T>(1, element.nodes[j].data(), phi.data());
      polytope_correction<T>(topology.length(k), phi);

      // interpolate the mesh coordinates
      std::array<coord_t, 3> p;
      for (size_t i = 0; i < phi.size(); i++) {
        // vertex coordinate corresponding to this basis function
        std::array<coord_t, 3> v;
        for (int d = 0; d < vertices.dim(); d++)
          v[d] = vertices[topology[k][i]][d];

        for (int d = 0; d < vertices.dim(); d++) p[d] += v[d] * phi[i];
      }

      // evaluate the function at the point
      f(p.data(), q.data());
      for (int r = 0; r < field.ranks(); r++) {
        index_t offset = r * (n_basis<T>(field.order()));
        field[k][offset + j] = q[r];
      }
    }
  }
}

static void evaluate_polyhedra(
    const Vertices& vertices, const Topology<Polyhedron>& topology,
    ElementField<Polyhedron>& field,
    std::function<void(const coord_t* x, coord_t*)> f) {
  ASSERT(field.order() == 0);
  ASSERT(topology.n() == field.n());
  UNUSED(vertices);
  UNUSED(f);
  LOG << "Skipping field evaluation on polyhedra.";
}

void Field::evaluate(const Mesh& mesh,
                     std::function<void(const coord_t*, coord_t*)> f) {
  initialize(mesh);
  evaluate_topology(mesh.vertices(), mesh.lines(), lines_, f);
  evaluate_topology(mesh.vertices(), mesh.triangles(), triangles_, f);
  evaluate_topology(mesh.vertices(), mesh.quads(), quads_, f);
  evaluate_topology(mesh.vertices(), mesh.polygons(), polygons_, f);
  evaluate_topology(mesh.vertices(), mesh.prisms(), prisms_, f);
  evaluate_topology(mesh.vertices(), mesh.pyramids(), pyramids_, f);
  evaluate_topology(mesh.vertices(), mesh.tetrahedra(), tetrahedra_, f);
  evaluate_polyhedra(mesh.vertices(), mesh.polyhedra(), polyhedra_, f);
  evaluate_topology(mesh.vertices(), mesh.polyhedra().faces(),
                    polyhedra_.faces(), f);
}

template <>
const ElementField<Triangle>& Field::get<Triangle>() const {
  return triangles_;
}

template <>
ElementField<Triangle>& Field::get<Triangle>() {
  return triangles_;
}

template <>
const ElementField<Polygon>& Field::get<Polygon>() const {
  return polygons_;
}

template <>
ElementField<Polygon>& Field::get<Polygon>() {
  return polygons_;
}

template <>
const ElementField<Quad>& Field::get<Quad>() const {
  return quads_;
}

template <>
ElementField<Quad>& Field::get<Quad>() {
  return quads_;
}

template <>
const ElementField<Tet>& Field::get<Tet>() const {
  return tetrahedra_;
}

template <>
ElementField<Tet>& Field::get<Tet>() {
  return tetrahedra_;
}

template <>
const ElementField<Polyhedron>& Field::get<Polyhedron>() const {
  return polyhedra_;
}

template <>
ElementField<Polyhedron>& Field::get<Polyhedron>() {
  return polyhedra_;
}

template <typename T>
static void set_group(const Topology<T>& topology, ElementField<T>& field) {
  ASSERT(field.n() == topology.n()) << "must initialize field";
  for (size_t k = 0; k < field.n(); k++) {
    field[k][0] = topology.group(k);
  }
}

void FieldLibrary::set_defaults(const Mesh& mesh) {
  add("cell", 1, 0);
  auto f1 = [](const coord_t* x, coord_t* y) {
    UNUSED(x);
    y[0] = double(rand()) / double(RAND_MAX);
  };
  fields_["cell"].evaluate(mesh, f1);

  add("group", 1, 0);
  Field& field = fields_["group"];
  field.initialize(mesh);
  set_group(mesh.lines(), field.lines());
  set_group(mesh.triangles(), field.triangles());
  set_group(mesh.quads(), field.quads());
  set_group(mesh.polygons(), field.polygons());
  set_group(mesh.prisms(), field.prisms());
  set_group(mesh.pyramids(), field.pyramids());
  set_group(mesh.tetrahedra(), field.tetrahedra());
  set_group(mesh.polyhedra(), field.polyhedra());
  set_group(mesh.polyhedra().faces(), field.polyhedra().faces());
}

#undef UNUSED

}  // namespace wings
