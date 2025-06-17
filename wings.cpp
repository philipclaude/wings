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
#include "wings.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cassert>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#define MAX_CLIENTS 10

#ifdef __cplusplus
extern "C" {
#endif

typedef void stbi_write_func(void* context, void* data, int size);
void stbi_flip_vertically_on_write(int flag);

int stbi_write_jpg_to_func(stbi_write_func* func, void* context, int x, int y,
                           int comp, const void* data, int quality);

#ifdef __cplusplus
}
#endif

#if WINGS_COMPILE_STB

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#pragma GCC diagnostic pop
#endif

#define GL_GLEXT_PROTOTYPES
#ifdef __APPLE__
#define __gl_h_
#define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#define GL_SILENCE_DEPRECATION
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#define HAVE_EGL 0
#define HAVE_CGL 1
#else
#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#define HAVE_EGL 1
#define HAVE_CGL 0
#endif

#define GL_CALL(X)                                                           \
  {                                                                          \
    (X);                                                                     \
    GLenum error = glGetError();                                             \
    if (error != GL_NO_ERROR) {                                              \
      const char* message = "";                                              \
      switch (error) {                                                       \
        case GL_INVALID_ENUM:                                                \
          message = "invalid enum";                                          \
          break;                                                             \
        case GL_INVALID_VALUE:                                               \
          message = "invalid value";                                         \
          break;                                                             \
        case GL_INVALID_OPERATION:                                           \
          message = "invalid operation";                                     \
          break;                                                             \
        case GL_INVALID_FRAMEBUFFER_OPERATION:                               \
          message = "invalid framebuffer operation";                         \
          break;                                                             \
        case GL_OUT_OF_MEMORY:                                               \
          message = "out of memory";                                         \
          break;                                                             \
        default:                                                             \
          message = "unknown error";                                         \
      }                                                                      \
      printf("OpenGL error in file %s at line %d: %s\n", __FILE__, __LINE__, \
             message);                                                       \
    }                                                                        \
    assert(error == GL_NO_ERROR);                                            \
  }

