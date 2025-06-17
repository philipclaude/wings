### **About**

`wings` is a web interface for graphics applications. It is primarily intended for server-side rendering of meshes and solution fields for scientific applications, in which a mesh might be stored on a remote cluster or a cloud virtual machine (e.g. using GCP or AWS).

`wings` directly manages `OpenGL` contexts and handles sharing contexts between threads. There is no dependency on an `OpenGL` function loader (such as `glad`) or a window or event manager (such as `GLFW`). It may be possible to support `Vulkan` in the future (the design of the rendering context structure supports this), but I have no experience developing with `Vulkan` yet so there are no examples here (please feel free to contribute with a pull request!). There is some initial support for directly serving the client HTML pages using a TCP server, however, this needs to be tested further. I recommend opening the client HTML pages locally for now, or open the pages which are hosted on GitHub Pages, for example, https://philipclaude.github.io/wings/vwing/.

The `apps` directory contains sample applications that use `wings`. Some programs are introductory examples (such as `xwing`) whereas others are relatively feature-complete visualization tools (such as `vwing`). Please see the sections below for a more complete description of each application. The `util` directory also contains some utilities for creating new applications.

When running `wings` applications on a remote server, remember to forward the port on which the WebSocket server is listening (the `wings` default port is usually 7681). I often use Visual Studio Code to develop remotely (using the Remote SSH Extension) and VS Code has a nice feature for forwarding a port.

Please see the GitHub Issues tab for a list of active projects and known bugs/limitations.

### **Core dependencies**

- `git`,
- `CMake` (>= 3.1)
- `C++` compiler with `C++14` support
- `OpenGL` (with `EGL` on Linux and `CoreGL` on OS X - these should be installed alongside `OpenGL`),
- `stb` for writing JPEG images before encoding them and sending them to a client (this can be omitted if you are already using `stb` in your project - otherwise it will be downloaded automatically).

### **Optional dependencies (to run the sample applications)**

The utilities and sample applications require a few external repositories for IO. If the examples are being built, the `wings` configuration will automatically download, build and link to them. They will be located in the `third_party` directory and can be deleted with `make clean_extern`.

- `fmtlib`: https://github.com/fmtlib/fmt
- `tinyobjloader`: https://github.com/tinyobjloader/tinyobjloader
- `libMeshb`: https://github.com/LoicMarechal/libMeshb

#### **Quickstart**

1. Clone the repository:

   - `$ git clone https://github.com/philipclaude/wings.git`

2. Build `wings`:

   - `$ mkdir wings/build`
   - `$ cd wings/build && cmake ../`
   - `$ make`

3. Run the example program (`xwing`):

   - `$ bin/xwing`

4. Connect to the server by opening `wings/apps/xwing/index.html` and you should see an icosahedron. Click and drag the mouse to rotate the mesh!

