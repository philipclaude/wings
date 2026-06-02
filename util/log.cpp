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
#include "log.h"

#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <sys/types.h>
#include <unistd.h>

#include <csignal>
#include <cstdarg>
#include <cstdlib>

namespace wings {

Exception::~Exception() noexcept(false) {
  os_ << get_backtrace(2);
  throw std::runtime_error(msg_);
}

std::string Exception::get_backtrace(int start_frame) {
  int i;
  enum { MAX_DEPTH = 50 };
  void* trace[MAX_DEPTH];
  char* demangled;
  int trace_size, status = 0;
  Dl_info dlinfo;
  const char* symname;

  std::string message = "\n";

  trace_size = backtrace(trace, MAX_DEPTH);

  for (i = start_frame; i < trace_size; i++) {
    if (!dladdr(trace[i], &dlinfo)) continue;
    symname = dlinfo.dli_sname;

    demangled = abi::__cxa_demangle(symname, NULL, 0, &status);
    if (status == 0 && demangled) symname = demangled;

    if (symname) {
      message += symname;
      message += '\n';
    }

    if (demangled) {
      free(demangled);
    }
  }
  return message;
}

}  // namespace wings