namespace sha1 {

/*
    Adapted from: https://github.com/vog/sha1 (which is in the public domain).

    ============
    SHA-1 in C++
    ============

    100% Public Domain.

    Original C Code
        -- Steve Reid <steve@edmweb.com>
    Small changes to fit into bglibs
        -- Bruce Guenter <bruce@untroubled.org>
    Translation to simpler C++ Code
        -- Volker Diels-Grabsch <v@njh.eu>
    Safety fixes
        -- Eugene Hopkinson <slowriot at voxelstorm dot com>
    Header-only library
        -- Zlatko Michailov <zlatko@michailov.org>
*/
constexpr size_t BLOCK_INTS = 16;  // # of 32-bit integers per SHA1 block
constexpr size_t BLOCK_BYTES = BLOCK_INTS * sizeof(uint32_t);

uint32_t rol(const uint32_t value, const size_t bits) {
  return (value << bits) | (value >> (32 - bits));
}

uint32_t blk(const std::array<uint32_t, BLOCK_INTS>& b, const size_t i) {
  return rol(b[(i + 13) & 15] ^ b[(i + 8) & 15] ^ b[(i + 2) & 15] ^ b[i], 1);
}

#define R0(v, w, x, y, z, i)                                    \
  z += ((w & (x ^ y)) ^ y) + block[i] + 0x5a827999 + rol(v, 5); \
  w = rol(w, 30);

#define R1(v, w, x, y, z, i)                                    \
  block[i] = blk(block, i);                                     \
  z += ((w & (x ^ y)) ^ y) + block[i] + 0x5a827999 + rol(v, 5); \
  w = rol(w, 30);

#define R2(v, w, x, y, z, i)                            \
  block[i] = blk(block, i);                             \
  z += (w ^ x ^ y) + block[i] + 0x6ed9eba1 + rol(v, 5); \
  w = rol(w, 30);

#define R3(v, w, x, y, z, i)                                          \
  block[i] = blk(block, i);                                           \
  z += (((w | x) & y) | (w & x)) + block[i] + 0x8f1bbcdc + rol(v, 5); \
  w = rol(w, 30);

#define R4(v, w, x, y, z, i)                            \
  block[i] = blk(block, i);                             \
  z += (w ^ x ^ y) + block[i] + 0xca62c1d6 + rol(v, 5); \
  w = rol(w, 30);

void transform(std::array<uint32_t, 5>& digest,
               std::array<uint32_t, BLOCK_INTS>& block, uint64_t& transforms) {
  // copy digest to working variables
  uint32_t a = digest[0];
  uint32_t b = digest[1];
  uint32_t c = digest[2];
  uint32_t d = digest[3];
  uint32_t e = digest[4];

  // perform rounds of 20 operations each (loop unrolled)
  R0(a, b, c, d, e, 0);
  R0(e, a, b, c, d, 1);
  R0(d, e, a, b, c, 2);
  R0(c, d, e, a, b, 3);
  R0(b, c, d, e, a, 4);
  R0(a, b, c, d, e, 5);
  R0(e, a, b, c, d, 6);
  R0(d, e, a, b, c, 7);
  R0(c, d, e, a, b, 8);
  R0(b, c, d, e, a, 9);
  R0(a, b, c, d, e, 10);
  R0(e, a, b, c, d, 11);
  R0(d, e, a, b, c, 12);
  R0(c, d, e, a, b, 13);
  R0(b, c, d, e, a, 14);
  R0(a, b, c, d, e, 15);
  R1(e, a, b, c, d, 0);
  R1(d, e, a, b, c, 1);
  R1(c, d, e, a, b, 2);
  R1(b, c, d, e, a, 3);
  R2(a, b, c, d, e, 4);
  R2(e, a, b, c, d, 5);
  R2(d, e, a, b, c, 6);
  R2(c, d, e, a, b, 7);
  R2(b, c, d, e, a, 8);
  R2(a, b, c, d, e, 9);
  R2(e, a, b, c, d, 10);
  R2(d, e, a, b, c, 11);
  R2(c, d, e, a, b, 12);
  R2(b, c, d, e, a, 13);
  R2(a, b, c, d, e, 14);
  R2(e, a, b, c, d, 15);
  R2(d, e, a, b, c, 0);
  R2(c, d, e, a, b, 1);
  R2(b, c, d, e, a, 2);
  R2(a, b, c, d, e, 3);
  R2(e, a, b, c, d, 4);
  R2(d, e, a, b, c, 5);
  R2(c, d, e, a, b, 6);
  R2(b, c, d, e, a, 7);
  R3(a, b, c, d, e, 8);
  R3(e, a, b, c, d, 9);
  R3(d, e, a, b, c, 10);
  R3(c, d, e, a, b, 11);
  R3(b, c, d, e, a, 12);
  R3(a, b, c, d, e, 13);
  R3(e, a, b, c, d, 14);
  R3(d, e, a, b, c, 15);
  R3(c, d, e, a, b, 0);
  R3(b, c, d, e, a, 1);
  R3(a, b, c, d, e, 2);
  R3(e, a, b, c, d, 3);
  R3(d, e, a, b, c, 4);
  R3(c, d, e, a, b, 5);
  R3(b, c, d, e, a, 6);
  R3(a, b, c, d, e, 7);
  R3(e, a, b, c, d, 8);
  R3(d, e, a, b, c, 9);
  R3(c, d, e, a, b, 10);
  R3(b, c, d, e, a, 11);
  R4(a, b, c, d, e, 12);
  R4(e, a, b, c, d, 13);
  R4(d, e, a, b, c, 14);
  R4(c, d, e, a, b, 15);
  R4(b, c, d, e, a, 0);
  R4(a, b, c, d, e, 1);
  R4(e, a, b, c, d, 2);
  R4(d, e, a, b, c, 3);
  R4(c, d, e, a, b, 4);
  R4(b, c, d, e, a, 5);
  R4(a, b, c, d, e, 6);
  R4(e, a, b, c, d, 7);
  R4(d, e, a, b, c, 8);
  R4(c, d, e, a, b, 9);
  R4(b, c, d, e, a, 10);
  R4(a, b, c, d, e, 11);
  R4(e, a, b, c, d, 12);
  R4(d, e, a, b, c, 13);
  R4(c, d, e, a, b, 14);
  R4(b, c, d, e, a, 15);

  // add the working variables back into digest
  digest[0] += a;
  digest[1] += b;
  digest[2] += c;
  digest[3] += d;
  digest[4] += e;

  // count the number of transformations
  transforms++;
}

void buffer_to_block(const std::string& buffer,
                     std::array<uint32_t, BLOCK_INTS>& block) {
  // convert the string (byte buffer) to a uint32_t array (MSB)
  for (size_t i = 0; i < BLOCK_INTS; i++) {
    block[i] = (buffer[4 * i + 3] & 0xff) | (buffer[4 * i + 2] & 0xff) << 8 |
               (buffer[4 * i + 1] & 0xff) << 16 |
               (buffer[4 * i + 0] & 0xff) << 24;
  }
}

enum OutputType : uint8_t { kHex, kByte };
void hash(const std::string& input, std::string& output, OutputType type) {
  // initialize the buffer from the input
  std::array<uint32_t, 5> digest = {0x67452301, 0xefcdab89, 0x98badcfe,
                                    0x10325476, 0xc3d2e1f0};
  uint64_t transforms = 0;
  std::istringstream is(input);
  std::string buffer;
  std::array<uint32_t, BLOCK_INTS> block;
  while (true) {
    std::array<char, BLOCK_BYTES> sbuf;
    is.read(sbuf.data(), BLOCK_BYTES - buffer.size());
    buffer.append(sbuf.data(), (std::size_t)is.gcount());
    if (buffer.size() != BLOCK_BYTES) break;
    buffer_to_block(buffer, block);
    transform(digest, block, transforms);
    buffer.clear();
  }

  // total number of hashed bits
  uint64_t total_bits = (transforms * BLOCK_BYTES + buffer.size()) * 8;

  // add padding
  buffer += (char)0x80;
  size_t orig_size = buffer.size();
  while (buffer.size() < BLOCK_BYTES) {
    buffer += (char)0x00;
  }
  buffer_to_block(buffer, block);
  if (orig_size > BLOCK_BYTES - 8) {
    transform(digest, block, transforms);
    for (size_t i = 0; i < BLOCK_INTS - 2; i++) {
      block[i] = 0;
    }
  }

  // append total_bits, split this uint64_t into two uint32_t
  block[BLOCK_INTS - 1] = (uint32_t)total_bits;
  block[BLOCK_INTS - 2] = (uint32_t)(total_bits >> 32);
  transform(digest, block, transforms);

  // convert to hex
  std::ostringstream result;
  for (size_t i = 0; i < sizeof(digest) / sizeof(digest[0]); i++) {
    result << std::hex << std::setfill('0') << std::setw(8);
    result << digest[i];
  }
  std::string output_hex = result.str();
  if (type == OutputType::kHex) {
    output = output_hex;
    return;
  }

  // convert the hex characters to bytes
  output.clear();
  output.resize(20);
  for (size_t i = 0; i < 20; i++) {
    std::string byte = output_hex.substr(2 * i, 2);
    output[i] = (char)(int)strtol(byte.c_str(), nullptr, 16);
  }
}

}  // namespace sha1