Alternatively, you can try out the more complete hybrid mesh viewer by running `bin/vwing` and opening `wings/apps/vwing/index.html` (or https://philipclaude.github.io/wings/vwing/).

#### **How `wings` works**

`wings` communicates asynchronously between the client and server using a WebSocket connection (following RFC 6455). Each client connection spawns a new listener thread in the server which will listen for rendering requests. These requests are encoded by the client (sent as a string), and wings will decipher them as a specific event. The first character typically encodes the type of event (`K`: key-value, `M` is a mouse-motion event, `W` is a mouse-wheel event):

- for key-value events (character 0 is `K`): the second character (character 1) encodes the type of the value (`F` for float, `I` for integer and `S` for string). The remaining characters contain the corresponding value (`fvalue`, `ivalue` or `svalue`, respectively).
- for a mouse-motion event (character 0 is `M`): characters 1-5 is the screen x-coordinate and characters 6-10 is the screen y-coordinate.
- for a mouse-wheel event (character 0 is `W`): character 1 will be `+` if scrolling in or `-` of scrolling out.

Depending on the message, the server will respond with a JPEG-encoded representation of the pixels in the image to display in the browser. The quality of the image can be adjusted to speed up message delivery, which can be useful when the view is being manipulated over slower connections.

Please note that `wings` was specifically designed in this way to reduce latency when sending an image over a network. A more natural communication method would consist of using JSON or protocol buffers to represent server messages, however, the server will almost always respond with an image, so the overhead caused by encoding/decoding the image from another format didn't seem worth it. Note that the JPEG image is encoded in `base64`, so if you want to send something other than an image from the server, you can use any non-`base64` character as the first character in the response and subsequently detect that character in the client code. For example, `*` is a non-`base64` character and is used in the `apps/vwing` application to print text messages in the browser.

It would also be possible to encode the client messages using JSON or protocol buffers, but that adds a dependency (and I tend to avoid using dependencies unless absolutely necessary).

#### **Using the `wings` API**

The core `wings` functionality is provided as a single-header, single-source library (`wings.h` and `wings.cpp`). The `CMake` configuration will build the `wings` library, however, you can also directly compile `wings.cpp` while setting the include path to contain the `wings` repository.

If you are already using `stb` in your project and are compiling the `stbi_write_jpg_to_func` function, please set the `WINGS_STB_LOCATION` option (in your `CMake` configuration before adding the `wings` subdirectory) to the root `stb` directory.

On the server-side, you will need to define a class that inherits from the `Scene` class which (1) creates the `RenderingContext` and (2) defines **two** methods: `onconnect` and `render`.

- The `RenderingContext` can be created by calling the static `RenderingContext::create` function which accepts an `enum` with the type of context to create (currently only `kOpenGL` is supported).
- The `onconnect` function is called when a new client is connected - the image displayed by this client will be indexed with a particular client/view index (this is how multiple client views are supported).
- The `render` function should render the scene according to the aforementioned client/view index and the input `ClientInput` structure. This structure contains information about the type of event (key-value, mouse motion, double click, scroll) and any corresponding data. The end of the render function should fill the RGB values of the `pixels_` (a `protected` member of the `Scene` class), which is a vector of bytes (`unsigned char`) of size `3 x width x height`. `wings` will then automatically encode the image as a string and send it to the browser which can be rendered by assigning the `src` attribute of an HTML `img` element.

You can then create a `RenderingServer` which accepts a derived `Scene` object and WebSocket port number.

On the client side, you need to (1) create a `img` HTML element and (2) create a WebSocket connection handler. The `src` attribute of the `img` element can be assigned to the event data when the WebSocket handler receives a message from the server (the `onmessage` callback). You can then send messages to your specialized `Scene` using the `send` function of your WebSocket connection (using the message conventions described above). I recommend inspecting the `apps/xwing` source for a complete example.

#### **Reminders**

Note that `OpenGL` contexts can share resources such as buffers and textures but **cannot** share containers, such as vertex array objects or framebuffer objects (holding the render buffer and depth buffer). A `glCanvas` utility structure is provided to store and handle these, which should be created for each client connection (i.e. in each `onconnect` callback).

### **Sample applications**

#### **`xwing`**: example wing viewer

This is a minimal, self-contained example that includes everything needed to set up a view and rotate a model when clicking (a lot of the code for manipulating vectors and matrices in the `util` directory is duplicated). By default, `xwing` will load and render an icosahedron, but a `.obj` file can also be passed as the second argument, which will load the triangulation with `tinyobjloader`.

#### **`vwing`**: hybrid mesh viewer

This is a more complete program for rendering mixed-element meshes consisting of lines, triangles, quads, polygons, tetrahedra, prisms, pyramids and polyhedra. `vwing` also sets up a few default "fields" (attributes) corresponding to the element group number (or reference) or the cell id. You can cycle through the available fields by pressing the `f` key. `vwing` supports clipping planes as well as element "picking" and prints a message in the browser with the picked element information. After picking an element, you can press `c` to center the view on the picked element.

#### **License**

All `wings` source code (`wings.h`, `wings.cpp` as well as all `C++`, `HTML`, `JavaScript` and `GLSL` code for the apps) is distributed under the Apache-2.0 License.

Copyright 2023 Philip Claude Caplan

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
