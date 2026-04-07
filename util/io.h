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

#include <algorithm>
#include <string>

#include "mesh.h"

namespace wings {

class Mesh;

namespace meshb {
void read(const std::string& filename, Mesh& mesh);
}  // namespace meshb

namespace obj {
void read(const std::string& filename, Mesh& mesh);
void write(const Mesh& mesh, const std::string& filename);
}  // namespace obj

inline std::string get_file_ext(const std::string& filename) {
  std::string::size_type idx;
  idx = filename.rfind('.');  // find the '.' in reverse order
  if (idx != std::string::npos) return filename.substr(idx + 1);
  return "";
}

inline void read_mesh(const std::string& filename, Mesh& mesh) {
  std::string ext = get_file_ext(filename);
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (ext == "mesh" || ext == "meshb")
    meshb::read(filename, mesh);
  else if (ext == "obj")
    obj::read(filename, mesh);
  else
    NOT_IMPLEMENTED;
}

}  // namespace wings