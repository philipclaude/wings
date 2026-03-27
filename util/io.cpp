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
#include "io.h"

#include <fmt/format.h>
#include <libmeshb7.h>
#include <tinyobjloader/tiny_obj_loader.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <unordered_set>

#include "mesh.h"

namespace wings {

namespace meshb {

#define ASSERT_GMF(X) ASSERT((X) == 1)

void read_vertices(int64_t fid, int version, Vertices& vertices) {
  int dim = vertices.dim();
  ASSERT(GmfGotoKwd(fid, GmfVertices) > 0);
  size_t n = GmfStatKwd(fid, GmfVertices);
  int n_bits = version == 1 ? 32 : 64;
  LOGF("Reading {} vertices, dim = {} ({}-bit).", n, dim, n_bits);
  std::vector<int> groups(n);
  vertices.allocate(n);
  // auto& groups = vertices.groups();
  if (version == 1) {  // 32-bit reals
    std::vector<float> coords(n * dim);
    ASSERT_GMF(GmfGetBlock(fid, GmfVertices, 1, n, 0, NULL, NULL, GmfFloatVec,
                           dim, &coords[0], &coords[(n - 1) * dim], GmfInt,
                           groups.begin(), groups.end()));
    for (size_t k = 0; k < n; k++) {
      for (int j = 0; j < dim; j++) vertices[k][j] = coords[k * dim + j];
    }
  } else {  // 64-bit reals
    ASSERT_GMF(GmfGetBlock(fid, GmfVertices, 1, n, 0, NULL, NULL, GmfDoubleVec,
                           dim, vertices[0], vertices[n - 1], GmfInt,
                           groups.begin(), groups.end()));
  }
}

void read_edges(int64_t fid, Mesh& mesh) {
  int status = 1;
  int n;
  int data[3] = {-1, -1, -1};
  index_t edge[3] = {0, 0, 0};

  if (GmfGotoKwd(fid, GmfEdges) <= 0) return;

  n = GmfStatKwd(fid, GmfEdges);
  mesh.lines().reserve(n);
  LOG << fmt::format("reading {} edges", n);

  for (int k = 0; k < n; k++) {
    status = GmfGetLin(fid, GmfEdges, &data[0], &data[1], &data[2]);
    ASSERT(status == 1);

    for (int j = 0; j < 2; j++) edge[j] = data[j] - 1;

    mesh.lines().add(edge);
    mesh.lines().set_group(k, data[2]);
  }
}

void read_triangles(int64_t fid, Mesh& mesh) {
  int status = 1;
  int n;
  int data[4] = {-1, -1, -1, -1};
  index_t triangle[4] = {0, 0, 0, 0};

  if (GmfGotoKwd(fid, GmfTriangles) <= 0) return;

  n = GmfStatKwd(fid, GmfTriangles);
  mesh.triangles().reserve(n);
  LOG << fmt::format("reading {} triangles", n);

  for (int k = 0; k < n; k++) {
    status =
        GmfGetLin(fid, GmfTriangles, &data[0], &data[1], &data[2], &data[3]);
    ASSERT(status == 1);

    for (int j = 0; j < 3; j++) triangle[j] = data[j] - 1;
    if (data[3] == 0) {
      // triangle[0] = triangle[1] = triangle[2] = 0;
    }
    mesh.triangles().add(triangle);
    mesh.triangles().set_group(k, data[3]);
  }
}

void read_quads(int64_t fid, Mesh& mesh) {
  int status = 1;
  int n;
  int data[5] = {-1, -1, -1, -1, -1};
  index_t quad[5] = {0, 0, 0, 0, 0};

  if (GmfGotoKwd(fid, GmfQuadrilaterals) <= 0) return;

  n = GmfStatKwd(fid, GmfQuadrilaterals);
  mesh.quads().reserve(n);
  LOG << fmt::format("reading {} quads", n);

  for (int k = 0; k < n; k++) {
    status = GmfGetLin(fid, GmfQuadrilaterals, &data[0], &data[1], &data[2],
                       &data[3], &data[4]);
    ASSERT(status == 1);

    for (int j = 0; j < 4; j++) quad[j] = data[j] - 1;

    mesh.quads().add(quad);
    mesh.quads().set_group(k, data[4]);
  }
}

void read_tetrahedra(int64_t fid, Mesh& mesh) {
  if (GmfGotoKwd(fid, GmfTetrahedra) <= 0) return;

  int status = 1;
  int n;
  int data[5] = {-1, -1, -1, -1, -1};
  index_t tet[5] = {0, 0, 0, 0, 0};

  ASSERT(GmfGotoKwd(fid, GmfTetrahedra) > 0);
  n = GmfStatKwd(fid, GmfTetrahedra);
  mesh.tetrahedra().reserve(n);
  LOG << fmt::format("reading {} tetrahedra", n);

  for (int k = 0; k < n; k++) {
    status = GmfGetLin(fid, GmfTetrahedra, &data[0], &data[1], &data[2],
                       &data[3], &data[4]);
    ASSERT(status == 1);

    for (int j = 0; j < 4; j++) tet[j] = data[j] - 1;

    mesh.tetrahedra().add(tet);
    mesh.tetrahedra().set_group(k, data[4]);
  }
}

template <typename T>
struct Type2Keyword;
template <>
struct Type2Keyword<Line> {
  static const auto type = GmfEdges;
  const std::string name = "lines";
};
template <>
struct Type2Keyword<Triangle> {
  static const auto type = GmfTriangles;
  const std::string name = "triangles";
};
template <>
struct Type2Keyword<Tet> {
  static const auto type = GmfTetrahedra;
  const std::string name = "tetrahedra";
};
template <>
struct Type2Keyword<Pentatope> {
  static const auto type = GmfPentatopes;
  const std::string name = "pentatopes";
};

template <typename T>
void read_simplices(int64_t fid, Topology<T>& topology) {
  Type2Keyword<T> converter;
  auto kwd = converter.type;
  if (GmfGotoKwd(fid, kwd) <= 0) return;

  ASSERT(GmfGotoKwd(fid, kwd) > 0);
  size_t n = GmfStatKwd(fid, kwd);
  LOGF("Reading {} {}.", n, converter.name);

  std::vector<std::array<long, T::n_vertices + 1>> data(n);
  GmfGetBlock(fid, kwd, 1, n, 0, nullptr, nullptr, GmfLongVec,
              T::n_vertices + 1, &data.front(), &data.back());

  topology.allocate(n);
  for (size_t k = 0; k < n; k++) {
    auto group = data[k].back();
    for (int i = 0; i < T::n_vertices; i++) topology(k, i) = data[k][i] - 1;
    topology.set_group(k, group);
  }
}

void read_prisms(int64_t fid, Mesh& mesh) {
  if (GmfGotoKwd(fid, GmfPrisms) <= 0) return;

  int status = 1;
  int n;
  int data[7] = {-1, -1, -1, -1, -1, -1, -1};
  index_t prism[7] = {0, 0, 0, 0, 0, 0, 0};

  ASSERT(GmfGotoKwd(fid, GmfPrisms) > 0);
  n = GmfStatKwd(fid, GmfPrisms);
  mesh.prisms().reserve(n);
  LOG << fmt::format("reading {} prisms", n);

  for (int k = 0; k < n; k++) {
    status = GmfGetLin(fid, GmfPrisms, &data[0], &data[1], &data[2], &data[3],
                       &data[4], &data[5], &data[6]);
    ASSERT(status == 1);

    for (int j = 0; j < 6; j++) prism[j] = data[j] - 1;

    mesh.prisms().add(prism);
    mesh.prisms().set_group(k, data[6]);
  }
}

void read_pyramids(int64_t fid, Mesh& mesh) {
  if (GmfGotoKwd(fid, GmfPyramids) <= 0) return;

  int status = 1;
  int n;
  int data[6] = {-1, -1, -1, -1, -1, -1};
  index_t pyramid[6] = {0, 0, 0, 0, 0, 0};

  ASSERT(GmfGotoKwd(fid, GmfPyramids) > 0);
  n = GmfStatKwd(fid, GmfPyramids);
  mesh.pyramids().reserve(n);
  LOG << fmt::format("reading {} pyramids", n);

  for (int k = 0; k < n; k++) {
    status = GmfGetLin(fid, GmfPyramids, &data[0], &data[1], &data[2], &data[3],
                       &data[4], &data[5]);
    ASSERT(status == 1);

    for (int j = 0; j < 5; j++) pyramid[j] = data[j] - 1;

    mesh.pyramids().add(pyramid);
    mesh.pyramids().set_group(k, data[5]);
  }
}

void read_polydata(int64_t fid, GmfKwdCod header_type, GmfKwdCod entity_type,
                   std::function<void(std::vector<long>&, int)> add_entity_fn) {
  // read the polygon/polyhedra headers: (a, b)
  // a: first index of the vertex in the polygon/polyhedron
  // b: material reference (which will be used as the group number)
  // GmfGetLin doesn't seem to support GmfInnerPolygonHeaders, so
  // a block-based method is used similar to the utility in
  // extern/libmeshb/utilities/libmeshb7_helpers.c
  int64_t n_poly = GmfStatKwd(fid, header_type);
  std::vector<std::array<long, 2>> header(n_poly);
  ASSERT(GmfGetBlock(fid, header_type, 1, n_poly, 0, nullptr, nullptr,
                     GmfLongVec, 2, &header.front(), &header.back()));

  // read the polygon vertices or polyhedron faces
  int64_t n_entities = GmfStatKwd(fid, entity_type);
  std::vector<long> entities(n_entities);
  ASSERT(GmfGetBlock(fid, entity_type, 1, n_entities, 0, nullptr, nullptr,
                     GmfLong, &entities.front(), &entities.back()));

  // add the entities to the topology using the add_entity callback
  LOG << "creating entities";
  std::vector<long> entity(32);
  for (int64_t k = 0; k < n_poly; k++) {
    index_t u = header[k][0] - 1;
    index_t v = (k + 1 == n_poly) ? n_entities : (header[k + 1][0] - 1);
    entity.resize(v - u);
    for (index_t m = u; m < v; m++) entity[m - u] = entities[m] - 1;
    add_entity_fn(entity, header[k][1]);
  }
}

void read_polygons(int64_t fid, Mesh& mesh) {
  bool have_polygons = GmfGotoKwd(fid, GmfPolygons) > 0;
  bool have_boundary_polygons = GmfGotoKwd(fid, GmfBoundaryPolygonHeaders) > 0;
  if (!have_polygons && !have_boundary_polygons) return;

  if (have_boundary_polygons) {
    ASSERT(!have_polygons) << "should only have one set of polygons";
  }
  if (have_polygons) NOT_IMPLEMENTED;

  // read the boundary polygon data
  read_polydata(fid, GmfBoundaryPolygonHeaders, GmfBoundaryPolygonVertices,
                [&mesh](std::vector<long>& polygon, int group) {
                  std::reverse(polygon.begin(), polygon.end());
                  int64_t k = mesh.polygons().n();
                  mesh.polygons().add(polygon.data(), polygon.size());
                  if (group >= 0) mesh.polygons().set_group(k, group);
                });
}

void read_polyhedra(int64_t fid, Mesh& mesh) {
  if (GmfGotoKwd(fid, GmfPolyhedraHeaders) <= 0) return;

  // read the inner polygon data
  read_polydata(fid, GmfInnerPolygonHeaders, GmfInnerPolygonVertices,
                [&mesh](std::vector<long>& polygon, int group) {
                  std::reverse(polygon.begin(), polygon.end());
                  int64_t k = mesh.polyhedra().faces().n();
                  mesh.polyhedra().faces().add(polygon.data(), polygon.size());
                  if (group >= 0) mesh.polyhedra().faces().set_group(k, group);
                });

  //  read the polyhedra faces
  std::vector<index_t> faces(32);
  std::vector<short> signs(32);
  read_polydata(fid, GmfPolyhedraHeaders, GmfPolyhedraFaces,
                [&](std::vector<long>& data, int group) {
                  size_t n = data.size();
                  faces.resize(n);
                  signs.resize(n);
                  for (size_t j = 0; j < n; j++) {
                    long id = data[j] + 1;  // entity id was decremented above
                    faces[j] = std::fabs(id) - 1;
                    signs[j] = (id > 0) ? +1 : -1;
                  }
                  int64_t k = mesh.polyhedra().n();
                  mesh.polyhedra().add(faces.data(), signs.data(), n);
                  if (group >= 0) mesh.polyhedra().set_group(k, group);
                });
}

void write_polygons(int64_t fid, const Mesh& mesh) {
  if (mesh.polygons().n() == 0) return;

  index_t n_boundary = mesh.polygons().n();
  GmfSetKwd(fid, GmfBoundaryPolygonHeaders, n_boundary);
  index_t m = 1;
  for (size_t k = 0; k < mesh.polygons().n(); k++) {
    GmfSetLin(fid, GmfBoundaryPolygonHeaders, m, mesh.polygons().group(k));
    m += mesh.polygons().length(k);
  }

  GmfSetKwd(fid, GmfBoundaryPolygonVertices, m - 1);
  for (size_t k = 0; k < mesh.polygons().n(); k++) {
    for (int j = 0; j < mesh.polygons().length(k); j++)
      GmfSetLin(fid, GmfBoundaryPolygonVertices, mesh.polygons()(k, j) + 1);
  }
}

void write_polyhedra(int64_t fid, const Mesh& mesh) {
  if (mesh.polyhedra().n() == 0) return;

  index_t n_interior = mesh.polyhedra().faces().n();
  GmfSetKwd(fid, GmfInnerPolygonHeaders, n_interior);
  index_t m = 1;
  for (size_t k = 0; k < mesh.polyhedra().faces().n(); k++) {
    GmfSetLin(fid, GmfInnerPolygonHeaders, m,
              mesh.polyhedra().faces().group(k));
    m += mesh.polyhedra().faces().length(k);
  }

  GmfSetKwd(fid, GmfInnerPolygonVertices, m - 1);
  for (size_t k = 0; k < mesh.polyhedra().faces().n(); k++) {
    for (int j = 0; j < mesh.polyhedra().faces().length(k); j++)
      GmfSetLin(fid, GmfInnerPolygonVertices,
                mesh.polyhedra().faces()(k, j) + 1);
  }

  GmfSetKwd(fid, GmfPolyhedraHeaders, mesh.polyhedra().n());
  m = 1;
  for (size_t k = 0; k < mesh.polyhedra().n(); k++) {
    GmfSetLin(fid, GmfPolyhedraHeaders, m, mesh.polyhedra().group(k) + 1);
    m += mesh.polyhedra().length(k);
  }

  GmfSetKwd(fid, GmfPolyhedraFaces, m - 1);
  for (size_t k = 0; k < mesh.polyhedra().n(); k++) {
    for (int j = 0; j < mesh.polyhedra().length(k); j++) {
      int s = mesh.polyhedra().orientation()(k, j);
      index_t f = mesh.polyhedra()(k, j) + 1;
      GmfSetLin(fid, GmfPolyhedraFaces, s * f);
    }
  }
}

void write_prisms(int64_t fid, const Mesh& mesh) {
  if (mesh.prisms().n() == 0) return;

  int n = mesh.prisms().n();
  GmfSetKwd(fid, GmfPrisms, n);

  int indices[7];
  for (size_t k = 0; k < mesh.prisms().n(); k++) {
    for (int j = 0; j < 6; j++) indices[j] = mesh.prisms()(k, j) + 1;
    indices[6] = mesh.prisms().group(k) + 1;
    GmfSetLin(fid, GmfPrisms, indices[0], indices[1], indices[2], indices[3],
              indices[4], indices[5], indices[6]);
  }
}

template <typename T>
void write_simplex(int64_t fid, const Topology<T>& topology) {
  const int nv = T::n_vertices;
  int type;
  if (nv == 1)
    type = GmfCorners;
  else if (nv == 2)
    type = GmfEdges;
  else if (nv == 3)
    type = GmfTriangles;
  else if (nv == 4)
    type = GmfTetrahedra;
  else
    NOT_POSSIBLE;

  int n = topology.n();
  GmfSetKwd(fid, type, n);

  int indices[5];
  for (int k = 0; k < n; k++) {
    for (int j = 0; j < nv; j++) indices[j] = topology(k, j) + 1;
    indices[nv] = topology.group(k) + 1;

    if (nv == 1)
      GmfSetLin(fid, type, indices[0]);
    else if (nv == 2)
      GmfSetLin(fid, type, indices[0], indices[1], indices[2]);
    else if (nv == 3)
      GmfSetLin(fid, type, indices[0], indices[1], indices[2], indices[3]);
    else if (nv == 4)
      GmfSetLin(fid, type, indices[0], indices[1], indices[2], indices[3],
                indices[4]);
    else
      NOT_POSSIBLE;
  }
}

void read(const std::string& filename, Mesh& mesh) {
  // open the file
  int version;
  int dim;
  int64_t fid = GmfOpenMesh(filename.c_str(), GmfRead, &version, &dim);
  ASSERT(fid) << "Could not open mesh file " << filename;
  mesh.vertices().set_dim(dim);

  // 0-dimensional
  read_vertices(fid, version, mesh.vertices());

  // 1-dimensional
  read_simplices(fid, mesh.lines());

  // 2-dimensional
  read_simplices(fid, mesh.triangles());
  read_quads(fid, mesh);
  read_polygons(fid, mesh);

  // 3-dimensional
  read_simplices(fid, mesh.tetrahedra());
  read_prisms(fid, mesh);
  read_pyramids(fid, mesh);
  read_polyhedra(fid, mesh);

  // 4-dimensional
  read_simplices(fid, mesh.pentatopes());

  GmfCloseMesh(fid);
}

void write(const Mesh& mesh, const std::string& filename, bool twod) {
  int dim = mesh.vertices().dim();
  if (twod) dim = 2;

  int64_t fid = GmfOpenMesh(filename.c_str(), GmfWrite, GmfDouble, dim);
  ASSERT(fid);

  GmfSetKwd(fid, GmfVertices, mesh.vertices().n());
  for (size_t k = 0; k < mesh.vertices().n(); k++) {
    int ref = mesh.vertices().group(k) + 1;
    if (dim == 2)
      GmfSetLin(fid, GmfVertices, mesh.vertices()[k][0], mesh.vertices()[k][1],
                ref);
    else if (dim == 3)
      GmfSetLin(fid, GmfVertices, mesh.vertices()[k][0], mesh.vertices()[k][1],
                mesh.vertices()[k][2], ref);
  }

  // write the elements
  write_simplex(fid, mesh.lines());
  write_simplex(fid, mesh.triangles());
  write_polygons(fid, mesh);
  if (!twod) {
    write_simplex(fid, mesh.tetrahedra());
    write_prisms(fid, mesh);
  }
  write_polyhedra(fid, mesh);

  GmfCloseMesh(fid);
}
}  // namespace meshb

namespace obj {

std::string get_base_dir(const std::string& filepath) {
  if (filepath.find_last_of("/\\") != std::string::npos)
    return filepath.substr(0, filepath.find_last_of("/\\"));
  return "";
}

void read(const std::string& filename, Mesh& mesh) {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;

  std::string base_dir = get_base_dir(filename.c_str());
  if (base_dir.empty()) {
    base_dir = ".";
  }
  base_dir += "/";

  std::string warn;
  std::string err;
  bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                              filename.c_str(), base_dir.c_str(), false);
  if (!warn.empty()) {
    std::cout << "WARN: " << warn << std::endl;
  }
  if (!err.empty()) {
    std::cerr << err << std::endl;
  }
  ASSERT(ret) << "failed to load " << filename.c_str();

