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
#include "mesh.h"

#include <set>

namespace wings {

int Tet::edges[12] = {0, 1, 0, 2, 0, 3, 1, 2, 1, 3, 2, 3};
int Tet::faces[12] = {1, 2, 3, 2, 0, 3, 0, 1, 3, 0, 2, 1};

int Quad::faces[8] = {0, 1, 1, 2, 2, 3, 3, 0};
int Triangle::faces[6] = {1, 2, 2, 0, 0, 1};
int Triangle::edges[6] = {0, 1, 1, 2, 2, 0};

template <typename T>
void get_element_edges(const Topology<T>& elems, int k,
                       std::vector<Edge>& edges) {
  for (int j = 0; j < T::n_edges; j++) {
    auto e0 = elems(k, T::edges[2 * j]);
    auto e1 = elems(k, T::edges[2 * j + 1]);

    if (e0 > e1) std::swap(e0, e1);
    edges.push_back({e0, e1});
  }
}

template <>
void get_element_edges<Polygon>(const Topology<Polygon>& elems, int k,
                                std::vector<Edge>& edges) {
  auto n_vertices = elems.length(k);
  for (int j = 0; j < n_vertices; j++) {
    auto e0 = elems(k, j);
    auto e1 = elems(k, (j + 1) % n_vertices);

    if (e0 > e1) std::swap(e0, e1);
    edges.push_back({e0, e1});
  }
}

template <typename T>
void Topology<T>::append_edges(std::vector<Edge>& edges) const {
  std::set<Edge> E;
  std::vector<Edge> element_edges;

  for (size_t k = 0; k < n(); k++) {
    element_edges.clear();
    get_element_edges(*this, k, element_edges);
    for (const auto& e : element_edges) {
      if (E.find(e) == E.end()) {
        E.insert(e);
        edges.push_back(e);
      }
    }
  }
}

template <>
void Topology<Triangle>::flip_orientation() {
  for (size_t k = 0; k < n(); k++) {
    index_t t1 = (*this)(k, 1);
    index_t t2 = (*this)(k, 2);
    (*this)(k, 2) = t1;
    (*this)(k, 1) = t2;
  }
}

void Mesh::get_edges(std::vector<Edge>& edges) const {
  edges.clear();
  triangles_.append_edges(edges);
  tetrahedra_.append_edges(edges);
  polygons_.append_edges(edges);
}

template <>
const Topology<Triangle>& Mesh::get<Triangle>() const {
  return triangles_;
}

template <>
Topology<Triangle>& Mesh::get<Triangle>() {
  return triangles_;
}

template <>
const Topology<Polygon>& Mesh::get<Polygon>() const {
  return polygons_;
}

template <>
Topology<Polygon>& Mesh::get<Polygon>() {
  return polygons_;
}

template <>
const Topology<Quad>& Mesh::get<Quad>() const {
  return quads_;
}

template <>
Topology<Quad>& Mesh::get<Quad>() {
  return quads_;
}

template <>
const Topology<Tet>& Mesh::get<Tet>() const {
  return tetrahedra_;
}
template <>
Topology<Tet>& Mesh::get<Tet>() {
  return tetrahedra_;
}

template <>
const Topology<Pentatope>& Mesh::get<Pentatope>() const {
  return pentatopes_;
}
template <>
Topology<Pentatope>& Mesh::get<Pentatope>() {
  return pentatopes_;
}

template <>
const Topology<Polyhedron>& Mesh::get<Polyhedron>() const {
  return polyhedra_;
}

template <>
Topology<Polyhedron>& Mesh::get<Polyhedron>() {
  return polyhedra_;
}

void Vertices::print() const {
  for (size_t k = 0; k < n(); k++) {
    std::cout << fmt::format("v[{}] = (", k);
    for (int d = 0; d < dim(); d++) {
      std::cout << (*this)[k][d];
      if (d + 1 < dim())
        std::cout << ", ";
      else
        std::cout << ") ";
    }
    std::cout << std::endl;
  }
}

}  // namespace wings
