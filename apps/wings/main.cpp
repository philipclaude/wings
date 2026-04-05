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
#include <fstream>
#include <memory>
#include <set>

#include "../../wings.h"
#include "bvh.h"
#include "colormaps.h"
#include "glm.h"
#include "io.h"
#include "mesh.h"
#include "opengl.h"
#include "shader.h"
#include "util.h"

namespace wings {

enum TextureIndex {
  POINT_TEXTURE = 0,
  NORMAL_TEXTURE = 1,
  TEXCOORD_TEXTURE = 2,
  COLORMAP_TEXTURE = 3,
  INDEX_TEXTURE = 4,
  FIELD_TEXTURE = 5,
  IMAGE_TEXTURE = 6,
  AUX_TEXTURE = 7,
  GROUP_TEXTURE = 8,
  HIDDEN_TEXTURE = 9
};

struct ClientView {
  mat4f model_matrix;
  mat4f view_matrix;
  mat4f projection_matrix;
  mat4f center_translation, inverse_center_translation;
  mat4f translation_matrix;
  vec3f center, eye;
  wings::mat4f basis_projector = get_basis_projector(3);
  wings::vec4f hypernormal{0, 0, 0, -1};
  wings::vec4f hypercenter{0, 0, 0, 0};
  int hyperdir{3};
  float size{1.0};
  float fov{M_PI / 4.0};
  double x{0}, y{0};
  GLuint vertex_array;
  std::unordered_map<std::string, bool> active = {
      {"Points", false},    {"Nodes", true},   {"Lines", false},
      {"Triangles", true},  {"Quads", true},   {"Polygons", true},
      {"Tetrahedra", true}, {"Prisms", false}, {"Pyramids", false},
      {"Polyhedra", false}};
  int show_wireframe{1};
  float transparency{1.0};
  int lighting{1};
  bool culling{false};
  GLClipPlane plane;
  const BVHLeafBase* picked{nullptr};
  int field_mode{0};
  int field_index{0};
  glCanvas canvas{800, 600, true};
  float near{1e-1};
  float far{100};
  bool interactive{false};
  bool hover_highlight{false};
};

class BasePrimitive {
 public:
  BasePrimitive(const std::string& name, const Mesh& mesh,
                BoundingVolumeHierarchyBase& bvh)
      : name_(name), mesh_(mesh), bvh_(bvh) {}
  virtual ~BasePrimitive() {}
  virtual void draw(ShaderProgram&, const ClientView&) const = 0;
  virtual void write(const GLClipPlane*) = 0;

  float umin() const { return umin_; }
  float umax() const { return umax_; }

  const std::string& name() const { return name_; }
  int n_cells() const { return n_cells_; }
  int n_triangles() const { return n_triangles_; }
  int max_cell() const { return max_cell_; }

 protected:
  std::string name_;
  const Mesh& mesh_;
  float umin_;
  float umax_;
  int n_cells_{0};
  int n_triangles_{0};
  int max_cell_{-1};
  BoundingVolumeHierarchyBase& bvh_;
};

template <typename T>
struct VisualizationTriangles;

template <>
struct VisualizationTriangles<Triangle> {
  static const int n = 1;
  static int triangles[1][3];
  static int edges[1];
};
int VisualizationTriangles<Triangle>::triangles[1][3] = {{0, 1, 2}};
int VisualizationTriangles<Triangle>::edges[1] = {7};

template <>
struct VisualizationTriangles<Quad> {
  static const int n = 2;
  static int triangles[2][3];
  static int edges[2];
};
int VisualizationTriangles<Quad>::triangles[2][3] = {{0, 1, 2}, {0, 2, 3}};
int VisualizationTriangles<Quad>::edges[2] = {5, 3};

template <>
struct VisualizationTriangles<Tet> {
  static const int n = 2;
  static int triangles[4][3];
  static int edges[4];
};
int VisualizationTriangles<Tet>::triangles[4][3] = {
    {2, 3, 1}, {0, 3, 2}, {1, 3, 0}, {0, 2, 1}};
int VisualizationTriangles<Tet>::edges[4] = {7, 7, 7, 7};

class PointTexture {
 public:
  PointTexture() {}

  ~PointTexture() {
    // TODO
  }

  void write(const Vertices& vertices) {
    GL_CALL(glGenTextures(1, &texture_));
    GL_CALL(glGenBuffers(1, &buffer_));
    int dim = vertices.dim();
    std::vector<GLfloat> coordinates(vertices[0],
                                     vertices[0] + dim * vertices.n());
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, buffer_));
    GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * coordinates.size(),
                         coordinates.data(), GL_STATIC_DRAW));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
  }

  void bind(const ShaderProgram& shader, const std::string& name) {
    shader.use();
    GL_CALL(glActiveTexture(GL_TEXTURE0 + POINT_TEXTURE));
    GL_CALL(glBindTexture(GL_TEXTURE_BUFFER, texture_));
    GL_CALL(glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, buffer_));
    shader.set_uniform(name.c_str(), int(POINT_TEXTURE));
  }

 private:
  GLuint texture_;
  GLuint buffer_;
};