  LOG << fmt::format("# of vertices  = {}", attrib.vertices.size() / 3);
  LOG << fmt::format("# of normals   = {}", attrib.normals.size() / 3);
  LOG << fmt::format("# of texcoords = {}", attrib.texcoords.size() / 2);
  LOG << fmt::format("# of materials = {}", materials.size());
  LOG << fmt::format("# of shapes    = {}", shapes.size());

  // read the vertices
  mesh.vertices().set_dim(3);
  mesh.vertices().reserve(attrib.vertices.size() / 3);
  for (size_t v = 0; v < attrib.vertices.size() / 3; v++) {
    mesh.vertices().add(&attrib.vertices[3 * v]);
  }

  // read the texture coordinates
  array2d<coord_t> texcoord(2);
  texcoord.reserve(attrib.texcoords.size() / 2);
  for (size_t t = 0; t < attrib.texcoords.size() / 2; t++) {
    texcoord.add(&attrib.texcoords[2 * t]);
  }

  // read the shapes
  std::vector<index_t> indices;
  for (size_t i = 0; i < shapes.size(); i++) {
    size_t index_offset = 0;

    ASSERT(shapes[i].mesh.num_face_vertices.size() ==
           shapes[i].mesh.material_ids.size());
    ASSERT(shapes[i].mesh.num_face_vertices.size() ==
           shapes[i].mesh.smoothing_group_ids.size());

    // for each face
    for (size_t f = 0; f < shapes[i].mesh.num_face_vertices.size(); f++) {
      size_t fnum = shapes[i].mesh.num_face_vertices[f];
      indices.resize(fnum);
      for (size_t j = 0; j < fnum; j++)
        indices[j] = shapes[i].mesh.indices[index_offset + j].vertex_index;

      if (fnum == 3) {
        mesh.triangles().add(indices.data());
      } else if (fnum == 4) {
        mesh.quads().add(indices.data());
      } else {
        mesh.polygons().add(indices.data(), indices.size());
      }

      index_offset += fnum;
    }
  }  // loop over shape

// initialize the field of texture coordinates (after building the mesh)
#if 0
  mesh.fields().add("texcoord", 2, 1);

