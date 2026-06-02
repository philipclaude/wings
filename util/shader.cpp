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
#include "shader.h"

#include "glm.h"
#include "log.h"
#include "opengl.h"

namespace wings {

namespace {
std::string get_shader(const std::string& filename) {
  std::string content;
  std::ifstream file_stream(filename.c_str(), std::ios::in);

  WINGS_ASSERT(file_stream.is_open()) << "could not read file " << filename;

  std::string line = "";
  while (!file_stream.eof()) {
    std::getline(file_stream, line);
    content.append(line + "\n");
  }

  file_stream.close();
  return content;
}
}  // namespace

ShaderProgram::ShaderProgram() {
  handle_ = glCreateProgram();
  WINGS_ASSERT(handle_ >= 0);
}

void ShaderProgram::set_source(const std::string& dir, const std::string& name,
                               bool with_geometry, bool with_tessellation,
                               const std::vector<std::string>& macros) {
  macros_ = macros;

  std::string base = dir + name;

  // compile the program using each stage, and then link
  std::string vs = get_shader(base + "-vs.glsl");
  std::string fs = get_shader(base + "-fs.glsl");
  std::string gs, tcs, tes;
  if (with_geometry) gs = get_shader(base + "-gs.glsl");
  if (with_tessellation) {
    tcs = get_shader(base + "-tcs.glsl");
    tes = get_shader(base + "-tes.glsl");
  }
  compile(vs, fs, gs, tcs, tes);
}

void ShaderProgram::compile(const std::string& src, int itype) {
  WINGS_ASSERT(handle_ >= 0);
  GLenum type = itype;

  GLuint shader = glCreateShader(type);

  std::string source;
  if (macros_.size() > 0) {
    std::size_t idx = src.find("#version");
    idx = src.find("\n");

    std::string source1 = "//" + src.substr(0, idx);  // comment out version
    std::string source2 = "\n";
    for (auto& s : macros_) source2 += s + "\n";
    std::string source3 = src.substr(idx + 1, src.size());
    source = source1 + source2 + source3;
  } else
    source = src;

  const auto* c = source.data();
  GL_CALL(glShaderSource(shader, 1, &c, NULL));
  GL_CALL(glCompileShader(shader));

  // check for errors
  int result;
  GL_CALL(glGetShaderiv(shader, GL_COMPILE_STATUS, &result));
  if (result == GL_FALSE) {
    int length = 0;
    GL_CALL(glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length));
    if (length > 0) {
      std::string log(length, ' ');
      int written = 0;
      GL_CALL(glGetShaderInfoLog(shader, length, &written, &log[0]));
      std::cout << log << std::endl;
    }
    // std::cout << "shader source:\n" << source << std::endl;
  }
  WINGS_ASSERT(result == GL_TRUE) << "failed to compile shader stage " << type;

  // compile succeeded, attach shader to program
  GL_CALL(glAttachShader(handle_, shader));
}

void ShaderProgram::compile(const std::string& vs, const std::string& fs,
                            const std::string& gs, const std::string& tcs,
                            const std::string& tes) {
  // compile each shader stage
  compile(vs, GL_VERTEX_SHADER);
  compile(fs, GL_FRAGMENT_SHADER);
  if (!gs.empty()) compile(gs, GL_GEOMETRY_SHADER);
  if (!tcs.empty()) {
    WINGS_ASSERT(!tes.empty());
    compile(tcs, GL_TESS_CONTROL_SHADER);
    compile(tes, GL_TESS_EVALUATION_SHADER);
  }

  // link the program
  GL_CALL(glLinkProgram(handle_));

  int status = 0;
  GL_CALL(glGetProgramiv(handle_, GL_LINK_STATUS, &status));
  if (status == GL_FALSE) {
    int length = 0;
    GL_CALL(glGetProgramiv(handle_, GL_INFO_LOG_LENGTH, &length));
    if (length > 0) {
      std::string log(length, '\0');
      int written = 0;
      GL_CALL(glGetProgramInfoLog(handle_, length, &written, &log[0]));
      LOG << "GLSL compiler log:\n" << log;
    }
  }
  WINGS_ASSERT(status == GL_TRUE) << "failed to link program";
}

void ShaderProgram::use() const {
  WINGS_ASSERT(handle_ >= 0);
  GL_CALL(glUseProgram(handle_));
}

template <>
void ShaderProgram::set_uniform(const char* name, const vec4f& v) const {
  GLint location = glGetUniformLocation(handle_, name);
  if (location >= 0) GL_CALL(glUniform4f(location, v[0], v[1], v[2], v[3]));
}

template <>
void ShaderProgram::set_uniform(const char* name, const vec3f& v) const {
  GLint location = glGetUniformLocation(handle_, name);
  if (location >= 0) GL_CALL(glUniform3f(location, v[0], v[1], v[2]));
}

template <>
void ShaderProgram::set_uniform(const char* name, const vec2f& v) const {
  GLint location = glGetUniformLocation(handle_, name);
  if (location >= 0) GL_CALL(glUniform2f(location, v[0], v[1]));
}

template <>
void ShaderProgram::set_uniform(const char* name, const int& x) const {
  GLint location = glGetUniformLocation(handle_, name);
  if (location >= 0) GL_CALL(glUniform1i(location, x));
}

template <>
void ShaderProgram::set_uniform(const char* name, const float& x) const {
  GLint location = glGetUniformLocation(handle_, name);
  if (location >= 0) GL_CALL(glUniform1f(location, x));
}

template <>
void ShaderProgram::set_uniform(const char* name, const mat4f& m) const {
  GLint location = glGetUniformLocation(handle_, name);
  if (location >= 0)
    GL_CALL(glUniformMatrix4fv(location, 1, GL_FALSE, &m(0, 0)));
}

}  // namespace wings