template <typename T>
class LinearPrimitive4d : public BasePrimitive {
 public:
  LinearPrimitive4d(PointTexture& points, const std::string& name,
                    const Mesh& mesh, BoundingVolumeHierarchyBase& bvh)
      : BasePrimitive(name, mesh, bvh), points_(points) {
    write(mesh.get<T>());
  }

  void buffer(const std::vector<GLuint>& indices) {
    GL_CALL(glGenTextures(1, &index_texture_));
    GL_CALL(glGenBuffers(1, &index_buffer_));
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_));
    GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         sizeof(GLuint) * indices.size(), indices.data(),
                         GL_STATIC_DRAW));
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
  }

  void write(const Topology<T>& topology) {
    std::vector<GLuint> indices(topology.data().begin(), topology.data().end());
    buffer(indices);
    n_draw_ = topology.n();
    stride_ = topology.stride();
    max_cell_ = topology.n();

    GL_CALL(glGenBuffers(1, &group_buffer_));
    GL_CALL(glGenTextures(1, &group_texture_));
    std::vector<GLuint> group(topology.groups().begin(),
                              topology.groups().end());
    // write the group data
    GL_CALL(glBindBuffer(GL_TEXTURE_BUFFER, group_buffer_));
    GL_CALL(glBufferData(GL_TEXTURE_BUFFER, sizeof(GLuint) * group.size(),
                         group.data(), GL_STATIC_DRAW));
    GL_CALL(glBindBuffer(GL_TEXTURE_BUFFER, 0));
  }

  void write(const GLClipPlane*) { NOT_IMPLEMENTED; }

  void draw(ShaderProgram& shader, const ClientView& view) const {
    shader.use();

    shader.set_uniform("u_width", view.canvas.width);
    shader.set_uniform("u_height", view.canvas.height);
    shader.set_uniform("u_BasisProjectionMatrix", view.basis_projector);
    shader.set_uniform("u_hyperplane_normal", view.hypernormal);
    shader.set_uniform("u_hyperplane_center", view.hypercenter);

    // bind the group buffer to the group texture
    GL_CALL(glActiveTexture(GL_TEXTURE0 + GROUP_TEXTURE));
    GL_CALL(glBindTexture(GL_TEXTURE_BUFFER, group_texture_));
    GL_CALL(glTexBuffer(GL_TEXTURE_BUFFER, GL_R32UI, group_buffer_));
    shader.set_uniform("group", int(GROUP_TEXTURE));

    GL_CALL(glActiveTexture(GL_TEXTURE0 + INDEX_TEXTURE));
    GL_CALL(glBindTexture(GL_TEXTURE_BUFFER, index_texture_));
    if (stride_ == 4) {
      GL_CALL(glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32UI, index_buffer_));
    } else if (stride_ == 3) {
      GL_CALL(glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32UI, index_buffer_));
    } else {
      GL_CALL(glTexBuffer(GL_TEXTURE_BUFFER, GL_R32UI, index_buffer_));
    }
    shader.set_uniform("index", int(INDEX_TEXTURE));
    points_.bind(shader, "points");
    GL_CALL(glDrawArrays(GL_POINTS, 0, n_draw_));
  }

 private:
  PointTexture& points_;
  GLuint index_buffer_;
  GLuint index_texture_;
  GLuint group_buffer_;
  GLuint group_texture_;
  size_t n_draw_;
  int stride_;
};

template <typename T>
class LinearPrimitive3d : public BasePrimitive {
 public:
  LinearPrimitive3d(const std::string& name, const Mesh& mesh,
                    BoundingVolumeHierarchyBase& bvh)
      : BasePrimitive(name, mesh, bvh) {
    GL_CALL(glGenBuffers(1, &point_buffer_));
    GL_CALL(glGenTextures(1, &point_texture_));
    GL_CALL(glGenBuffers(1, &aux_buffer_));
    GL_CALL(glGenTextures(1, &aux_texture_));
    GL_CALL(glGenBuffers(1, &group_buffer_));
    GL_CALL(glGenTextures(1, &group_texture_));
    write(nullptr);
  }

  ~LinearPrimitive3d() {
    GL_CALL(glDeleteBuffers(1, &point_buffer_));
    GL_CALL(glDeleteTextures(1, &point_texture_));
    GL_CALL(glDeleteBuffers(1, &aux_buffer_));
    GL_CALL(glDeleteTextures(1, &aux_texture_));
    GL_CALL(glDeleteBuffers(1, &group_buffer_));
    GL_CALL(glDeleteTextures(1, &group_texture_));
  }