  Field& field = mesh.fields().fields()["texcoord"];
  field.triangles().reserve(mesh.triangles().n());
  // field.quads().reserve(mesh.quads().n());

  std::vector<coord_t> data;
  for (size_t i = 0; i < shapes.size(); i++) {
    size_t index_offset = 0;

    // for each face
    for (size_t f = 0; f < shapes[i].mesh.num_face_vertices.size(); f++) {
      size_t fnum = shapes[i].mesh.num_face_vertices[f];
      data.resize(2 * fnum);
      for (size_t j = 0; j < fnum; j++) {
        data[0 * fnum + j] =
            texcoord[shapes[i].mesh.indices[index_offset + j].texcoord_index]
                    [0];
        data[1 * fnum + j] =
            texcoord[shapes[i].mesh.indices[index_offset + j].texcoord_index]
                    [1];
      }

      if (fnum == 3) {
        field.triangles().add(data.data());
      } else if (fnum == 4) {
        // field.quads().add(data.data());
      } else {
        field.polygons().add(data.data(), data.size());
      }

      index_offset += fnum;
    }
  }  // loop over shape

  ASSERT(field.triangles().n() == mesh.triangles().n());
#endif
}

void write(const Mesh& mesh, const std::string& filename) {
  FILE* out = fopen(filename.c_str(), "w");
  ASSERT(out) << "failed to open file: " << filename;

  // header
  fprintf(out, "# obj mesh from avro\n");

  // write vertices
  for (size_t k = 0; k < mesh.vertices().n(); k++) {
    const auto* p = mesh.vertices()[k];
    double z = (mesh.vertices().dim() == 2) ? 0.0 : p[2];
    fprintf(out, "v %.10f %.10f %.10f\n", p[0], p[1], z);
  }

  // write triangles
  for (size_t k = 0; k < mesh.triangles().n(); k++) {
    fprintf(out, "f ");
    for (int j = 0; j < 3; j++) {
      int idx = mesh.triangles()[k][j];
      fprintf(out, "%d ", idx + 1);
    }
    fprintf(out, "\n");
  }

  // write quads
  for (size_t k = 0; k < mesh.quads().n(); k++) {
    fprintf(out, "f ");
    for (int j = 0; j < 4; j++) {
      int idx = mesh.quads()[k][j];
      fprintf(out, "%d ", idx + 1);
    }
    fprintf(out, "\n");
  }

  if (mesh.polygons().n() > 0)
    std::cout << "[warning] skipping polygons for now" << std::endl;

  fclose(out);
}
}  // namespace obj

}  // namespace wings
