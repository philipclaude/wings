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
#pragma once

#define GL_GLEXT_PROTOTYPES
#ifdef __APPLE__
#define __gl_h_
#define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#define GL_SILENCE_DEPRECATION
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

#define WINGS360_GL_VERSION_MAJOR 3
#define WINGS360_GL_VERSION_MINOR 3

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