  void write(const GLClipPlane* plane) {
    if (T::dimension == 3 and !plane) return;

    int dim = mesh_.vertices().dim();
    const auto& topology = mesh_.get<T>();

    auto& bvh = static_cast<BoundingVolumeHierarchy<BVHTriangle>&>(bvh_);

    std::vector<GLfloat> points;
    points.reserve(topology.n() * 9 * VisualizationTriangles<T>::n);
    std::vector<GLuint> aux, group;
    aux.reserve(topology.n() * VisualizationTriangles<T>::n);
    group.reserve(topology.n() * VisualizationTriangles<T>::n);

    vec3f center, normal;
    if (plane) {
      plane->get(center, normal);
    }

    max_cell_ = topology.n();
    n_triangles_ = 0;
    for (size_t k = 0; k < topology.n(); k++) {
      // check if this element is visible wrt to the plane
      // i.e. if there is at least one pair of vertices on opposite sides
      if (plane) {
        int side = 0;
        for (int i = 0; i < T::n_vertices; i++) {
          vec3f p{0, 0, 0};
          for (int d = 0; d < dim; d++)
            p[d] = mesh_.vertices()[topology(k, i)][d];
          if (dot(p - center, normal) > 0)
            side++;
          else
            side--;
        }
        // skip if the cell is entirely on one side
        // only keep volume elements with vertices on either side of the plane
        // only keep surface elements on the non-negative side of the plane
        if (T::dimension == 3 && std::abs(side) == T::n_vertices) continue;
        if (T::dimension == 2 && side == -T::n_vertices) continue;
      }

      std::array<vec3f, 3> triangle;
      for (int j = 0; j < VisualizationTriangles<T>::n; j++) {
        n_triangles_++;
        uint32_t aux_data = VisualizationTriangles<T>::edges[j] + (k << 3);
        aux.push_back(aux_data);
        group.push_back(topology.group(k));
        for (int i = 0; i < 3; i++) {
          auto vtx = topology(k, VisualizationTriangles<T>::triangles[j][i]);
          for (int d = 0; d < dim; d++) {
            triangle[i][d] = mesh_.vertices()[vtx][d];
            points.push_back(triangle[i][d]);
          }
          if (dim == 2) points.push_back(0.0);
        }
        bvh.add(triangle, k, topology.group(k));
      }
    }
    points.shrink_to_fit();
    aux.shrink_to_fit();
    group.shrink_to_fit();

    // write the coordinates
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, point_buffer_));
    GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * points.size(),
                         points.data(), GL_STATIC_DRAW));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    // write aux data: edge visibility info and cell number
    GL_CALL(glBindBuffer(GL_TEXTURE_BUFFER, aux_buffer_));
    GL_CALL(glBufferData(GL_TEXTURE_BUFFER, sizeof(GLuint) * aux.size(),
                         aux.data(), GL_STATIC_DRAW));
    GL_CALL(glBindBuffer(GL_TEXTURE_BUFFER, 0));

    // write the group data
    GL_CALL(glBindBuffer(GL_TEXTURE_BUFFER, group_buffer_));
    GL_CALL(glBufferData(GL_TEXTURE_BUFFER, sizeof(GLuint) * group.size(),
                         group.data(), GL_STATIC_DRAW));
    GL_CALL(glBindBuffer(GL_TEXTURE_BUFFER, 0));
  }

  void draw(ShaderProgram& shader, const ClientView&) const {
    shader.use();
    if (n_triangles_ == 0) {
      return;
    }

    // bind the aux buffer to the aux texture
    GL_CALL(glActiveTexture(GL_TEXTURE0 + AUX_TEXTURE));
    GL_CALL(glBindTexture(GL_TEXTURE_BUFFER, aux_texture_));
    GL_CALL(glTexBuffer(GL_TEXTURE_BUFFER, GL_R32UI, aux_buffer_));
    shader.set_uniform("aux", int(AUX_TEXTURE));

    // bind the point buffer to the point texture
    GL_CALL(glActiveTexture(GL_TEXTURE0 + POINT_TEXTURE));
    GL_CALL(glBindTexture(GL_TEXTURE_BUFFER, point_texture_));
    GL_CALL(glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, point_buffer_));
    shader.set_uniform("points", int(POINT_TEXTURE));

    // bind the group buffer to the group texture
    GL_CALL(glActiveTexture(GL_TEXTURE0 + GROUP_TEXTURE));
    GL_CALL(glBindTexture(GL_TEXTURE_BUFFER, group_texture_));
    GL_CALL(glTexBuffer(GL_TEXTURE_BUFFER, GL_R32UI, group_buffer_));
    shader.set_uniform("group", int(GROUP_TEXTURE));

    // draw
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, point_buffer_));
    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 3 * n_triangles_));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
  }

 private:
  GLuint point_buffer_;
  GLuint point_texture_;
  GLuint aux_buffer_;
  GLuint aux_texture_;
  GLuint group_buffer_;
  GLuint group_texture_;
};

