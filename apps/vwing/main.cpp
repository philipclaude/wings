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
#include "texture.h"
#include "util.h"

namespace wings {

enum BufferOption {
  JAGGED_TEXTURE = 1,
  RECTANGULAR_TEXTURE = 2,
  NOT_TEXTURED = 0
};

enum TextureIndex {
  POINT_TEXTURE = 0,
  NORMAL_TEXTURE = 1,
  TEXCOORD_TEXTURE = 2,
  COLORMAP_TEXTURE = 3,
  INDEX_TEXTURE = 4,
  FIRST_TEXTURE = 5,
  LENGTH_TEXTURE = 6,
  FIELD_TEXTURE = 7,
  IMAGE_TEXTURE = 8,
  FONT_TEXTURE = 9,
  POINT_VISIBILITY = 10
};

class GLPrimitive {
 public:
  template <typename T>
  GLPrimitive(const std::string& title, const Topology<T>& topology,
              const std::string& name, enum BufferOption option)
      : title_(title),
        name_(name),
        option_(option),
        has_texture_coordinates_(false) {
    write(topology);
  }

  template <typename T>
  void write(const Topology<T>& topology) {
    if (option_ == JAGGED_TEXTURE) {
      std::vector<GLuint> indices(topology.data().begin(),
                                  topology.data().end());
      GL_CALL(glGenBuffers(1, &index_buffer_));
      GL_CALL(glBindBuffer(GL_TEXTURE_BUFFER, index_buffer_));
      GL_CALL(glBufferData(GL_TEXTURE_BUFFER, sizeof(GLuint) * indices.size(),
                           indices.data(), GL_STATIC_DRAW));
      GL_CALL(glBindBuffer(GL_TEXTURE_BUFFER, 0));

      std::vector<GLuint> first(topology.first().begin(),
                                topology.first().end());
      GL_CALL(glGenBuffers(1, &first_buffer_));
      GL_CALL(glBindBuffer(GL_TEXTURE_BUFFER, first_buffer_));
      GL_CALL(glBufferData(GL_TEXTURE_BUFFER, sizeof(GLuint) * first.size(),
                           first.data(), GL_STATIC_DRAW));
      GL_CALL(glBindBuffer(GL_TEXTURE_BUFFER, 0));

      std::vector<GLuint> length(topology.length().begin(),
                                 topology.length().end());
      GL_CALL(glGenBuffers(1, &length_buffer_));
      GL_CALL(glBindBuffer(GL_TEXTURE_BUFFER, length_buffer_));
      GL_CALL(glBufferData(GL_TEXTURE_BUFFER, sizeof(GLuint) * length.size(),
                           length.data(), GL_STATIC_DRAW));
      GL_CALL(glBindBuffer(GL_TEXTURE_BUFFER, 0));

      n_draw_ = first.size();
    } else if (option_ == RECTANGULAR_TEXTURE) {
      // get all the group indices of the elements in the topology
      std::set<int> groups;
      for (size_t k = 0; k < topology.n(); k++)
        groups.insert(topology.group(k));
      std::vector<GLuint> order(topology.n());
      int count = 0;
      int igroup = 0;
      n_draw_group_.resize(groups.size());
      for (int group : groups) {
        for (size_t k = 0; k < topology.n(); k++) {
          if (topology.group(k) != group) continue;
          order[count++] = k;
        }
        n_draw_group_[igroup++] = count;
      }
      std::vector<GLuint> indices(topology.data().begin(),
                                  topology.data().end());
      GL_CALL(glGenBuffers(1, &index_buffer_));
      GL_CALL(glBindBuffer(GL_TEXTURE_BUFFER, index_buffer_));
      GL_CALL(glBufferData(GL_TEXTURE_BUFFER, sizeof(GLuint) * indices.size(),
                           indices.data(), GL_STATIC_DRAW));
      GL_CALL(glBindBuffer(GL_TEXTURE_BUFFER, 0));
      n_draw_ = topology.n();
    } else {
      std::vector<GLuint> indices(topology.data().begin(),
                                  topology.data().end());
      GL_CALL(glGenBuffers(1, &index_buffer_));
      GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_));
      GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                           sizeof(GLuint) * indices.size(), indices.data(),
                           GL_STATIC_DRAW));
      GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
      n_draw_ = indices.size();
    }
    LOG << fmt::format("wrote {} {}", n_draw_, title_);

    // generate a buffer for the field
    GL_CALL(glGenBuffers(1, &field_buffer_));
  }
  void write_field(const Field& field, int rank) {
    int n_basis = -1;
    const array2d<coord_t>* data = nullptr;
    if (title_ == "Lines") data = &(field.lines()), n_basis = field.lines().m();
    if (title_ == "Triangles")
      data = &(field.triangles()), n_basis = field.triangles().m();
    if (title_ == "Quads") data = &(field.quads()), n_basis = field.quads().m();
    if (title_ == "Polygons")
      data = &(field.polygons()), n_basis = field.polygons().m();
    if (title_ == "Prisms")
      data = &(field.prisms()), n_basis = field.prisms().m();
    if (title_ == "Pyramids")
      data = &(field.pyramids()), n_basis = field.pyramids().m();
    if (title_ == "Tetrahedra")
      data = &(field.tetrahedra()), n_basis = field.tetrahedra().m();
    if (title_ == "Polyhedra")
      data = &(field.polyhedra().faces()),
      n_basis = field.polyhedra().faces().m();
    ASSERT(data != nullptr);

    // extract the appropriate rank and buffer the data
    std::vector<GLfloat> u(data->n() * n_basis, 0.0f);
    for (size_t k = 0; k < data->n(); k++) {
      for (int j = 0; j < n_basis; j++) {
        u[k * n_basis + j] = (*data)[k][rank * n_basis + j];
      }
    }
    if (u.size() == 0) return;

    umin_ = *std::min_element(u.begin(), u.end());
    umax_ = *std::max_element(u.begin(), u.end());

    // bind the data to the buffer
    GL_CALL(glBindBuffer(GL_TEXTURE_BUFFER, field_buffer_));
    GL_CALL(glBufferData(GL_TEXTURE_BUFFER, sizeof(GLfloat) * u.size(),
                         u.data(), GL_STATIC_DRAW));
    GL_CALL(glBindBuffer(GL_TEXTURE_BUFFER, 0));
  }
  void write_texcoord(const Field& field) {
    int n_basis = -1;
    const array2d<coord_t>* data = nullptr;
    if (title_ == "Triangles")
      data = &(field.triangles()), n_basis = 3;
    else if (title_ == "Quads")
      data = &(field.quads()), n_basis = 4;
    else
      NOT_POSSIBLE;
    ASSERT(field.ranks() == 2);

    // extract the appropriate rank and buffer the data
    std::vector<GLfloat> u(data->n() * n_basis * 2, 0.0f);
    index_t m = 0;
    for (size_t k = 0; k < data->n(); k++) {
      for (int j = 0; j < n_basis; j++) {
        u[m++] = (*data)[k][0 * n_basis + j];
        u[m++] = (*data)[k][1 * n_basis + j];
      }
    }
    ASSERT(m == index_t(u.size()));

    // bind the data to the buffer
    GL_CALL(glGenBuffers(1, &texcoord_buffer_));
    GL_CALL(glBindBuffer(GL_TEXTURE_BUFFER, texcoord_buffer_));
    GL_CALL(glBufferData(GL_TEXTURE_BUFFER, sizeof(GLfloat) * u.size(),
                         u.data(), GL_STATIC_DRAW));
    GL_CALL(glBindBuffer(GL_TEXTURE_BUFFER, 0));

    has_texture_coordinates_ = true;
  }

  void draw(const ShaderProgram& shader, GLuint point_buffer,
            GLuint field_texture, GLuint texcoord_texture, GLuint index_texture,
            GLuint first_texture, GLuint length_texture) const {
    shader.use();

    // bind the field buffer to the field texture
    GL_CALL(glActiveTexture(GL_TEXTURE0 + FIELD_TEXTURE));
    GL_CALL(glBindTexture(GL_TEXTURE_BUFFER, field_texture));
    GL_CALL(glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, field_buffer_));
    shader.set_uniform("field", int(FIELD_TEXTURE));

    if (has_texture_coordinates_) {
      GL_CALL(glActiveTexture(GL_TEXTURE0 + TEXCOORD_TEXTURE));
      GL_CALL(glBindTexture(GL_TEXTURE_BUFFER, texcoord_texture));
      GL_CALL(glTexBuffer(GL_TEXTURE_BUFFER, GL_RG32F, texcoord_buffer_));
      shader.set_uniform("texcoord", int(TEXCOORD_TEXTURE));
    }

    if (option_ == JAGGED_TEXTURE) {
      GL_CALL(glActiveTexture(GL_TEXTURE0 + INDEX_TEXTURE));
      GL_CALL(glBindTexture(GL_TEXTURE_BUFFER, index_texture));
      GL_CALL(glTexBuffer(GL_TEXTURE_BUFFER, GL_R32UI, index_buffer_));
      shader.set_uniform("index", int(INDEX_TEXTURE));

      GL_CALL(glActiveTexture(GL_TEXTURE0 + FIRST_TEXTURE));
      GL_CALL(glBindTexture(GL_TEXTURE_BUFFER, first_texture));
      GL_CALL(glTexBuffer(GL_TEXTURE_BUFFER, GL_R32UI, first_buffer_));
      shader.set_uniform("first", int(FIRST_TEXTURE));

      GL_CALL(glActiveTexture(GL_TEXTURE0 + LENGTH_TEXTURE));
      GL_CALL(glBindTexture(GL_TEXTURE_BUFFER, length_texture));
      GL_CALL(glTexBuffer(GL_TEXTURE_BUFFER, GL_R32UI, length_buffer_));
      shader.set_uniform("count", int(LENGTH_TEXTURE));

      GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, point_buffer));
      GL_CALL(glDrawArrays(GL_POINTS, 0, n_draw_));
    } else if (option_ == RECTANGULAR_TEXTURE) {
      GL_CALL(glActiveTexture(GL_TEXTURE0 + INDEX_TEXTURE));
      GL_CALL(glBindTexture(GL_TEXTURE_BUFFER, index_texture));
      GL_CALL(glTexBuffer(GL_TEXTURE_BUFFER, GL_R32UI, index_buffer_));
      shader.set_uniform("index", int(INDEX_TEXTURE));

      GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, point_buffer));
      GL_CALL(glBindBuffer(GL_TEXTURE_BUFFER, index_buffer_));
      GL_CALL(glDrawArrays(GL_POINTS, 0, n_draw_));
    } else {
      GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_));
      GL_CALL(glDrawElements(GL_TRIANGLES, n_draw_, GL_UNSIGNED_INT, 0));
    }
  }
  const std::string& name() const { return name_; }
  const std::string& title() const { return title_; }

  float umin() const { return umin_; }
  float umax() const { return umax_; }

 private:
  index_t n_draw_;
  std::vector<index_t> n_draw_group_;
  std::string title_;
  std::string name_;
  BufferOption option_;
  bool has_texture_coordinates_;

  GLuint index_buffer_;
  GLuint first_buffer_;
  GLuint length_buffer_;
  GLuint field_buffer_;
  GLuint texcoord_buffer_;

  float umin_;
  float umax_;

  std::set<int> groups_;
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
    float fov{45};
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
    glCanvas canvas{800, 600};
    float near{1e-3};
    float far{1000};
    bool numbers{true};
  };

 public:
  MeshScene(const Mesh& mesh)
      : mesh_(mesh), shaders_(std::string(WINGS_SOURCE_DIR) + "/apps/vwing/") {
    context_ =
        wings::RenderingContext::create(wings::RenderingContextType::kOpenGL);
    context_->print();
    shaders_.create();
    write(&mesh_.fields());
    build_pickables();
  }

  void build_pickables() {
    pickables_.clear();
    auto add_pickables = [&](const Vertices& vertices, const auto& topology,
                             const std::string& name) {
      for (size_t k = 0; k < topology.n(); k++) {
        pickables_.emplace_back(vertices, topology, k, name);
      }
    };

    add_pickables(mesh_.vertices(), mesh_.triangles(), "Triangles");
    add_pickables(mesh_.vertices(), mesh_.quads(), "Quads");
    add_pickables(mesh_.vertices(), mesh_.polygons(), "Polygons");
    add_pickables(mesh_.vertices(), mesh_.tetrahedra(), "Tetrahedra");
    add_pickables(mesh_.vertices(), mesh_.prisms(), "Prisms");
    // add_pickables(mesh_.vertices(), mesh_.polyhedra(), "Polyhedra");
  }

  const PickableObject* pick(float x, float y, const ClientView& view) {
    // calculate the basis for the camera transformation which is needed
    // to calculate the ray direction
    vec3f g = view.center - view.eye;
    vec3f up = {0, 1, 0};
    vec3f w = -1.0f * unit_vector(g);
    vec3f u = unit_vector(cross(up, w));
    vec3f v = cross(w, u);
    mat3f basis;
    for (int d = 0; d < 3; d++) {
      basis(d, 0) = u[d];
      basis(d, 1) = v[d];
      basis(d, 2) = w[d];
    }

    // computes the 3d world coordinates of a pixel
    // can save some computation by not adding eye, but leaving it for now
    auto pixel2world = [&](double u, double v) {
      double d = length(view.center - view.eye);
      double a = double(view.canvas.width) / double(view.canvas.height);
      double h = 2.0 * d * tan(view.fov / 2.0);
      double w = a * h;

      float pu = -0.5 * w + w * u;
      float pv = -0.5 * h + h * v;
      float pw = -d;
      vec3f q = {pu, pv, pw};

      return basis * q + view.eye;
    };
    vec3f ray = unit_vector(
        pixel2world(x / view.canvas.width, 1.0 - y / view.canvas.height) -
        view.eye);

    // find the closest element
    double tmin = 1e20;
    const PickableObject* picked = nullptr;
    for (size_t k = 0; k < pickables_.size(); k++) {
      const PickableObject& object = pickables_[k];
      if (!view.active.at(object.name)) continue;
      if (!object.visible(view.plane)) continue;

      double t = object.intersection(view.eye, ray, view.model_matrix);
      if (t < tmin) {
        tmin = t;
        picked = &object;
      }
    }
    if (picked != nullptr) {
      // TODO more general element printing
      // LOG << fmt::format("picked element {}", picked->index);
    }
    return picked;
  }

  void write_point_visibility(const PickableObject* elem) {
    std::vector<GLubyte> visibility(mesh_.vertices().n(), 0);
    if (elem != nullptr) {
      for (const auto& n : elem->nodes) visibility[n] = 1;
    } else {
      std::fill(visibility.begin(), visibility.end(), 0);
    }
    GL_CALL(glGenBuffers(1, &point_visibility_buffer_));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, point_visibility_buffer_));
    GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(GLubyte) * visibility.size(),
                         visibility.data(), GL_STATIC_DRAW));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
  }

  void write(const FieldLibrary* fields = nullptr) {
    context_->make_context_current();

    // generate textures
    GL_CALL(glGenTextures(1, &point_texture_));
    GL_CALL(glGenTextures(1, &index_texture_));
    GL_CALL(glGenTextures(1, &first_texture_));
    GL_CALL(glGenTextures(1, &length_texture_));
    GL_CALL(glGenTextures(1, &field_texture_));
    GL_CALL(glGenTextures(1, &texcoord_texture_));
    GL_CALL(glGenTextures(1, &image_texture_));
    GL_CALL(glGenTextures(1, &font_texture_));
    GL_CALL(glGenTextures(1, &point_visibility_texture_));

    // write font texture
    TextureOptions tex_opts;
    std::string f = std::string(WINGS_SOURCE_DIR) + "/data/monaco-numbers.png";
    tex_opts.format = TextureFormat::kRGBA;
    tex_opts.flipy = false;
    Texture font(f, tex_opts);
    GL_CALL(glBindTexture(GL_TEXTURE_2D, font_texture_));
    GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, font.width(), font.height(),
                         0, GL_RGBA, GL_UNSIGNED_BYTE, font.data()));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    // write the point data
    std::vector<GLfloat> coordinates(3 * mesh_.vertices().n());
    for (size_t i = 0; i < mesh_.vertices().n(); i++)
      for (int d = 0; d < 3; d++) {
        float x = mesh_.vertices()[i][d];
        coordinates[3 * i + d] = x;
        if (x < aabb_.min[d]) aabb_.min[d] = x;
        if (x > aabb_.max[d]) aabb_.max[d] = x;
      }
    GL_CALL(glGenBuffers(1, &point_buffer_));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, point_buffer_));
    GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * coordinates.size(),
                         coordinates.data(), GL_STATIC_DRAW));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    // write point visibility data
    write_point_visibility(nullptr);

    coordinates.clear();
    n_nodes_ = 0;
    for (size_t i = 0; i < mesh_.vertices().n(); i++) {
      // auto* e = mesh_.vertices().entity(i);
      // if (!e || e->dim() != 0) continue;
      // n_nodes_++;
      // for (int d = 0; d < 3; d++)
      // coordinates.push_back(mesh_.vertices()[i][d]);
    }
    if (n_nodes_ > 0) {
      GL_CALL(glGenBuffers(1, &node_buffer_));
      GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, node_buffer_));
      GL_CALL(glBufferData(GL_ARRAY_BUFFER,
                           sizeof(GLfloat) * coordinates.size(),
                           coordinates.data(), GL_STATIC_DRAW));
      GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
    }

    // generate a texture to hold the mesh coordinates
    GL_CALL(glActiveTexture(GL_TEXTURE0 + POINT_TEXTURE));
    GL_CALL(glBindTexture(GL_TEXTURE_BUFFER, point_texture_));
    GL_CALL(glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, point_buffer_));

    // write the primitives (edges, triangles, quads, polygons, tets, polyhedra)
    primitives_.reserve(6);

    if (mesh_.triangles().n() > 0) {
      primitives_.emplace_back("Triangles", mesh_.triangles(), "triangles-q1",
                               RECTANGULAR_TEXTURE);
    }

    if (mesh_.quads().n() > 0) {
      primitives_.emplace_back("Quads", mesh_.quads(), "quads-q1",
                               RECTANGULAR_TEXTURE);
    }

    if (mesh_.polygons().n() > 0) {
      primitives_.emplace_back("Polygons", mesh_.polygons(), "polygons-q1",
                               JAGGED_TEXTURE);
    }

    if (mesh_.prisms().n() > 0) {
      primitives_.emplace_back("Prisms", mesh_.prisms(), "prisms-q1",
                               RECTANGULAR_TEXTURE);
    }

    if (mesh_.pyramids().n() > 0) {
      primitives_.emplace_back("Pyramids", mesh_.pyramids(), "pyramids-q1",
                               RECTANGULAR_TEXTURE);
    }

    if (mesh_.tetrahedra().n() > 0) {
      primitives_.emplace_back("Tetrahedra", mesh_.tetrahedra(),
                               "tetrahedra-q1", RECTANGULAR_TEXTURE);
    }

    if (mesh_.polyhedra().faces().n() > 0) {
      primitives_.emplace_back("Polyhedra", mesh_.polyhedra().faces(),
                               "polyhedra-q1", JAGGED_TEXTURE);
    }

    if (mesh_.lines().n() > 0) {
      primitives_.emplace_back("Lines", mesh_.lines(), "edges-q1",
                               RECTANGULAR_TEXTURE);
    }

    if (fields != nullptr) {
      // pick one of the fields and activate it
      fields_ = fields;
      const auto& f = fields->fields().begin()->second;
      change_field(f, 0);  // activate rank 0

      field_names_.clear();
      for (const auto& fld : fields->fields()) {
        for (int i = 0; i < fld.second.ranks(); i++) {
          field_names_.push_back({fld.first, i});
        }
      }

      // is there a texture coordinate field?
      if (fields->fields().find("texcoord") != fields->fields().end()) {
        const Field& field = fields->fields().at("texcoord");
        for (size_t k = 0; k < primitives_.size(); k++) {
          std::string name = primitives_[k].title();
          if (name == "Triangles" || name == "Quads")
            primitives_[k].write_texcoord(field);
        }
      }
    }

    draw_order_.resize(primitives_.size());
    for (size_t k = 0; k < primitives_.size(); k++) draw_order_[k] = k;

    // generate initial buffer & texture for the colormap
    GL_CALL(glGenBuffers(1, &colormap_buffer_));
    GL_CALL(glGenTextures(1, &colormap_texture_));
    change_colormap("giraffe");
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

  void change_field(const Field& field, int rank) {
    // change the field in all the primitives
    for (size_t k = 0; k < primitives_.size(); k++) {
      primitives_[k].write_field(field, rank);
    }
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
            double dy = (input.y - view.y) / view.canvas.height;
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
        view.picked = pick(input.x, input.y, view);
        if (view.picked) {
          write_point_visibility(view.picked);
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
        else if (input.key == 'q')
          view.active["Quads"] = input.ivalue > 0;
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
        else if (input.key == '#')
          view.numbers = input.ivalue > 0;
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
          // updated = false;
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
            // LOG << fmt::format(
            //     "active = {}, visible = {}, dim = {}, distance = {}",
            //     view.plane.active, view.plane.visible, view.plane.dimension,
            //     view.plane.distance);
            view.plane.update();
          } else {
            if (active) *msg = "*clipping deactivated";
          }
          updated = true;
        } else if (input.key == 'C') {  // colormap change
          change_colormap(input.svalue);
          updated = true;
        } else if (input.key == 'p') {
          center_view(view);
          updated = true;
        } else if (input.key == 'f') {
          if (fields_) {
            if (view.field_mode == 0) {
              view.field_mode = 1;
              view.field_index = 0;
            } else if (size_t(view.field_index + 1) == field_names_.size()) {
              view.field_mode = 0;
              view.field_index = 0;
            } else {
              view.field_mode = 1;
              view.field_index++;  // = (view.field_index + 1) % view.field_;
            }
            auto field_info = field_names_[view.field_index];
            const auto& field = fields_->fields().at(field_info.first);
            int rank = field_info.second;
            change_field(field, rank);
            if (view.field_mode == 1)
              *msg =
                  fmt::format("*plotting \"{}\" ({})", field_info.first, rank);
            else
              *msg = "*no field";
            updated = true;
          }
        }
        break;
      }
      case wings::InputType::Scroll: {
        vec3f direction = view.eye - view.center;
        view.eye = view.center + direction * 1.0f * input.fvalue;
        vec3f up{0, 1, 0};
        view.view_matrix = glm::lookat(view.eye, view.center, up);
        updated = true;
        break;
      }
      default:
        break;
    }
    if (!updated) return false;

    // write shader uniforms
    GL_CALL(glViewport(0, 0, view.canvas.width, view.canvas.height));
    GL_CALL(glClearColor(1.0f, 1.0f, 1.0f, 1.0f));
    GL_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_POLYGON_SMOOTH);
    glDepthMask(GL_TRUE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1, 1);
    glEnable(GL_BLEND);
    // glEnable(GL_STENCIL_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_CLAMP);
    float alpha = view.transparency;
    int u_lighting = view.lighting;
    // glDepthRange(0.1, 0.9);
    if (alpha < 1.0) {
      glDisable(GL_CULL_FACE);
      // glDepthMask(GL_FALSE);
      // glDepthFunc(GL_LEQUAL);
      u_lighting = 0;
    }

    // compute the matrices
    mat4f model_view_matrix = view.view_matrix * view.model_matrix;
    mat4f normal_matrix = glm::inverse(glm::transpose(model_view_matrix));
    mat4f mvp_matrix = view.projection_matrix * model_view_matrix;

    if (view.active["Nodes"] && n_nodes_ > 0) {
      // bind which attributes we want to draw
      GL_CALL(glBindVertexArray(view.vertex_array));
      GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, node_buffer_));
      GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0));
      GL_CALL(glEnableVertexAttribArray(0));
      GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));

      const ShaderProgram& shader = shaders_["points"];
      shader.use();
      shader.set_uniform("u_ModelViewProjectionMatrix", mvp_matrix);
      shader.set_uniform("u_length", view.size);
      GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, node_buffer_));
      GL_CALL(glDrawArrays(GL_POINTS, 0, n_nodes_));
      GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
    }

    // bind which attributes we want to draw
    GL_CALL(glBindVertexArray(view.vertex_array));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, point_buffer_));
    GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0));
    GL_CALL(glEnableVertexAttribArray(0));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    if (view.active["Points"]) {
      const ShaderProgram& shader = shaders_["points"];
      shader.use();
      shader.set_uniform("u_ModelViewProjectionMatrix", mvp_matrix);
      GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, point_buffer_));
      GL_CALL(glDrawArrays(GL_POINTS, 0, mesh_.vertices().n()));
      GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
    }

    // generate a texture to hold the mesh coordinates
    GL_CALL(glActiveTexture(GL_TEXTURE0 + POINT_TEXTURE));
    GL_CALL(glBindTexture(GL_TEXTURE_BUFFER, point_texture_));
    GL_CALL(glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, point_buffer_));

    for (size_t k = 0; k < primitives_.size(); k++) {
      const GLPrimitive& primitive = primitives_[draw_order_[k]];
      if (!view.active[primitive.title()]) continue;

      int order = 0;  // parameters["field order"];
      const ShaderProgram& shader = select_shader(primitive, order);
      shader.use();

      // bind the image texture (if any)
      glActiveTexture(GL_TEXTURE0 + IMAGE_TEXTURE);
      GL_CALL(glBindTexture(GL_TEXTURE_2D, image_texture_));
      shader.set_uniform("image", int(IMAGE_TEXTURE));

      // bind the desired colormap
      glActiveTexture(GL_TEXTURE0 + COLORMAP_TEXTURE);
      GL_CALL(glBindTexture(GL_TEXTURE_BUFFER, colormap_texture_));
      shader.set_uniform("colormap", int(COLORMAP_TEXTURE));

      // set the uniforms for the shader
      shader.set_uniform("u_edges", view.show_wireframe);
      shader.set_uniform("u_lighting", u_lighting);
      shader.set_uniform("u_alpha", alpha);

      shader.set_uniform("u_ModelViewProjectionMatrix", mvp_matrix);
      shader.set_uniform("u_ModelViewMatrix", model_view_matrix);
      shader.set_uniform("u_NormalMatrix", normal_matrix);
      vec2f screen_size({1.0f * view.canvas.width, 1.0f * view.canvas.height});
      shader.set_uniform("u_ViewportSize", screen_size);

      shader.set_uniform("u_umin", primitive.umin());
      shader.set_uniform("u_umax", primitive.umax());
      shader.set_uniform("u_field_mode", view.field_mode);
      shader.set_uniform("u_picking", int(-1));

      if (view.picked) {
        if (view.picked->name == primitive.title()) {
          shader.set_uniform("u_picking", int(view.picked->index));
          if (view.picked->name == "Polyhedra") {
            std::vector<GLint> picked(32, -1);
            const auto& polyhedra = mesh_.polyhedra();
            for (int k = 0; k < polyhedra.length(view.picked->index); k++) {
              picked[k] = polyhedra[view.picked->index][k];
            }

            GLint location = glGetUniformLocation(shader.handle(), "u_faces");
            ASSERT(location >= 0);
            GL_CALL(glUniform1iv(location, picked.size(), picked.data()));
          }
        }
      }

      if (view.plane.active) {
        shader.set_uniform("u_clip", int(1));
        vec3f point, normal;
        view.plane.get(point, normal);
        shader.set_uniform("u_clip_point", point);
        shader.set_uniform("u_clip_normal", view.plane.direction * normal);
      } else
        shader.set_uniform("u_clip", int(0));

      primitive.draw(shader, point_buffer_, field_texture_, texcoord_texture_,
                     index_texture_, first_texture_, length_texture_);
    }

    if (view.plane.visible)
      view.plane.draw(view.model_matrix, view.view_matrix,
                      view.projection_matrix);

    // draw the text
    if (view.numbers) {
      const auto& shader = shaders_["text"];
      shader.use();
      glActiveTexture(GL_TEXTURE0 + POINT_TEXTURE);
      GL_CALL(glBindTexture(GL_TEXTURE_BUFFER, point_texture_));
      shader.set_uniform("points", int(POINT_TEXTURE));

      glActiveTexture(GL_TEXTURE0 + FONT_TEXTURE);
      GL_CALL(glBindTexture(GL_TEXTURE_2D, font_texture_));
      shader.set_uniform("font", int(FONT_TEXTURE));

      glActiveTexture(GL_TEXTURE0 + POINT_VISIBILITY);
      GL_CALL(glBindTexture(GL_TEXTURE_BUFFER, point_visibility_texture_));
      GL_CALL(glTexBuffer(GL_TEXTURE_BUFFER, GL_R8, point_visibility_buffer_));
      shader.set_uniform("visibility", int(POINT_VISIBILITY));

      shader.set_uniform("u_ModelViewProjectionMatrix", mvp_matrix);
      GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, point_buffer_));
      GL_CALL(glDrawArrays(GL_POINTS, 0, mesh_.vertices().n()));
      GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
    }

    // save the pixels in the wings::Scene
    view.canvas.bind();
    channels_ = 3;
    int stride = channels_ * view.canvas.width;
    // stride += (stride % 4) ? (4 - stride % 4) : 0;
    pixels_.resize(stride * view.canvas.height);
    GL_CALL(glPixelStorei(GL_PACK_ALIGNMENT, 4));
    GL_CALL(glReadPixels(0, 0, view.canvas.width, view.canvas.height, GL_RGB,
                         GL_UNSIGNED_BYTE, pixels_.data()));
    glFinish();

    return true;
  }

  void onconnect() {
    // set up the view
    view_.emplace_back();
    ClientView& view = view_.back();
    view.eye = {0, 0, 0};
    view.center = {0, 0, 0};
    vec3f xmin{1e20f, 1e20f, 1e20f}, xmax{-1e20f, -1e20f, -1e20f};
    for (size_t i = 0; i < mesh_.vertices().n(); i++) {
      for (int d = 0; d < 3; d++) {
        float x = mesh_.vertices()(i, d);
        view.center[d] += x;
        if (x > xmax[d]) xmax[d] = x;
        if (x < xmin[d]) xmin[d] = x;
      }
    }
    view.center = view.center / (1.0f * mesh_.vertices().n());
    view.center_translation.eye();
    view.inverse_center_translation.eye();
    for (int d = 0; d < 3; d++) {
      view.center_translation(d, 3) = view.center[d];
      view.inverse_center_translation(d, 3) = -view.center[d];
    }

    vec3f scale = aabb_.max - aabb_.min;
    view.size = std::max(std::max(scale[0], scale[1]), scale[2]);
    float d = scale[2] / 2.0 + scale[0] / (2.0 * tan(view.fov / 2.0));

    vec3f dir{0, 0, 1};
    view.eye = view.center + 1.05f * d * dir;
    view.center = view.eye - 2.1f * d * dir;

    view.model_matrix.eye();
    vec3f up{0, 1, 0};
    view.view_matrix = glm::lookat(view.eye, view.center, up);
    view.projection_matrix = glm::perspective(
        view.fov, float(view.canvas.width) / float(view.canvas.height),
        view.near, view.far);
    view.translation_matrix.eye();

    // vertex arrays are not shared between OpenGL contexts in different threads
    // (buffers & textures are though). Each client runs in a separate thread
    // so we need to create a vertex array upon each client connection.
    GL_CALL(glGenVertexArrays(1, &view.vertex_array));
    view.plane.initialize();
    view.plane.define(aabb_);
    view.field_mode = 0;
  }

  const ShaderProgram& select_shader(const GLPrimitive& primitive,
                                     int order) const {
    std::string suffix = (order < 0) ? "t" : std::to_string(order);
    return shaders_[primitive.name() + "-p" + suffix];
  }

 private:
  const Mesh& mesh_;
  const FieldLibrary* fields_{nullptr};
  std::vector<std::pair<std::string, int>> field_names_;

  std::vector<ClientView> view_;
  AABB aabb_;

  GLuint point_buffer_;
  GLuint point_texture_;
  GLuint node_buffer_;
  GLuint n_nodes_{0};

  GLuint index_texture_;
  GLuint first_texture_;
  GLuint length_texture_;

  GLuint image_texture_;
  GLuint field_texture_;
  GLuint texcoord_texture_;
  GLuint colormap_texture_;
  GLuint colormap_buffer_;
  GLuint font_texture_;

  GLuint point_visibility_buffer_;
  GLuint point_visibility_texture_;

  std::vector<GLPrimitive> primitives_;
  std::vector<int> draw_order_;
  ShaderLibrary shaders_;
  std::vector<PickableObject> pickables_;
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
  if (argc < 2) {
    std::cout << "usage:\n\tvwing meshfile ws_port" << std::endl;
    std::cout << "inputs:" << std::endl;
    std::cout << "[required] meshfile: can be a .meshb or .obj file"
              << std::endl;
    std::cout << "[optional] ws_port: port to use for the WebSocket connection "
                 "(default: 7681)"
              << std::endl;
    return 0;
  }

  // int tcp_port = -1;
  int ws_port = 7681;
  if (argc > 2) ws_port = std::atoi(argv[2]);
  // if (argc > 3) tcp_port = std::atoi(argv[3]);

  wings::Mesh mesh(3);
  read_mesh(argv[1], mesh);

  // calculate bounding box
  wings::vec3f xmin{1e20, 1e20, 1e20}, xmax = -1.0f * xmin;
  for (size_t k = 0; k < mesh.vertices().n(); k++) {
    for (int d = 0; d < mesh.vertices().dim(); d++) {
      auto x = mesh.vertices()[k][d];
      if (x < xmin[d]) xmin[d] = x;
      if (x > xmax[d]) xmax[d] = x;
    }
  }

  // scale and center vertices
  float lmax = xmax[0] - xmin[0];
  for (int d = 1; d < 3; d++) {
    lmax = std::max(lmax, xmax[d] - xmin[d]);
  }
  wings::vec3f center = 0.5f * (xmin + xmax);
  for (size_t k = 0; k < mesh.vertices().n(); k++) {
    for (int d = 0; d < 3; d++) {
      auto x = mesh.vertices()[k][d];
      mesh.vertices()[k][d] = 2.0 * (x - center[d]) / lmax;
    }
  }

  mesh.fields().set_defaults(mesh);
  wings::Viewer viewer(mesh, ws_port);

  return 0;
}