namespace base64 {

/*
Adapted from: https://base64.sourceforge.net/b64.c (MIT license).

MODULE NAME:    b64.c

AUTHOR:         Bob Trower 2001/08/04

PROJECT:        Crypt Data Packaging

COPYRIGHT:      Copyright (c) Trantor Standard Systems Inc., 2001

NOTES:          This source code may be used as you wish, subject to
                the MIT license.  See the LICENCE section below.

                Canonical source should be at:
                    http://base64.sourceforge.net

LICENCE:        Copyright (c) 2001 Bob Trower, Trantor Standard Systems Inc.

                Permission is hereby granted, free of charge, to any person
                obtaining a copy of this software and associated
                documentation files (the "Software"), to deal in the
                Software without restriction, including without limitation
                the rights to use, copy, modify, merge, publish, distribute,
                sublicense, and/or sell copies of the Software, and to
                permit persons to whom the Software is furnished to do so,
                subject to the following conditions:

                The above copyright notice and this permission notice shall
                be included in all copies or substantial portions of the
                Software.

                THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
                KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
                WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
                PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
                OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
                OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
                OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
                SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
static const unsigned char b64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void encodeblock(const unsigned char* x, unsigned char* y, int m) {
  y[0] = b64[(int)(x[0] >> 2)];
  y[1] = b64[(int)(((x[0] & 0x03) << 4) | ((x[1] & 0xf0) >> 4))];
  y[2] =
      (m > 1 ? b64[(int)(((x[1] & 0x0f) << 2) | ((x[2] & 0xc0) >> 6))] : '=');
  y[3] = (m > 2 ? b64[(int)(x[2] & 0x3f)] : '=');
};

void encode(const char* input, size_t n_input, std::string& output) {
  std::array<unsigned char, 3> x = {0U, 0U, 0U};
  std::array<unsigned char, 4> y = {0U, 0U, 0U, 0U};

  output.resize(4 * (n_input + 2) / 3, '\0');

  size_t n = 0, n_output = 0;
  while (n < n_input) {
    int n_block = 0;
    for (int k = 0; k < 3; k++) {
      if (n < n_input) {
        x[k] = (unsigned char)input[n++];
        n_block++;
      } else {
        x[k] = 0U;
      }
    }
    if (n_block > 0) {
      encodeblock(x.data(), y.data(), n_block);
      for (int k = 0; k < 4; k++) {
        output[n_output++] = y[k];
      }
    }
  }
  output.resize(n_output);
}

}  // namespace base64

namespace wings {

#if HAVE_EGL
static std::map<EGLint, std::string> egl_error_string = {
    {EGL_NOT_INITIALIZED, "not initialized"},
    {EGL_BAD_ACCESS, "bad access"},
    {EGL_BAD_ALLOC, "bad alloc"},
    {EGL_BAD_ATTRIBUTE, "bad attribute"},
    {EGL_BAD_CONTEXT, "bad context"},
    {EGL_BAD_CONFIG, "bad config"},
    {EGL_BAD_CURRENT_SURFACE, "bad current surface"},
    {EGL_BAD_DISPLAY, "bad display"},
    {EGL_BAD_SURFACE, "bad surface"},
    {EGL_BAD_MATCH, "bad match"},
    {EGL_BAD_PARAMETER, "bad parameter"},
    {EGL_BAD_NATIVE_PIXMAP, "bad native pixmap"},
    {EGL_BAD_NATIVE_WINDOW, "bad native window"},
    {EGL_CONTEXT_LOST, "context lost"}};

#define EGL_CHECK()                                                       \
  {                                                                       \
    EGLint error = eglGetError();                                         \
    if (error != EGL_SUCCESS)                                             \
      printf("EGL error in file %s at line %d: %s\n", __FILE__, __LINE__, \
             egl_error_string[error].c_str());                            \
    assert(error == EGL_SUCCESS);                                         \
  }

#define EGL_CALL(X) \
  {                 \
    (X);            \
    EGL_CHECK();    \
  }

#endif

// opcodes and GUID defined by RFC6455
#define RFC6455_OP_CONTINUE 0x0
#define RFC6455_OP_TEXT 0x1
#define RFC6455_OP_BINARY 0x2
#define RFC6455_OP_CLOSE 0x8
#define RFC6455_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

int shakehands(int client_fd, const std::string& request) {
  // GUID defined by RFC6455
  const std::string guid(RFC6455_GUID);

  // look for the 'Sec-WebSocket-Key' string
  std::string lookfor = "Sec-WebSocket-Key:";
  int idx0 = request.find(lookfor);
  int idx1 = request.find("\r\n", idx0);

  // extract the key and remove whitespace
  int n = idx1 - (idx0 + lookfor.size());
  std::string key = request.substr(idx0 + lookfor.size(), n);
  const char* ws = " \t\n\r\f\v";
  key.erase(0, key.find_first_not_of(ws));

  // encode the key + GUID for the handshake response
  std::string key_guid = key + guid;
  std::string hash;
  sha1::hash(key_guid, hash, sha1::OutputType::kByte);
  std::string accept;
  base64::encode(hash.c_str(), hash.size(), accept);

  // accumulate the handshake response
  std::string response =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: " +
      accept + "\r\n\r\n";

  // send the handshake response back to the client
  return send(client_fd, response.c_str(), response.size(), 0);
}

#ifndef ntohll
// https://stackoverflow.com/a/18221570
uint64_t ntohll(uint64_t x) {
  const unsigned t = 1;
  if (*(const unsigned char*)&t) {
    x = ((uint64_t)ntohl(x & 0xffffffffU) << 32) | ntohl((uint32_t)(x >> 32));
  }
  return x;
}
#endif

StatusCode parseframe(const std::string& frame, size_t n_bytes,
                      std::string& message) {
  // I found this website helpful:
  // https://www.openmymind.net/WebSocket-Framing-Masking-Fragmentation-and-More/
  // the first bit must be set to 1 (fin)
  if ((frame[0] & 0x80) != 0x80) return StatusCode::kFrameError;

  // the next 3 bits are the extensions (which are not supported)
  if ((frame[0] & 0x70) != 0x0) return StatusCode::kFrameError;

  // the next 4 bits are the opcode
  char opcode = frame[0] & 0xF;
  std::string state = "unknown";
  if (opcode == RFC6455_OP_TEXT)
    state = "text";
  else if (opcode == RFC6455_OP_BINARY)
    state = "binary";
  else if (opcode == RFC6455_OP_CLOSE) {
    message = "close";
    return StatusCode::kSuccess;
  } else if (opcode == RFC6455_OP_CONTINUE)
    state = "continue";

  // the masking bit should be equal to 1 (client -> server message always have
  // this equal to 1)
  if ((frame[1] & 0x80) != 0x80) return StatusCode::kFrameError;

  // read the 7 bits after the mask to get the initial payload length
  size_t length = frame[1] & 0x7F;

  // initialize the masking key offset to two bytes from the frame start
  // (what has been read already)
  size_t offset = 2;
  if (length <= 125) {
    // nothing to do, masking key is already at the right offset
  } else if (length == 126) {  // 0x7E
    // the next two bytes have the length (16-bit unsigned integer)
    uint16_t n;
    std::memcpy(&n, &frame[2], sizeof(n));
    length = ntohs(n);
    offset += 2;
  } else if (length == 127) {  // 0x7F
    // the next 8 bytes have the length (64-bit unsigned integer)
    // this is not tested yet - this module is intended for small client
    // messages - we might need to support continuation frames and fin != 1
    // before being able to handle longer messages
    assert(false);
    uint64_t n;
    std::memcpy(&n, &frame[2], sizeof(n));
    length = ntohll(n);
    offset += 8;
  }

  // we always have a masking key when receiving a message from the client
  const char* masking_key = frame.data() + offset;

  offset += 4;  // account for masking key offset
  if (length + offset != n_bytes) {
    // printf("error parsing frame\n");
    //  return -1;
  }

  message.resize(length);
  for (size_t i = 0; i < length; i++) {
    // reverse the xor done by the client (with the masking key)
    message[i] = frame[i + offset] ^ masking_key[i % 4];
  }

  return StatusCode::kSuccess;
}

void RenderingContext::print(bool with_extensions) {
  const GLubyte* renderer = glGetString(GL_RENDERER);
  const GLubyte* vendor = glGetString(GL_VENDOR);
  const GLubyte* version = glGetString(GL_VERSION);
  const GLubyte* glsl_version = glGetString(GL_SHADING_LANGUAGE_VERSION);

  GLint major, minor;
  glGetIntegerv(GL_MAJOR_VERSION, &major);
  glGetIntegerv(GL_MINOR_VERSION, &minor);

  printf("-------------------------------------------------------------\n");
  printf("--> vendor: %s\n", vendor);
  printf("--> device: %s\n", renderer);
  printf("--> driver: %s (@ OpenGL %d.%d)\n", version, major, minor);
  printf("--> @ GLSL: %s\n", glsl_version);
#if HAVE_EGL
  // calling eglInitialize on an already initialized context has no effect
  EGLint egl_major, egl_minor;
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGL_CALL(eglInitialize(display, &egl_major, &egl_minor));
  printf("--> @ EGL: %d.%d\n", egl_major, egl_minor);
#endif
  printf("-------------------------------------------------------------\n");

  if (with_extensions) {
    GLint nb_ext;
    glGetIntegerv(GL_NUM_EXTENSIONS, &nb_ext);
    for (int i = 0; i < nb_ext; i++)
      printf("%s\n", glGetStringi(GL_EXTENSIONS, i));
  }
}

struct glRenderingContext : public RenderingContext {
  glRenderingContext(const RenderingContext* ctx);

  // void create_renderbuffers(int width, int height);
  void make_context_current();
  void release_context();
  void swap_buffers();
  void resize_canvas(int width, int height);

#if HAVE_EGL
  EGLDisplay display;
  EGLSurface surface;
  EGLContext context;
#elif HAVE_CGL
  CGLContextObj context;
#endif
  // GLuint framebuffer, depthbuffer, renderbuffer;
};

glRenderingContext::glRenderingContext(const RenderingContext* ctx)
    : RenderingContext(RenderingContextType::kOpenGL) {
#if HAVE_EGL

  // initialize EGL
  display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGL_CALL(eglInitialize(display, nullptr, nullptr));
  EGL_CALL(eglBindAPI(EGL_OPENGL_API));

  // select an appropriate configuration
  EGLint config_attribs[] = {EGL_SURFACE_TYPE,
                             EGL_PBUFFER_BIT,
                             EGL_RED_SIZE,
                             8,
                             EGL_GREEN_SIZE,
                             8,
                             EGL_BLUE_SIZE,
                             8,
                             EGL_ALPHA_SIZE,
                             8,
                             EGL_DEPTH_SIZE,
                             24,
                             EGL_STENCIL_SIZE,
                             8,
                             EGL_RENDERABLE_TYPE,
                             EGL_OPENGL_BIT,
                             EGL_NONE};
  EGLint n_config;
  EGLConfig config;
  EGL_CALL(eglChooseConfig(display, config_attribs, &config, 1, &n_config));

  // create a context
  // we need to make sure we have a EGL_CONTEXT_CLIENT_VERSION of 2
  // in order for shared contexts to work, though it doesn't seem
  // this needs to be set explicitly
  EGLint context_attribs[] = {
      EGL_CONTEXT_MAJOR_VERSION,
      3,
      EGL_CONTEXT_MINOR_VERSION,
      0,
      EGL_CONTEXT_OPENGL_PROFILE_MASK,
      EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
      EGL_NONE,
  };
  EGLContext ref_ctx = EGL_NO_CONTEXT;
  if (ctx) ref_ctx = static_cast<const glRenderingContext*>(ctx)->context;
  context = eglCreateContext(display, config, ref_ctx, context_attribs);
  EGL_CHECK();

  // create a surface
  EGLint surface_attribs[] = {EGL_WIDTH, static_cast<int>(800), EGL_HEIGHT,
                              static_cast<int>(600), EGL_NONE};
  surface = eglCreatePbufferSurface(display, config, surface_attribs);
  EGL_CHECK();

#elif HAVE_CGL
  CGLPixelFormatAttribute attributes[13] = {
      kCGLPFAOpenGLProfile,
      (CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core,
      kCGLPFAAccelerated,
      kCGLPFAColorSize,
      (CGLPixelFormatAttribute)24,
      kCGLPFAAlphaSize,
      (CGLPixelFormatAttribute)8,
      kCGLPFADoubleBuffer,
      kCGLPFASampleBuffers,
      (CGLPixelFormatAttribute)1,
      kCGLPFASamples,
      (CGLPixelFormatAttribute)4,
      (CGLPixelFormatAttribute)0};

  CGLPixelFormatObj pix;
  GLint num;
  CGLChoosePixelFormat(attributes, &pix, &num);
  CGLContextObj ref_ctx = nullptr;
  if (ctx) ref_ctx = static_cast<const glRenderingContext*>(ctx)->context;
  CGLCreateContext(pix, ref_ctx, &context);
  CGLDestroyPixelFormat(pix);

#else
#error "no OpenGL backend was found"
#endif
  make_context_current();
}

void glRenderingContext::make_context_current() {
#if HAVE_EGL
  assert(display != EGL_NO_DISPLAY);
  assert(surface != EGL_NO_SURFACE);
  assert(context != EGL_NO_CONTEXT);
  EGL_CALL(eglMakeCurrent(display, surface, surface, context));
#elif HAVE_CGL
  CGLSetCurrentContext(context);
#endif
}

void glRenderingContext::release_context() {
#if HAVE_EGL
  EGL_CALL(
      eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
#elif HAVE_CGL
  CGLSetCurrentContext(nullptr);
#endif
}

void glRenderingContext::swap_buffers() {
#if HAVE_EGL
  eglSwapBuffers(display, surface);
#elif HAVE_CGL
  CGLFlushDrawable(context);
#endif
}

void glRenderingContext::resize_canvas(int width, int height) {
#if HAVE_EGL
  EGLint config_attribs[] = {EGL_SURFACE_TYPE,
                             EGL_PBUFFER_BIT,
                             EGL_RED_SIZE,
                             8,
                             EGL_GREEN_SIZE,
                             8,
                             EGL_BLUE_SIZE,
                             8,
                             EGL_ALPHA_SIZE,
                             8,
                             EGL_DEPTH_SIZE,
                             24,
                             EGL_STENCIL_SIZE,
                             8,
                             EGL_RENDERABLE_TYPE,
                             EGL_OPENGL_BIT,
                             EGL_NONE};
  EGLint n_config;
  EGLConfig config;
  EGL_CALL(eglChooseConfig(display, config_attribs, &config, 1, &n_config));
  EGLint surface_attribs[] = {EGL_WIDTH, static_cast<int>(width), EGL_HEIGHT,
                              static_cast<int>(height), EGL_NONE};
  surface = eglCreatePbufferSurface(display, config, surface_attribs);
  EGL_CHECK();
#else
  // ignore unused parameter warnings
  (void)(width);
  (void)(height);
#endif
}

std::shared_ptr<RenderingContext> RenderingContext::create(
    const RenderingContext& ctx) {
  if (ctx.type == RenderingContextType::kOpenGL)
    return std::make_shared<glRenderingContext>(&ctx);
  return nullptr;
}

std::shared_ptr<RenderingContext> RenderingContext::create(
    RenderingContextType type) {
  if (type == RenderingContextType::kOpenGL)
    return std::make_shared<glRenderingContext>(nullptr);
  return nullptr;
}

class WebsocketClient {
 public:
  WebsocketClient(Scene& scene, int fd, int idx)
      : scene_(scene), fd_(fd), idx_(idx) {
    // launch the thread that listens and responds to client messages
    listener_ = std::async(std::launch::async, [this]() { return listen(); });
  }

  StatusCode listen() {
    // create a context from the server context (created by the scene in the
    // main thread) which can be used in this thread
    context_ = RenderingContext::create(scene_.context());
    scene_.onconnect();
    sendmessage("*wings server initialized!", RFC6455_OP_TEXT);

    size_t max_length = 1e5;
    std::string frame(max_length, ' ');

    int flags = fcntl(fd_, F_GETFL, 0);
    fcntl(fd_, F_SETFL, flags | O_NONBLOCK);

    struct pollfd p[1];
    p[0].fd = fd_;
    p[0].events = POLLIN;

    int timeout = 1000;
    while (true) {
      //  poll for an event on the file descriptor
      if (poll(p, 1, timeout) != -1) {
        if (p[0].revents & POLLIN) {
          // we are ready for a recv, read the frame bytes
          ssize_t n_bytes = recv(fd_, &frame[0], frame.size(), 0);
          if (n_bytes == 0) {
            close(fd_);
            return StatusCode::kListenError;
          }
          if (n_bytes < 2) continue;

          // parse the frame and then the message
          std::string message;
          if (parseframe(frame, n_bytes, message) != StatusCode::kSuccess)
            continue;
          if (message == "close") {
            close(fd_);
            return StatusCode::kSuccess;
          }

          // parse the mouse control
          ClientInput input;
          if (message[0] == 'M') {
            input.type = InputType::MouseMotion;
            input.x = std::atoi(message.substr(3, 5).c_str());
            input.y = std::atoi(message.substr(8, 5).c_str());
            if (message[1] == 'D') input.dragging = true;
            if (message[2] == 'M') input.modifier = true;
          } else if (message[0] == 'D') {
            // double click
            input.type = InputType::DoubleClick;
            input.x = std::atoi(message.substr(1, 5).c_str());
            input.y = std::atoi(message.substr(6, 5).c_str());
          } else if (message[0] == 'W') {
            // mousewheel event
            input.type = InputType::Scroll;
            input.fvalue = (message[1] == '-') ? 0.95 : 1.05;
          } else if (message[0] == 'K') {
            input.key = message[2];
            if (message[1] == 'I') {
              input.type = InputType::KeyValueInt;
              input.ivalue = std::atoi(&message[3]);
            } else if (message[1] == 'B') {
              input.type = InputType::KeyValueBool;
              input.bvalue = !(message[3] == 'f' || message[3] == 'F' ||
                               message[3] == '0');
            } else if (message[1] == 'F') {
              input.type = InputType::KeyValueFloat;
              input.fvalue = std::atof(&message[3]);
            } else if (message[1] == 'S') {
              input.type = InputType::KeyValueStr;
              input.svalue = &message[3];
            } else if (message[0] == 'A') {
              input.type = InputType::AnimationRequest;
              if (message[1] == 'L') input.looping = true;

            } else if (message[0] == 'S') {
              input.type = InputType::SetTime;
              input.time = std::atoi(message.substr(3, 9).c_str());
            } else if (message[0] == 'F') {
              input.type = InputType::SetFrames;
            }
          }

          context_->enter_render_section();
          std::string msg;
          bool updated = scene_.render(input, idx_, &msg);
          if (updated) {
            // convert the scene pixels to a jpeg
            bytes_.resize(scene_.pixels().size());
            n_bytes_ = 0;
            // std::cout << "w = " << scene_.width() << " h = " <<
            // scene_.height()
            //           << std::endl;
            stbi_flip_vertically_on_write(1);
            stbi_write_jpg_to_func(custom_stbi_write_mem, this, scene_.width(),
                                   scene_.height(), scene_.channels(),
                                   scene_.pixels().data(), scene_.quality());

            base64::encode(bytes_.c_str(), n_bytes_, img_);
            sendmessage(img_, RFC6455_OP_TEXT);
            if (!msg.empty()) sendmessage(msg, RFC6455_OP_TEXT);
          }
          context_->leave_render_section();
        }
      }
    }
    return StatusCode::kSuccess;
  }

  const auto& bytes() const { return bytes_; }
  int64_t n_bytes() const { return n_bytes_; }
  void set_n_bytes(int64_t n) { n_bytes_ = n; }

  static void custom_stbi_write_mem(void* ctx, void* data, int size) {
    auto* client = static_cast<WebsocketClient*>(ctx);
    char* dst = (char*)client->bytes().c_str();
    char* src = (char*)data;
    int64_t n = client->n_bytes();
    for (int i = 0; i < size; i++) dst[n++] = src[i];
    client->set_n_bytes(n);
  }

  std::string makeframe(const std::string& payload, int opcode) {
    // https://datatracker.ietf.org/doc/html/rfc6455#section-5.2
    std::string header(2, '\0');
    uint64_t length = payload.size();

    // set the first byte of the header
    //           FIN | RSV  1   2   3 | opcode | mask (none)
    header[0] = (128 | /**/ 0 | 0 | 0 | opcode | 0);

    if (length <= 125) {
      // the second byte needs to contain the payload length
      header[1] = length & 127;
    } else if (length >= 126 && length <= 65535) {
      // for a size between 126 and 65535, the next byte must be 126
      // then we have two more bytes (16-bit integer) representing the length
      header[1] = 126;
      header.resize(4);

      // the next two bytes (16-bit unsigned integer) represent the length
      for (int i = 2; i < 4; i++)
        header[i] = (length >> (16 - (8 * (i - 1))) &
                     255);  // 255 for 64-bit unsigned int
    } else {
      // for more than 65535 bytes, the next byte must be 127,
      // then the next 8 bytes (64-bit unsigned integer) represents the length
      header[1] = 127;
      header.resize(10);
      for (int i = 2; i < 10; i++)
        header[i] = (length >> (64 - (8 * (i - 1))) &
                     255);  // 255 for 64-bit unsigned int
    }

    // frame = header + payload data
    return header + payload;
  }

  int sendmessage(const std::string& msg, int type) {
    std::string frame = makeframe(msg, type);
    return send(fd_, frame.c_str(), frame.size(), RFC6455_OP_BINARY);
  }

 private:
  Scene& scene_;
  const int fd_;
  const int idx_;
  std::future<StatusCode> listener_;
  std::shared_ptr<RenderingContext> context_;
  std::string img_;
  std::string bytes_;
  int64_t n_bytes_{0};
};