class ShaderLibrary2 {
 public:
  ShaderLibrary2(const std::string& base) : base_(base) {}
  void create() {
    std::string version = "#version " +
                          std::to_string(WINGS360_GL_VERSION_MAJOR) +
                          std::to_string(WINGS360_GL_VERSION_MINOR) + "0";
    add("triangles-3d-q1-p0", "triangles-3d-q1", false, false,
        {version, "#define ORDER 0"});
    add("triangles-3d-q1-p1", "triangles-3d-q1", false, false,
        {version, "#define ORDER 1"});
    add("triangles-4d-q1-p0", "triangles-4d-q1", true, false, {version});
    add("tet-4d-q1-p0", "tet-4d-q1", true, false, {version});
  }

  void add(const std::string& name, const std::string& prefix,
           bool with_geometry, bool with_tessellation,
           const std::vector<std::string>& macros = {}) {
    shaders_.insert({name, ShaderProgram()});
    shaders_[name].set_source(base_, prefix, with_geometry, with_tessellation,
                              macros);
  }

  ShaderProgram& operator[](const std::string& name) {
    assert(shaders_.find(name) != shaders_.end());
    return shaders_.at(name);
  }

 private:
  std::string base_;
  std::map<std::string, ShaderProgram> shaders_;
};

class MeshScene : public wings::Scene {
 public:
  MeshScene(const Mesh& mesh)
      : mesh_(mesh), shaders_(std::string(WINGS_SOURCE_DIR) + "/apps/wings/") {
    context_ =
        wings::RenderingContext::create(wings::RenderingContextType::kOpenGL);
    context_->print();
    shaders_.create();
    write();
  }

  void write() {
    context_->make_context_current();

    if (mesh_.vertices().dim() == 3)
      bvh_ = std::make_unique<BoundingVolumeHierarchy<BVHTriangle>>(&mesh_);
    else if (mesh_.vertices().dim() == 4) {
      bvh_ = std::make_unique<BoundingVolumeHierarchy<BVHTet>>(&mesh_);
      auto* bvh_4d = static_cast<BoundingVolumeHierarchy<BVHTet>*>(bvh_.get());
      for (size_t k = 0; k < mesh_.tetrahedra().n(); k++) {
        BVHTet tet(mesh_, k, mesh_.tetrahedra().group(k));
        bvh_4d->add(tet);
      }
    } else
      NOT_IMPLEMENTED;

    groups_.clear();
    auto add_topology = [&](const std::string& name, auto& topology) {
      using T = typename std::decay_t<decltype(topology)>::type;
      if (topology.n() == 0) return;
      std::set<int> groups;
      for (size_t k = 0; k < topology.n(); k++) {
        groups.insert(topology.group(k));
        groups_.insert(topology.group(k));
      }
      if (mesh_.vertices().dim() <= 3) {
        primitives_.push_back(
            std::make_unique<LinearPrimitive3d<T>>(name, mesh_, *bvh_));
      } else {
        primitives_.push_back(std::make_unique<LinearPrimitive4d<T>>(
            points_, name, mesh_, *bvh_));
      }
    };

    // write the primitives
    add_topology("Triangles", mesh_.triangles());
    add_topology("Quads", mesh_.quads());
    add_topology("Tetrahedra", mesh_.tetrahedra());
    //    add_topology("Prisms", mesh_.prisms());
    //    add_topology("Pyramids", mesh_.pyramids());
    int ming = *std::min_element(groups_.begin(), groups_.end());
    int maxg = *std::max_element(groups_.begin(), groups_.end());
    LOGF("Found {} groups, min = {}, max = {}", groups_.size(), ming, maxg);
    hidden_.resize(maxg + 1, false);

    bvh_->build();

    if (mesh_.vertices().dim() == 4) {
      points_.write(mesh_.vertices());
    }

    // write the point data
    int dim = mesh_.vertices().dim();
    for (size_t i = 0; i < mesh_.vertices().n(); i++) {
      for (int d = 0; d < dim; d++) {
        float x = mesh_.vertices()[i][d];
        if (x < xmin_[d]) xmin_[d] = x;
        if (x > xmax_[d]) xmax_[d] = x;
      }
    }

    // generate initial buffer & texture for the colormap
    GL_CALL(glGenBuffers(1, &colormap_buffer_));
    GL_CALL(glGenTextures(1, &colormap_texture_));
    change_colormap("blue-white-red");

    // generate initial buffer and texture for the hidden sampler
    GL_CALL(glGenBuffers(1, &hidden_buffer_));
    GL_CALL(glGenTextures(1, &hidden_texture_));
    write_hidden();
  }

