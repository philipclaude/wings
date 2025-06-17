//
//  wings: web interface for graphics applications
//
//  Copyright 2023 - 2025 Philip Claude Caplan
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

#include <cstdint>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace wings {

// Possible event types.
enum class InputType : uint32_t {
  MouseMotion,
  SingleClick,
  Scroll,
  DoubleClick,
  KeyValueBool,
  KeyValueInt,
  KeyValueFloat,
  KeyValueStr,
  KeyPressed,
  KeyReleased,
  AnimationRequest,
  GetTime,
  SetTime,
  SetFrames
};

// Client input passed to the Scene::render function.
struct ClientInput {
  InputType type;
  bool dragging{false};
  bool modifier{false};
  float x{0.0f};
  float y{0.0f};
  char key{'\0'};
  bool bvalue{false};
  float fvalue{0.0f};
  int ivalue{0};
  const char* svalue{nullptr};
  float time{0.0f};
  bool looping{false};
};

// Possible rendering context types.
enum RenderingContextType : uint8_t { kOpenGL };

// The main rendering context which will be constructed for each client thread.
struct RenderingContext {
  // Creates a rendering context of a specific type.
  RenderingContext(RenderingContextType _type) : type(_type) {}
  virtual ~RenderingContext(){};

  // Prints information about the rendering context in use,
  // such as version number and supported extensions.
  void print(bool with_extensions = false);

  // Creates a rendering context of a specific type at the server-level.
  static std::shared_ptr<RenderingContext> create(RenderingContextType type);

  // Creates a rendering context from an existing server context, thus sharing
  // resources such as buffers but not containers such as vertex array objects.
  static std::shared_ptr<RenderingContext> create(const RenderingContext& ctx);

  // Starts a rendering section.
  void enter_render_section() {
    lock_.lock();
    make_context_current();
  }

  // Finishes a rendering section.
  void leave_render_section() {
    swap_buffers();
    release_context();
    lock_.unlock();
  }

  // Resizes the canvas on which to draw, calling the context-specific
  // resize_canvas function declared below.
  void resize(int width, int height) {
    lock_.lock();
    make_context_current();
    resize_canvas(width, height);
    release_context();
    lock_.unlock();
  }

  // Makes the rendering context active (needed when multiple contexts are
  // active due to the client-server model and each thread has a unique
  // context).
  virtual void make_context_current() = 0;

  // Release the current context.
  virtual void release_context() = 0;

  // Swap the frame buffers.
  virtual void swap_buffers() = 0;

  // Resizes the canvas on which to draw.
  // CoreGL on OS X doesn't need this, but the EGL context
  // is associated with a "surface" on which to draw, which has dimensions.
  virtual void resize_canvas(int width, int height) = 0;

  std::mutex lock_;
  RenderingContextType type;
};

class Scene {
 public:
  virtual ~Scene() {}
  // Called whenever a client connection request comes in from the browser
  // each connection will then be associated with a client_idx (see below),
  // which can be used to control the view for a particular client.
  virtual void onconnect() = 0;

  // Renders the scene for a particular client (indexed by client_idx)
  // according to the input defined by ClientInput.
  // Returns true if rendering occurred and false otherwise.
  // A string can also be returned (for example to display a text message in the
  // browser).
  virtual bool render(const ClientInput& input, int client_idx,
                      std::string* = nullptr) = 0;

  // Access the rendering context
  const RenderingContext& context() const { return *context_; }
  RenderingContext& context() { return *context_; }

  // Returns the quality (%) used to compress the JPEG image.
  int quality() const { return quality_; }

  // Current canvas width - should be set in derived object upon resizing.
  int width() const { return width_; }

  // Current canvas height - should be set in derived object upon resizing.
  int height() const { return height_; }

  // Number of color channels in each pixel (usually 3).
  int channels() const { return channels_; }

  // The pixels to send to stb to write the JPEG image.
  const std::vector<unsigned char>& pixels() const { return pixels_; }

 protected:
  std::shared_ptr<RenderingContext> context_;
  std::vector<unsigned char> pixels_;
  int quality_{80};
  int width_{800};
  int height_{600};
  int channels_{3};
};

// Internal wings status codes.
enum class StatusCode {
  kCreateSocketError,
  kBindError,
  kListenError,
  kConnectError,
  kHandshakeError,
  kFrameError,
  kSuccess
};

// String description of wings status codes.
static const std::map<StatusCode, std::string> RenderStatusString = {
    {StatusCode::kCreateSocketError, "cannot create socket"},
    {StatusCode::kBindError, "cannot bind socket on port"},
    {StatusCode::kListenError, "cannot listen on port"},
    {StatusCode::kConnectError, "cannot connect to client"},
    {StatusCode::kSuccess, "success"}};

class WebsocketRenderer;
class RenderingServer {
 public:
  // Starts a websocket server (listening on specified port) to render a scene.
  RenderingServer(Scene& scene, int port);
  ~RenderingServer();

  // Serves an HTML file using a TCP server on a specified port number.
  StatusCode start(const std::string& html_file, int port);

  // Stops the TCP server.
  StatusCode stop();

 private:
  std::unique_ptr<WebsocketRenderer> renderer_;
  std::atomic<bool> listen_;
  std::future<StatusCode> server_;
  // Scene& scene_;
};

// Utility structure to represent an OpenGL canvas.
// Each client view should have a canvas since framebuffers cannot be shared
// between different contexts.
struct glCanvas {
  // Initializes the canvas to a particular width and height.
  glCanvas(int w, int h) : width(w), height(h) { create(); }
  ~glCanvas() { release(); }

  // Creates the framebuffer, depthbuffer and renderbuffer.
  // Should be called on initialized and on resize.
  void create();

  // Makes this canvas framebuffer the current one in use.
  void bind();

  // Resizes the canvas.
  void resize(int w, int h);

  // Frees the buffers associated with this canvas.
  void release();

  int width{0}, height{0};
  int renderbuffer{-1}, framebuffer{-1}, depthbuffer{-1};
};

}  // namespace wings