// A websockets server which can connect to any number of clients.
// The server will render the scene according to the current client's view.
class WebsocketRenderer {
 public:
  WebsocketRenderer(Scene& scene, int port) : scene_(scene), port_(port) {}
  int port() const { return port_; }
  StatusCode start() {
    // create a socket for the server
    struct sockaddr_in server;
    int server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd < 0) return StatusCode::kCreateSocketError;

    // allow the port to be re-used so that we don't have to keep changing the
    // port number when re-running
    int enable = 1;
    size_t len = sizeof(int);
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, len) < 0)
      std::cout << "error setting socket options" << std::endl;

    // bind the server to our socket
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port_);
    if (bind(server_fd, (struct sockaddr*)&server, sizeof(server)) < 0) {
      close(server_fd);
      return StatusCode::kBindError;
    }

    // listen on the specified port
    std::cout << "--> waiting for clients on port " << port_ << std::endl;
    listen(server_fd, MAX_CLIENTS);
    run_ = true;

    scene_.context().release_context();
    server_ = std::async(std::launch::async, [this, server_fd]() {
      std::string request(4096, ' ');

      while (run_) {
        // accept a connection from the client
        struct sockaddr_in client;
        socklen_t len = sizeof(struct sockaddr_in);
        int client_fd = accept(server_fd, (struct sockaddr*)&client, &len);
        if (client_fd < 0) continue;  // no client connected

        // receive the request from the client
        int bytes_received = recv(client_fd, &request[0], request.size(), 0);
        if (bytes_received == 0) {
          close(server_fd);
          close(client_fd);
          shutdown(server_fd, 2);
          return StatusCode::kHandshakeError;
        }

        // shake hands with the client
        if (shakehands(client_fd, request) < 0) {
          close(server_fd);
          close(client_fd);
          shutdown(server_fd, 2);
          return StatusCode::kHandshakeError;
        }

        char* connected_ip = inet_ntoa(client.sin_addr);
        int client_port = ntohs(client.sin_port);
        std::cout << "--> connected to " << connected_ip << ":" << client_port
                  << "(" << client_fd << ")" << std::endl;

        // start a client thread, creating a new rendering context
        // from the server rendering context
        client_.push_back(std::make_unique<WebsocketClient>(scene_, client_fd,
                                                            client_.size()));
      }
      return StatusCode::kSuccess;
    });
    return StatusCode::kSuccess;
  }

 private:
  Scene& scene_;
  std::atomic<bool> run_;
  std::future<StatusCode> server_;
  std::vector<std::unique_ptr<WebsocketClient>> client_;
  int port_{-1};
};