  void change_colormap(const std::string& name) {
    index_t n_color = 256 * 3;
    const float* colormap = nullptr;
    if (name == "giraffe") colormap = colormaps::color_giraffe;
    if (name == "viridis") colormap = colormaps::color_viridis;
    if (name == "blue-white-red") colormap = colormaps::color_bwr;
    if (name == "blue-green-red") colormap = colormaps::color_bgr;
    if (name == "jet") colormap = colormaps::color_jet;
    if (name == "hot") colormap = colormaps::color_hot;
    if (name == "hsv") colormap = colormaps::color_hsv;
    ASSERT(colormap != nullptr);

    GL_CALL(glBindBuffer(GL_TEXTURE_BUFFER, colormap_buffer_));
    GL_CALL(glBufferData(GL_TEXTURE_BUFFER, sizeof(GLfloat) * n_color, colormap,
                         GL_STATIC_DRAW));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + COLORMAP_TEXTURE));
    GL_CALL(glBindTexture(GL_TEXTURE_BUFFER, colormap_texture_));
    GL_CALL(glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, colormap_buffer_));
  }

  void write_hidden() {
    size_t n_groups = *std::max_element(groups_.begin(), groups_.end());
    std::vector<GLint> hidden(n_groups + 1, 0);
    for (size_t k = 0; k < hidden_.size(); k++) {
      if (hidden_[k]) hidden[k] = 1;
    }
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, hidden_buffer_));
    GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(GLint) * hidden.size(),
                         hidden.data(), GL_STATIC_DRAW));
    GL_CALL(glActiveTexture(GL_TEXTURE0 + HIDDEN_TEXTURE));
    GL_CALL(glBindTexture(GL_TEXTURE_BUFFER, hidden_texture_));
    GL_CALL(glTexBuffer(GL_TEXTURE_BUFFER, GL_R32I, hidden_buffer_));
  }

  void center_view(ClientView& view, const vec3f& point) {
    vec4f p(point.data(), 3);
    p[3] = 1.0;
    vec3f q = (view.model_matrix * p).xyz();
    float len = length(view.center - view.eye);
    vec3f dir = unit_vector(view.center - view.eye);
    view.center = q;
    view.eye = view.center - len * dir;

    const vec3f up = {0, 1, 0};
    view.view_matrix = glm::lookat(view.eye, view.center, up);

    view.center_translation.eye();
    view.center_translation(0, 3) = view.center[0];
    view.center_translation(1, 3) = view.center[1];
    view.center_translation(2, 3) = view.center[2];
  }

  void center_view(ClientView& view) {
    if (!view.picked) return;
    if (mesh_.vertices().dim() == 3)
      center_view(view, view.picked->center());
    else if (mesh_.vertices().dim() == 4) {
      auto cell = view.picked->cell();
      vec4f p{0, 0, 0, 0};
      for (int j = 0; j < 4; j++) {
        vec4f pj(mesh_.vertices()[mesh_.tetrahedra()(cell, j)]);
        p = p + pj;
      }
      vec3f q = project4d(0.25f * p, view.hyperdir);
      center_view(view, q);
    } else
      NOT_POSSIBLE;
  }

  Intersection raycast(const ClientView& view, float pixel_x, float pixel_y) {
    float h = 2.0 * std::tan(view.fov / 2.0);
    float w = h * float(view.canvas.width) / float(view.canvas.height);
    float x = -w / 2 + w * (pixel_x + 0.5) / view.canvas.width;
    float y = -h / 2 + h * (1 - (pixel_y + 0.5) / view.canvas.height);

    auto transformation = glm::inverse(view.view_matrix * view.model_matrix);
    Ray<3> ray3d({0, 0, 0}, {x, y, -1}, transformation);

    if (mesh_.vertices().dim() == 4) {
      int dim = view.hyperdir;
      vec4f origin4d = lift4d(ray3d.origin, dim, view.hypercenter[dim]);
      vec4f direction4d = lift4d(ray3d.direction, dim, 0.0f);  // 1e-5f);
      Ray<4> ray4d(origin4d, direction4d);
      return bvh_->intersect(ray4d, hidden_);
    }

    return bvh_->intersect(ray3d, hidden_);
  }

  bool render(const wings::ClientInput& input, int client_idx,
              std::string* msg) {
    ClientView& view = view_[client_idx];
    bool updated = false;
    switch (input.type) {
      case wings::InputType::MouseMotion: {
        if (input.dragging) {
          if (!input.modifier) {
            double dx = (view.x - input.x) / view.canvas.width;
            double dy = -(view.y - input.y) / view.canvas.height;
            mat4f R =
                view.center_translation * view.translation_matrix *
                glm::rotation(dx, dy) *
                glm::inverse(view.translation_matrix * view.center_translation);
            view.model_matrix = R * view.model_matrix;
          } else {
            double dx = -(view.x - input.x) / view.canvas.width;
            double dy = (view.y - input.y) / view.canvas.height;
            dx *= view.size;
            dy *= view.size;
            mat4f T = glm::translation(dx, dy);
            view.translation_matrix = T * view.translation_matrix;
            view.model_matrix = T * view.model_matrix;
          }
          updated = true;
        } else if (view.hover_highlight) {
          if (view.hover_highlight) {
            auto ixn = raycast(view, input.x, input.y);
            if (ixn.elem) {
              selected_group_ = ixn.elem->group();
              selected_cell_ = ixn.elem->cell();
              updated = true;
            } else {
              selected_group_ = -1;
              selected_cell_ = -1;
            }
          }
        }
        view.x = input.x;
        view.y = input.y;
        break;
      }
      case wings::InputType::DoubleClick: {
        auto ixn = raycast(view, input.x, input.y);
        if (ixn.elem) {
          selected_group_ = ixn.elem->group();
          selected_cell_ = ixn.elem->cell();
          std::string info = fmt::format("*Picked cell {} in group {}",
                                         selected_cell_, selected_group_);
          LOG << info;
          *msg = info;
          view.picked = ixn.elem;
          updated = true;
        } else {
          selected_group_ = -1;
          selected_cell_ = -1;
        }
        updated = true;
        break;
      }
      case wings::InputType::KeyValueInt: {
        updated = true;
        if (input.key == 'Q')
          quality_ = input.ivalue;
        else if (input.key == 'n')
          view.active["Nodes"] = input.ivalue > 0;
        else if (input.key == 'v')
          view.active["Points"] = input.ivalue > 0;
        else if (input.key == 'e')
          view.active["Lines"] = input.ivalue > 0;
        else if (input.key == 't')
          view.active["Triangles"] = input.ivalue > 0;
        else if (input.key == 'T')
          view.active["Tetrahedra"] = input.ivalue > 0;
        else if (input.key == 'p')
          view.active["Polygons"] = input.ivalue > 0;
        else if (input.key == 'y')
          view.active["Prisms"] = input.ivalue > 0;
        else if (input.key == 'Y')
          view.active["Pyramids"] = input.ivalue > 0;
        else if (input.key == 'P')
          view.active["Polyhedra"] = input.ivalue > 0;
        else if (input.key == 'a')
          view.transparency = 0.01 * input.ivalue;
        else if (input.key == 'w')
          view.show_wireframe = input.ivalue > 0;
        else if (input.key == 'W') {
          int w = input.ivalue;
          view.canvas.resize(w, view.canvas.height);
          view.projection_matrix = wings::glm::perspective(
              view.fov, float(w) / float(view.canvas.height), view.near,
              view.far);
          // save the width for the scene to write the image
          view.canvas.width = w;
          width_ = w;
        } else if (input.key == 'L') {
          view.lighting = input.ivalue > 0;
        } else if (input.key == 'H') {
          int h = input.ivalue;
          view.canvas.resize(view.canvas.width, h);
          view.projection_matrix = wings::glm::perspective(
              view.fov, float(view.canvas.width) / float(h), view.near,
              view.far);
          // save the height for the scene to write the image
          view.canvas.height = h;
          height_ = h;
        } else if (input.key == 'D') {
          view.hyperdir = input.ivalue - 1;
          view.basis_projector = get_basis_projector(view.hyperdir);
          view.hypernormal = {0, 0, 0, 0};
          view.hypernormal[view.hyperdir] = -1;
          updated = true;
        } else {
          updated = false;
        }
        break;
      }
      case wings::InputType::KeyValueStr: {
        if (input.key == 'c') {
          bool active = view.plane.active;
          view.plane.active = input.svalue[0] != '0';
          if (view.plane.active) {
            if (!active) *msg = "*Clipping activated.";
            int idx = (input.svalue[0] - '0') - 1;
            view.plane.dimension = idx % 3;
            view.plane.direction = idx < 3 ? 1 : -1;
            view.plane.visible = (input.svalue[1] - '0') > 0;
            view.plane.distance = std::atoi(&input.svalue[2]);
            view.plane.update();
          } else {
            if (active) *msg = "*Clipping deactivated.";
          }
          updated = true;
        } else if (input.key == 'x') {
          bvh_->clear();
          for (auto& prim : primitives_) prim->write(&view.plane);
          bvh_->build();
          updated = true;
        } else if (input.key == 'C') {  // colormap change
          change_colormap(input.svalue);
          updated = true;
        } else if (input.key == 'p') {
          center_view(view);
          updated = true;
        } else if (input.key == 'f') {
          view.field_mode = (view.field_mode + 1) % 4;
          updated = true;
        } else if (input.key == 'h') {
          view.hover_highlight = !view.hover_highlight;
        } else if (input.key == 's') {
          if (selected_group_ >= 0) {
            hidden_[selected_group_] = true;
            hidden_order_.push_back(selected_group_);
            write_hidden();
            updated = true;
          }
        } else if (input.key == 'S') {
          if (hidden_order_.size() > 0) {
            int last = hidden_order_.back();
            LOGF("Show {}", last);
            hidden_order_.pop_back();
            selected_group_ = -1;
            hidden_[last] = false;
            write_hidden();
            updated = true;
          }
        } else if (input.key == 'H') {
          // scale to [-1, 1]
          float d = std::atof(input.svalue) / 100.f;

          // just so it doesn't disappear at +1 or -1 due to precision
          if (d == 1.0f) d = 0.9999f;
          if (d == -1.0f) d = -0.9999f;

          // scale to [-aabb.min, aabb.max]
          view.hypercenter = {0, 0, 0, 0};
          float l = xmax_[view.hyperdir] - xmin_[view.hyperdir];
          view.hypercenter[view.hyperdir] =
              xmin_[view.hyperdir] + l * (d + 1) / 2;
          updated = true;
        } else if (input.key == 'T' || input.key == 't') {
          int s = input.key == 'T' ? +1 : -1;
          std::string dir(input.svalue);
          double dx[2] = {0, 0};
          int d = (dir == "x") ? 0 : 1;
          dx[d] = s * 0.1;
          mat4f T = glm::translation(dx[0], dx[1]);
          view.translation_matrix = T * view.translation_matrix;
          view.model_matrix = T * view.model_matrix;
          updated = true;
        }
        break;
      }
      case wings::InputType::Scroll: {
        // view.fov += 0.25 * (1 - input.fvalue);
        view.fov *= input.fvalue;
        if (view.fov < 1e-4) view.fov = 1e-4;
        if (view.fov > 3.12f) view.fov = 3.12f;
        view.projection_matrix = glm::perspective(
            view.fov, float(view.canvas.width) / float(view.canvas.height),
            view.near, view.far);
        updated = true;
        break;
      }
      default:
        break;
    }
    if (!updated) return false;

    // write shader uniforms
    view.canvas.bind();
    GL_CALL(glViewport(0, 0, view.canvas.width, view.canvas.height));
    GL_CALL(glClearColor(1.0f, 1.0f, 1.0f, 1.0f));
    GL_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_CLAMP);
    glEnable(GL_MULTISAMPLE);
    float alpha = view.transparency;
    int u_lighting = view.lighting;
    if (alpha < 1.0) {
      glDisable(GL_CULL_FACE);
      glDepthMask(GL_FALSE);
      glDepthFunc(GL_LEQUAL);
      u_lighting = 0;
    }

    // compute the matrices
    mat4f model_view_matrix = view.view_matrix * view.model_matrix;
    mat4f mvp_matrix = view.projection_matrix * model_view_matrix;
    wings::mat4f normal_matrix =
        wings::glm::inverse(wings::glm::transpose(model_view_matrix));

    // bind which attributes we want to draw
    GL_CALL(glBindVertexArray(view.vertex_array));

    int dim = mesh_.vertices().dim();
    for (size_t k = 0; k < primitives_.size(); k++) {
      const auto& primitive = *primitives_[k];
      if (!view.active[primitive.name()]) continue;

      // select shader by primitive type and field order
      const std::string prefix =
          (primitive.name() == "Tetrahedra" && dim == 4) ? "tet" : "triangles";
      const std::string name = prefix + "-" + std::to_string(dim) + "d-q1-p0";
      ShaderProgram& shader = shaders_[name];
      shader.use();

      // bind the desired colormap
      glActiveTexture(GL_TEXTURE0 + COLORMAP_TEXTURE);
      GL_CALL(glBindTexture(GL_TEXTURE_BUFFER, colormap_texture_));
      shader.set_uniform("colormap", int(COLORMAP_TEXTURE));

      // set the group colors uniform
      auto location = glGetUniformLocation(shader.handle(), "u_group_palette");
      if (location >= 0) {
        GL_CALL(glUniform3fv(location, 12, colormaps::color_groups));
      }

      glActiveTexture(GL_TEXTURE0 + HIDDEN_TEXTURE);
      GL_CALL(glBindTexture(GL_TEXTURE_BUFFER, hidden_texture_));
      shader.set_uniform("hidden", int(HIDDEN_TEXTURE));

      // // set the uniforms for the shader
      shader.set_uniform("u_edges", view.show_wireframe);
      shader.set_uniform("u_lighting", u_lighting);
      shader.set_uniform("u_alpha", alpha);

      shader.set_uniform("u_ModelViewProjectionMatrix", mvp_matrix);
      shader.set_uniform("u_ModelViewMatrix", model_view_matrix);
      shader.set_uniform("u_NormalMatrix", normal_matrix);

      shader.set_uniform("u_field_mode", view.field_mode);
      shader.set_uniform("u_min_group", int(*groups_.begin()));
      shader.set_uniform("u_max_group", int(*groups_.rbegin() + 1));
      shader.set_uniform("u_max_cell", int(primitive.max_cell()));
      shader.set_uniform("u_umin", 0.0f);
      shader.set_uniform("u_umax", 1.0f);
      shader.set_uniform("u_width", view.canvas.width);
      shader.set_uniform("u_height", view.canvas.height);
      shader.set_uniform("u_selected_group", selected_group_);
      shader.set_uniform("u_selected_cell", selected_cell_);

      primitive.draw(shader, view);
    }

    if (view.plane.visible)
      view.plane.draw(view.model_matrix, view.view_matrix,
                      view.projection_matrix);

    // read the pixels
    view.canvas.save(*this);

    return true;
  }

  void onconnect() {
    // set up the view
    view_.emplace_back();
    ClientView& view = view_.back();

    view.eye = {0, 0, 4};
    view.center = {0, 0, 0};
    view.size = 1.0;

    view.center_translation.eye();
    view.inverse_center_translation.eye();
    for (int d = 0; d < 3; d++) {
      view.center_translation(d, 3) = view.center[d];
      view.inverse_center_translation(d, 3) = -view.center[d];
    }

    view.model_matrix.eye();
    vec3f up{0, 1, 0};
    view.view_matrix = glm::lookat(view.eye, view.center, up);
    view.projection_matrix = glm::perspective(
        view.fov, float(view.canvas.width) / float(view.canvas.height),
        view.near, view.far);
    view.translation_matrix.eye();

    // vertex arrays are not shared between OpenGL contexts in different
    // threads (buffers & textures are though). Each client runs in a separate
    // thread so we need to create a vertex array upon each client connection.
    GL_CALL(glGenVertexArrays(1, &view.vertex_array));
    view.plane.initialize();
    AABB<3> aabb;
    aabb.min() = {-1, -1, -1};
    aabb.max() = {1, 1, 1};
    view.plane.define(aabb);
    view.field_mode = 0;
  }

 private:
  const Mesh& mesh_;
  std::vector<ClientView> view_;
  vec4f xmin_, xmax_;
  PointTexture points_;

  GLuint colormap_texture_;
  GLuint colormap_buffer_;
  GLuint hidden_texture_;
  GLuint hidden_buffer_;

  std::vector<std::unique_ptr<BasePrimitive>> primitives_;
  ShaderLibrary2 shaders_;
  std::set<int> groups_;  // total groups
  std::unique_ptr<BoundingVolumeHierarchyBase> bvh_;
  std::vector<bool> hidden_;
  std::vector<int> hidden_order_;
  int selected_group_{-1};
  int selected_cell_{-1};
};

