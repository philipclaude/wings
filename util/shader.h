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

#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#ifndef WINGS_SOURCE_DIR
#define WINGS_SOURCE_DIR "undefined"  // to silence vs-code warning
#endif

namespace wings {

class ShaderProgram {
 public:
  ShaderProgram();
  void set_source(const std::string& base, const std::string& name,
                  bool with_geometry, bool with_tessellation,
                  const std::vector<std::string>& macros);
  void compile(const std::string& src, int type);
  void compile(const std::string& vs, const std::string& fs,
               const std::string& gs, const std::string& tcs,
               const std::string& tes);

  template <typename T>
  void set_uniform(const char* name, const T& value) const;
  void use() const;
  int handle() const { return handle_; }

 private:
  int handle_{-1};
  std::vector<std::string> macros_;
};

}  // namespace wings