RenderingServer::RenderingServer(Scene& scene, int port) {
  renderer_ = std::make_unique<WebsocketRenderer>(scene, port);
  renderer_->start();
}

RenderingServer::~RenderingServer() {}

StatusCode RenderingServer::start(const std::string& html_file, int port) {
  if (port < 0) std::cout << "no port, not starting TCP server" << std::endl;

  // start the TCP server
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr("0.0.0.0");

  int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd < 0) return StatusCode::kCreateSocketError;

  if (bind(socket_fd, (sockaddr*)&addr, sizeof(addr)) < 0)
    return StatusCode::kBindError;

  if (listen(socket_fd, MAX_CLIENTS) < 0) return StatusCode::kListenError;
  std::cout << "--> listening at " << inet_ntoa(addr.sin_addr) << ":" << port
            << std::endl;

  // create the server response
  std::ifstream f(html_file);
  std::stringstream buffer;
  buffer << f.rdbuf();
  std::string response = std::string(
                             "HTTP/1.1 200 OK\nContent-Type: "
                             "text/html\nContent-Length: ") +
                         std::to_string(buffer.str().size()) + "\n\n" +
                         buffer.str();

  // replace the websocket port label if it exists
  std::string port_label = "{WEBSOCKET_PORT}";
  auto idx = response.find(port_label);
  if (idx != std::string::npos)
    response.replace(idx, port_label.size(), std::to_string(renderer_->port()));

  // start the server thread
  listen_ = true;
  server_ = std::async(
      std::launch::async, [this, socket_fd, &addr, &response]() -> StatusCode {
        socklen_t len = sizeof(addr);
        while (listen_) {
          // TODO: sleep this thread?
          int client_fd = accept(socket_fd, (sockaddr*)&addr, &len);
          if (client_fd < 0) return StatusCode::kConnectError;

          // send the HTML page to the client
          ssize_t n_bytes = write(client_fd, response.c_str(), response.size());
          (void)(n_bytes);
          close(client_fd);
        }
        close(socket_fd);

        return StatusCode::kSuccess;
      });
  return StatusCode::kSuccess;
}

