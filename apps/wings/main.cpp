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
  AUX_TEXTURE = 7
};

class BaseElementGroup {
 public:
  BaseElementGroup(const std::string& name, const Mesh& mesh, int group)
      : name_(name), mesh_(mesh), group_(group) {}
  virtual ~BaseElementGroup() {}
  virtual void draw(ShaderProgram&, bool) const = 0;
  virtual void write(const GLClipPlane*) = 0;

  float umin() const { return umin_; }
  float umax() const { return umax_; }

  const std::string& name() const { return name_; }
  int group() const { return group_; }
  int n_cells() const { return n_cells_; }
  int n_triangles() const { return n_triangles_; }
  int max_cell() const { return max_cell_; }

 protected:
  std::string name_;
  const Mesh& mesh_;
  int group_{-1};
  float umin_;
  float umax_;
  int n_cells_;
  int n_triangles_;
  int max_cell_;
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

class CurvedElementGroup {};
class SpacetimeElementGroup {};

template <typename T>
class LinearElementGroup : public BaseElementGroup {
 public:
  LinearElementGroup(const std::string& name, const Mesh& mesh, int group)
      : BaseElementGroup(name, mesh, group) {
    GL_CALL(glGenBuffers(1, &point_buffer_));
    GL_CALL(glGenBuffers(1, &altitude_buffer_));
    GL_CALL(glGenBuffers(1, &aux_buffer_));
    GL_CALL(glGenTextures(1, &aux_texture_));
    write(nullptr);
  }

  ~LinearElementGroup() {
    GL_CALL(glDeleteBuffers(1, &point_buffer_));
    GL_CALL(glDeleteBuffers(1, &altitude_buffer_));
    GL_CALL(glDeleteBuffers(1, &aux_buffer_));
    GL_CALL(glDeleteTextures(1, &aux_texture_));
  }

  void write(const GLClipPlane* plane) {
    if (T::dimension == 3 and !plane) return;

    int dim = mesh_.vertices().dim();
    const auto& topology = mesh_.get<T>();

    std::vector<GLfloat> points;
    points.reserve(topology.n() * 9 * VisualizationTriangles<T>::n);
    std::vector<GLuint> aux;
    aux.reserve(topology.n() * VisualizationTriangles<T>::n);
    std::vector<GLfloat> altitude;
    altitude.reserve(topology.n() * 3 * VisualizationTriangles<T>::n);

    vec3f center, normal;
    if (plane) {
      plane->get(center, normal);
    }

    max_cell_ = topology.n();
    n_triangles_ = 0;
    for (size_t k = 0; k < topology.n(); k++) {
      if (topology.group(k) != group_) continue;

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
        for (int i = 0; i < 3; i++) {
          auto vtx = topology(k, VisualizationTriangles<T>::triangles[j][i]);
          for (int d = 0; d < dim; d++) {
            triangle[i][d] = mesh_.vertices()[vtx][d];
            points.push_back(triangle[i][d]);
          }
          if (dim == 2) points.push_back(0.0);
        }

        float a = length(triangle[2] - triangle[1]);
        float b = length(triangle[0] - triangle[2]);
        float c = length(triangle[1] - triangle[0]);
        float s = 0.5 * (a + b + c);
        float area = std::sqrt(s * (s - a) * (s - b) * (s - c));
        altitude.push_back(2 * area / a);
        altitude.push_back(2 * area / b);
        altitude.push_back(2 * area / c);
      }
    }
    points.shrink_to_fit();
    altitude.shrink_to_fit();
    aux.shrink_to_fit();

    // write the coordinates
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, point_buffer_));
    GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * points.size(),
                         points.data(), GL_STATIC_DRAW));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    // write the altitudes
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, altitude_buffer_));
    GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * altitude.size(),
                         altitude.data(), GL_STATIC_DRAW));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    // write aux data: edge visibility info and cell number
    GL_CALL(glBindBuffer(GL_TEXTURE_BUFFER, aux_buffer_));
    GL_CALL(glBufferData(GL_TEXTURE_BUFFER, sizeof(GLuint) * aux.size(),
                         aux.data(), GL_STATIC_DRAW));
    GL_CALL(glBindBuffer(GL_TEXTURE_BUFFER, 0));
  }

  void draw(ShaderProgram& shader, bool interactive) const {
    shader.use();
    if (n_triangles_ == 0) return;

    // bind the aux buffer to the aux texture
    GL_CALL(glActiveTexture(GL_TEXTURE0 + AUX_TEXTURE));
    GL_CALL(glBindTexture(GL_TEXTURE_BUFFER, aux_texture_));
    GL_CALL(glTexBuffer(GL_TEXTURE_BUFFER, GL_R32UI, aux_buffer_));
    shader.set_uniform("aux", int(AUX_TEXTURE));

    // enable the position attribute
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, point_buffer_));
    GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0));
    GL_CALL(glEnableVertexAttribArray(0));

    // enable the altitude attribute
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, altitude_buffer_));
    GL_CALL(glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, 0));
    GL_CALL(glEnableVertexAttribArray(1));

    // draw
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, point_buffer_));
    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 3 * n_triangles_));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
  }

 private:
  GLuint point_buffer_;
  GLuint altitude_buffer_;
  GLuint aux_buffer_;
  GLuint aux_texture_;
};

