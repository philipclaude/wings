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

#include <string.h>
#include <unistd.h>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

namespace wings {

class Timer {
 public:
  void start() { a = std::chrono::high_resolution_clock::now(); }
  void stop() { b = std::chrono::high_resolution_clock::now(); }
  double seconds() const { return milliseconds() / 1000.0; }
  double milliseconds() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count();
  }

 private:
  std::chrono::time_point<std::chrono::high_resolution_clock> a, b;
};

struct Log {
  Log(std::ostream& os) : os_(os) {}
  ~Log() { os_ << std::endl; }

  template <typename T>
  std::ostream& operator<<(const T& s) && {
    return os_ << s;
  }
  std::ostream& os_;
};

struct Exception {
  Exception(std::ostream& os, const char* msg) : os_(os), msg_(msg) {}
  ~Exception() noexcept(false);

  static std::string get_backtrace(int start_frame);

  template <typename T>
  std::ostream& operator<<(const T& t) && {
    return os_ << t;
  }
  std::ostream& os_;
  const char* msg_;
};

#define __FILENAME__ \
  (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define LOG                                                                \
  wings::Log(std::cout) << "[" << getpid() << ":" << std::setw(16)         \
                        << __FILENAME__ << ":" << std::setw(4) << __LINE__ \
                        << "]: "

#define __ERR__(X)                                                      \
  wings::Exception(std::cout, X)                                        \
      << "[" << getpid() << ":" << std::setw(16) << __FILENAME__ << ":" \
      << std::setw(4) << __LINE__ << "]: "

#ifndef unlikely
#ifdef __GNUC__
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define unlikely(x) x
#endif
#endif

#define ASSERT(X)     \
  if (unlikely(!(X))) \
  __ERR__("assertion error") << "assertion " << #X " failed "
#define NOT_IMPLEMENTED __ERR__("Not implemented.")
#define NOT_POSSIBLE __ERR__("Should not be reached.")
#define NOT_CONFIGURED __ERR__("Not configured.")
#define LOGF LOG << fmt::format

#ifdef NDEBUG
#define DBG_ASSERT(X)
#else
#define DBG_ASSERT(X) ASSERT(X)
#endif

#define WINGS_ASSERT(X)                                                       \
  if (unlikely(!(X)))                                                         \
  std::cout << "[" << getpid() << ":" << std::setw(16) << __FILENAME__ << ":" \
            << std::setw(4) << __LINE__ << "]: "                              \
            << "assertion " << #X " failed "

}  // namespace wings
