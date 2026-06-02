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
#include <cstdint>

namespace wings {

using coord_t = double;
using index_t = uint32_t;

typedef std::array<index_t, 2> Edge;

struct Line {
  static const int dimension = 1;
  static const int n_vertices = 2;
  static const int n_edges = 1;
  static const int n_faces = 2;
  static constexpr int edges[2] = {0, 1};
};

struct Triangle {
  static const int dimension = 2;
  static const int n_vertices = 3;
  static const int n_edges = 3;
  static const int n_faces = 3;
  static int edges[6];
  static int faces[6];
  typedef Line face_type;
};

struct Quad {
  static const int dimension = 2;
  static const int n_vertices = 4;
  static const int n_edges = 4;
  static const int n_faces = 4;
  static constexpr int edges[8] = {0, 1, 1, 2, 2, 3, 3, 0};
  static int faces[8];
  typedef Line face_type;
};

struct Polygon {
  static const int dimension = 2;
  static const int n_vertices = -1;
  static const int n_edges = -1;
  static const int n_faces = -1;
  static constexpr int* edges = nullptr;
  typedef Line face_type;
};

struct Tet {
  static const int dimension = 3;
  static const int n_vertices = 4;
  static const int n_edges = 6;
  static const int n_faces = 4;
  static int edges[12];
  static int faces[12];
  typedef Triangle face_type;
};

struct Pentatope {
  static const int dimension = 4;
  static const int n_vertices = 5;
  static const int n_edges = 10;
  static const int n_faces = 5;
  static int edges[20];
  static int faces[20];
  typedef Tet face_type;
};
using PentatopeFace_t = std::array<int, 4>;
static std::array<PentatopeFace_t, 5> kPentatopesFaces = {
    {{0, 1, 2, 3}, {0, 2, 3, 4}, {0, 1, 3, 4}, {0, 1, 2, 4}, {1, 2, 3, 4}}};

struct Prism {
  static const int dimension = 3;
  static const int n_vertices = 6;
  static const int n_edges = 9;
  static const int n_faces = 5;
  static constexpr int edges[18] = {0, 1, 1, 2, 2, 0, 3, 4, 4,
                                    5, 5, 3, 0, 3, 1, 4, 2, 5};
  static constexpr int faces[18] = {0, 1, 2, 3, 5, 4, 0, 3, 4,
                                    1, 1, 4, 5, 2, 0, 2, 5, 3};
  typedef Polygon face_type;  // this should never be used
};

struct Pyramid {
  static const int dimension = 3;
  static const int n_vertices = 5;
  static const int n_edges = 8;
  static const int n_faces = 5;
  static constexpr int edges[16] = {0, 1, 1, 2, 2, 3, 3, 0,
                                    0, 4, 1, 4, 2, 4, 3, 4};
  static constexpr int faces[18] = {0, 1, 4, 1, 2, 4, 2, 3,
                                    4, 3, 0, 4, 0, 3, 2, 1};
  typedef Polygon face_type;  // this should never be used
};

struct Polyhedron {
  static const int dimension = 3;
  static const int n_vertices = -1;
  static const int n_edges = -1;
  static const int n_faces = -1;
  static constexpr int* edges = nullptr;
  typedef Polygon face_type;
};

}  // namespace wings