class ShaderLibrary2 {
 public:
  ShaderLibrary2(const std::string& base) : base_(base) {}
  void create() {
    std::string version = "#version " +
                          std::to_string(WINGS360_GL_VERSION_MAJOR) +
                          std::to_string(WINGS360_GL_VERSION_MINOR) + "0";
    add("triangles-q1-p0", "triangles-q1", false, false,
        {version, "#define ORDER 0"});
    add("triangles-q1-p1", "triangles-q1", false, false,
        {version, "#define ORDER 1"});
  }

  void add(const std::string& name, const std::string& prefix,
           bool with_geometry, bool with_tessellation,
           const std::vector<std::string>& macros = {}) {
    shaders_.insert({name, ShaderProgram()});
    shaders_[name].set_source(base_, prefix, with_geometry, with_tessellation,
                              macros);
  }

  ShaderProgram& operator[](const std::string& name) {
    WINGS_ASSERT(shaders_.find(name) != shaders_.end())
        << "could not find shader " << name;
    return shaders_.at(name);
  }

 private:
  std::string base_;
  std::map<std::string, ShaderProgram> shaders_;
};

class MeshScene : public wings::Scene {
  struct ClientView {
    mat4f model_matrix;
    mat4f view_matrix;
    mat4f projection_matrix;
    mat4f center_translation, inverse_center_translation;
    mat4f translation_matrix;
    vec3f center, eye;
    float size{1.0};
    float fov{M_PI / 4.0};
    double x{0}, y{0};
    GLuint vertex_array;
    std::unordered_map<std::string, bool> active = {
        {"Points", false},     {"Nodes", true},   {"Lines", false},
        {"Triangles", true},   {"Quads", true},   {"Polygons", true},
        {"Tetrahedra", false}, {"Prisms", false}, {"Pyramids", false},
        {"Polyhedra", false}};
    int show_wireframe{1};
    float transparency{1.0};
    int lighting{1};
    bool culling{false};
    GLClipPlane plane;
    const PickableObject* picked{nullptr};
    int field_mode{0};
    int field_index{0};
    glCanvas canvas{800, 600, true};
    float near{1e-1};
    float far{100};
  };

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

    // // generate a texture to hold the mesh coordinates
    // GL_CALL(glActiveTexture(GL_TEXTURE0 + POINT_TEXTURE));
    // GL_CALL(glBindTexture(GL_TEXTURE_BUFFER, point_texture_));
    // GL_CALL(glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, point_buffer_));

    groups_.clear();
    auto add_topology = [&](const std::string& name, auto& topology) {
      using T = typename std::decay_t<decltype(topology)>::type;
      if (topology.n() == 0) return;
      std::set<int> groups;
      for (size_t k = 0; k < topology.n(); k++) {
        groups.insert(topology.group(k));
        groups_.insert(topology.group(k));
      }
      for (auto group : groups) {
        primitives_.push_back(
            std::make_unique<LinearElementGroup<T>>(name, mesh_, group));
      }
    };

    // write the primitives
    add_topology("Triangles", mesh_.triangles());
    add_topology("Quads", mesh_.quads());
    add_topology("Tetrahedra", mesh_.tetrahedra());
    //   add_topology("Prisms", mesh_.prisms());
    //   add_topology("Pyramids", mesh_.pyramids());
    LOGF("Found {} groups.", groups_.size());