StatusCode RenderingServer::stop() {
  // renderer_->stop();
  listen_ = false;
  return server_.get();
}

void glCanvas::bind() {
  GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer));
}

void glCanvas::create() {
  GLuint gl_framebuffer;
  GL_CALL(glGenFramebuffers(1, &gl_framebuffer));
  GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, gl_framebuffer));

  GLuint gl_renderbuffer;
  GL_CALL(glGenRenderbuffers(1, &gl_renderbuffer));
  GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, gl_renderbuffer));
  GL_CALL(glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height));

  GLuint gl_depthbuffer;
  GL_CALL(glGenRenderbuffers(1, &gl_depthbuffer));
  GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, gl_depthbuffer));
  GL_CALL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width,
                                height));

  GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                    GL_RENDERBUFFER, gl_renderbuffer));
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, gl_depthbuffer);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

  framebuffer = gl_framebuffer;
  renderbuffer = gl_renderbuffer;
  depthbuffer = gl_depthbuffer;
}

void glCanvas::resize(int w, int h) {
  bind();
  release();
  width = w;
  height = h;
  create();
}

void glCanvas::release() {
  GLuint gl_renderbuffer = renderbuffer;
  GLuint gl_depthbuffer = depthbuffer;
  GLuint gl_framebuffer = framebuffer;
  if (renderbuffer >= 0) GL_CALL(glDeleteRenderbuffers(1, &gl_renderbuffer));
  if (depthbuffer >= 0) GL_CALL(glDeleteRenderbuffers(1, &gl_depthbuffer));
  if (framebuffer >= 0) GL_CALL(glDeleteFramebuffers(1, &gl_framebuffer));
}

}  // namespace wings