class Viewer {
 public:
  Viewer(const Mesh& mesh, int port);
  ~Viewer();

 private:
  std::unique_ptr<MeshScene> scene_;
  std::unique_ptr<wings::RenderingServer> renderer_;
};

Viewer::Viewer(const Mesh& mesh, int port) {
  scene_ = std::make_unique<MeshScene>(mesh);
  renderer_ = std::make_unique<wings::RenderingServer>(*scene_, port);
}

Viewer::~Viewer() {}

}  // namespace wings

int main(int argc, const char** argv) {
  int ws_port = 7681;
  if (argc > 2) ws_port = std::atoi(argv[2]);

  wings::Mesh mesh(3);
  read_mesh(argv[1], mesh);

  // calculate bounding box
  wings::vec4f xmin{1e20, 1e20, 1e20, 1e20}, xmax = -1.0f * xmin;
  for (size_t k = 0; k < mesh.vertices().n(); k++) {
    for (int d = 0; d < mesh.vertices().dim(); d++) {
      auto x = mesh.vertices()[k][d];
      if (x < xmin[d]) xmin[d] = x;
      if (x > xmax[d]) xmax[d] = x;
    }
  }

  // scale and center vertices
  float lmax = xmax[0] - xmin[0];
  for (int d = 1; d < mesh.vertices().dim(); d++) {
    lmax = std::max(lmax, xmax[d] - xmin[d]);
  }
  wings::vec4f center = 0.5f * (xmin + xmax);
  for (size_t k = 0; k < mesh.vertices().n(); k++) {
    for (int d = 0; d < mesh.vertices().dim(); d++) {
      auto x = mesh.vertices()[k][d];
      mesh.vertices()[k][d] = 2.0 * (x - center[d]) / lmax;
    }
  }

  wings::Viewer viewer(mesh, ws_port);

  return 0;
}