    // generate initial buffer & texture for the colormap
    GL_CALL(glGenBuffers(1, &colormap_buffer_));
    GL_CALL(glGenTextures(1, &colormap_texture_));
    change_colormap("blue-white-red");
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
    vec4f point;
    for (const auto& p : view.picked->points) point = point + p;
    point = (1.0f / view.picked->points.size()) * point;
    center_view(view, point.xyz());
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
        }
        view.x = input.x;
        view.y = input.y;
        break;
      }
      case wings::InputType::DoubleClick: {
        view.picked = nullptr;  // pick(input.x, input.y, view);
        if (view.picked) {
          std::string info = fmt::format("*picked element {} ({}): (",
                                         view.picked->index, view.picked->name);
          size_t i = 0;
          for (auto p : view.picked->nodes) {
            info += std::to_string(p);
            if (i + 1 < view.picked->nodes.size())
              info += ", ";
            else
              info += ")";
            i++;
          }
          *msg = info;
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
        } else if (input.key == 'H') {
          int h = input.ivalue;
          view.canvas.resize(view.canvas.width, h);
          view.projection_matrix = wings::glm::perspective(
              view.fov, float(view.canvas.width) / float(h), view.near,
              view.far);
          // save the height for the scene to write the image
          view.canvas.height = h;
          height_ = h;
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
            if (!active) *msg = "*clipping activated";
            int idx = (input.svalue[0] - '0') - 1;
            view.plane.dimension = idx % 3;
            view.plane.direction = idx < 3 ? 1 : -1;
            view.plane.visible = (input.svalue[1] - '0') > 0;
            view.plane.distance = std::atoi(&input.svalue[2]);
            view.plane.update();
          } else {
            if (active) *msg = "*clipping deactivated";
          }
          updated = true;
        } else if (input.key == 'x') {
          for (auto& prim : primitives_) prim->write(&view.plane);
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
        }
        break;
      }
      case wings::InputType::Scroll: {
        view.fov += 0.25 * (1 - input.fvalue);
        if (view.fov < 0.01) view.fov = 0.01;
        if (view.fov > 3.0f) view.fov = 3.0f;
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
    glEnable(GL_POLYGON_SMOOTH);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1, 1);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_CLAMP);
    glEnable(GL_MULTISAMPLE);
    float alpha = view.transparency;
    int u_lighting = view.lighting;
    if (alpha < 1.0) {
      glDisable(GL_CULL_FACE);
      // glDepthMask(GL_FALSE);
      // glDepthFunc(GL_LEQUAL);
      u_lighting = 0;
    }

    // compute the matrices
    mat4f model_view_matrix = view.view_matrix * view.model_matrix;
    mat4f mvp_matrix = view.projection_matrix * model_view_matrix;

    // bind which attributes we want to draw
    GL_CALL(glBindVertexArray(view.vertex_array));

    for (size_t k = 0; k < primitives_.size(); k++) {
      const auto& primitive = *primitives_[k];
      if (!view.active[primitive.name()]) continue;

      // select shaderby primitive type and field order
      ShaderProgram& shader = shaders_["triangles-q1-p0"];
      shader.use();

      // bind the desired colormap
      glActiveTexture(GL_TEXTURE0 + COLORMAP_TEXTURE);
      GL_CALL(glBindTexture(GL_TEXTURE_BUFFER, colormap_texture_));
      shader.set_uniform("colormap", int(COLORMAP_TEXTURE));

      // // set the uniforms for the shader
      shader.set_uniform("u_edges", view.show_wireframe);
      shader.set_uniform("u_lighting", u_lighting);
      shader.set_uniform("u_alpha", alpha);

      shader.set_uniform("u_ModelViewProjectionMatrix", mvp_matrix);
      shader.set_uniform("u_ModelViewMatrix", model_view_matrix);

      shader.set_uniform("u_field_mode", view.field_mode);
      shader.set_uniform("u_group", primitive.group());
      shader.set_uniform("u_min_group", int(*groups_.begin()));
      shader.set_uniform("u_max_group", int(*groups_.rbegin() + 1));
      shader.set_uniform("u_max_cell", int(primitive.max_cell()));
      shader.set_uniform("u_umin", 0.0f);
      shader.set_uniform("u_umax", 1.0f);

      shader.set_uniform("u_PixelScale", float(0.5 * view.canvas.height) /
                                             std::tan(view.fov / 2));

      if (view.picked) {
        // if (view.picked->name == primitive.title()) {
        //   shader.set_uniform("u_picking", int(view.picked->index));
        // }
      }
      primitive.draw(shader, false);
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

    view.eye = {0, 0, 5};
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
    AABB aabb;
    aabb.min = {-1, -1, -1};
    aabb.max = {1, 1, 1};
    view.plane.define(aabb);
    view.field_mode = 0;
  }

 private:
  const Mesh& mesh_;
  std::vector<ClientView> view_;

  GLuint colormap_texture_;
  GLuint colormap_buffer_;

  std::vector<std::unique_ptr<BaseElementGroup>> primitives_;
  ShaderLibrary2 shaders_;
  std::set<int> groups_;  // total groups
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