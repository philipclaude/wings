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

#include <array>
#include <cassert>
#include <cmath>
#include <iostream>

namespace wings {

template <size_t N, typename T>
struct vec : public std::array<T, N> {
  vec() {
    for (size_t i = 0; i < N; i++) (*this)[i] = 0;
  }

  template <typename T2>
  vec(const vec<N, T2>& u) {
    for (size_t i = 0; i < N; i++) (*this)[i] = u[i];
  }

  vec(const T& v) {
    for (size_t i = 0; i < N; i++) (*this)[i] = v;
  }
  vec(const std::initializer_list<T>& v) {
    assert(v.size() == N);
    int i = 0;
    for (const auto& value : v) (*this)[i++] = value;
  }
  template <typename T2>
  vec(const T2* v, size_t n = 0) {
    if (n == 0) n = N;
    for (size_t i = 0; i < n; i++) (*this)[i] = v[i];
  }
  vec<3, T> xyz() const { return {(*this)[0], (*this)[1], (*this)[2]}; }
};  // namespace std::array

template <size_t N, typename T>
class mat {
 public:
  static constexpr size_t n_elem = N * N;
  mat() { zero(); }
  void zero() { std::fill(data_, data_ + n_elem, 0); }
  void eye() {
    zero();
    for (size_t i = 0; i < N; i++) (*this)(i, i) = 1;
  }

  T& operator()(int i, int j) { return data_[N * j + i]; }
  const T& operator()(int i, int j) const { return data_[N * j + i]; }

 private:
  T data_[N * N];
};

template <typename T>
inline T length(const vec<3, T>& u) {
  return std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
}

template <typename T>
inline T length(const vec<4, T>& u) {
  return std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2] + u[3] * u[3]);
}

template <typename T>
inline vec<3, T> unit_vector(const vec<3, T>& u) {
  T l = length(u);
  return {u[0] / l, u[1] / l, u[2] / l};
}

template <typename T>
inline vec<4, T> unit_vector(const vec<4, T>& u) {
  T l = length(u);
  return {u[0] / l, u[1] / l, u[2] / l, u[3] / l};
}

template <typename T>
inline T dot(const vec<3, T>& u, const vec<3, T>& v) {
  return u[0] * v[0] + u[1] * v[1] + u[2] * v[2];
}

template <typename T>
inline T dot(const vec<4, T>& u, const vec<4, T>& v) {
  return u[0] * v[0] + u[1] * v[1] + u[2] * v[2] + u[3] * v[3];
}

template <typename T>
inline vec<3, T> cross(const vec<3, T>& u, const vec<3, T>& v) {
  return {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
          u[0] * v[1] - u[1] * v[0]};
}

template <typename T>
inline vec<3, T> operator-(const vec<3, T>& u, const vec<3, T>& v) {
  return {u[0] - v[0], u[1] - v[1], u[2] - v[2]};
}

template <typename T>
inline vec<4, T> operator-(const vec<4, T>& u, const vec<4, T>& v) {
  return {u[0] - v[0], u[1] - v[1], u[2] - v[2], u[3] - v[3]};
}

template <typename T>
inline vec<3, T> operator+(const vec<3, T>& u, const vec<3, T>& v) {
  return {u[0] + v[0], u[1] + v[1], u[2] + v[2]};
}

template <typename T>
inline vec<4, T> operator+(const vec<4, T>& u, const vec<4, T>& v) {
  return {u[0] + v[0], u[1] + v[1], u[2] + v[2], u[3] + v[3]};
}

template <typename T, typename R,
          class = typename std::enable_if<std::is_scalar<R>::value>::type>
inline vec<3, T> operator*(const vec<3, T>& u, const R& a) {
  return {u[0] * a, u[1] * a, u[2] * a};
}

template <typename T, typename R,
          class = typename std::enable_if<std::is_scalar<R>::value>::type>
inline vec<3, T> operator*(const R& a, const vec<3, T>& u) {
  return {u[0] * a, u[1] * a, u[2] * a};
}

template <typename T, typename R,
          class = typename std::enable_if<std::is_scalar<R>::value>::type>
inline vec<4, T> operator*(const vec<4, T>& u, const R& a) {
  return {u[0] * a, u[1] * a, u[2] * a, u[3] * a};
}

template <typename R, typename T,
          class = typename std::enable_if<std::is_scalar<R>::value>::type>
inline vec<4, T> operator*(const R& a, const vec<4, T>& u) {
  return {u[0] * a, u[1] * a, u[2] * a, u[3] * a};
}

template <typename T, typename R,
          class = typename std::enable_if<std::is_scalar<R>::value>::type>
inline vec<3, T> operator/(const vec<3, T>& u, const R& a) {
  return {u[0] / a, u[1] / a, u[2] / a};
}

template <size_t N, typename T>
inline mat<N, T> operator*(const mat<N, T>& a, const mat<N, T>& b) {
  mat<N, T> c;
  for (size_t i = 0; i < N; i++) {
    for (size_t j = 0; j < N; j++) {
      T sum = 0;
      for (size_t k = 0; k < N; k++) sum += a(i, k) * b(k, j);
      c(i, j) = sum;
    }
  }
  return c;
}

template <size_t N, typename T>
inline vec<N, T> operator*(const mat<N, T>& a, const vec<N, T>& x) {
  vec<N, T> b;
  for (size_t i = 0; i < N; i++) {
    b[i] = 0;
    for (size_t j = 0; j < N; j++) {
      b[i] += a(i, j) * x[j];
    }
  }
  return b;
}

using vec2f = vec<2, float>;
using vec3f = vec<3, float>;
using vec3d = vec<3, double>;
using vec4f = vec<4, float>;
using mat4f = mat<4, float>;
using mat3f = mat<3, float>;

namespace glm {

inline mat4f lookat(const vec3f& eye, const vec3f& center, const vec3f& up) {
  mat4f m;

  vec3f w = unit_vector(eye - center);
  vec3f u = unit_vector(cross(up, w));
  vec3f v = cross(w, u);

  m(0, 0) = u[0];
  m(1, 0) = v[0];
  m(2, 0) = w[0];
  m(0, 1) = u[1];
  m(1, 1) = v[1];
  m(2, 1) = w[1];
  m(0, 2) = u[2];
  m(1, 2) = v[2];
  m(2, 2) = w[2];

  m(0, 3) = -eye[0] * u[0] - eye[1] * u[1] - eye[2] * u[2];
  m(1, 3) = -eye[0] * v[0] - eye[1] * v[1] - eye[2] * v[2];
  m(2, 3) = -eye[0] * w[0] - eye[1] * w[1] - eye[2] * w[2];
  m(3, 3) = 1.0;

  return m;
}

inline mat4f perspective(float fov, float aspect, float n, float f) {
  mat4f m;
  m.eye();
  float a = 1.0 / tan(fov / 2.0);

  m(0, 0) = a / aspect;
  m(1, 1) = a;
  m(2, 2) = (f + n) / (n - f);
  m(2, 3) = 2 * f * n / (n - f);
  m(3, 2) = -1.0;
  m(3, 3) = 0.0;
  return m;
}

inline mat4f rotation(double X, double Y) {
  float X2 = X * X, Y2 = Y * Y;
  float q = 1 + X2 + Y2;
  float s = 1 - X2 - Y2;
  float r2 = 1 / (q * q), s2 = s * s;
  float A = (s2 + 4 * (Y2 - X2)) * r2;
  float B = -8 * X * Y * r2;
  float C = 4 * s * X * r2;
  float D = (s2 + 4 * (X2 - Y2)) * r2;
  float E = 4 * s * Y * r2;
  float F = (s2 - 4 * (X2 + Y2)) * r2;

  mat4f R;  // initializes to zero
  R(0, 0) = A;
  R(1, 0) = B;
  R(2, 0) = C;
  R(0, 1) = B;
  R(1, 1) = D;
  R(2, 1) = E;
  R(0, 2) = -C;
  R(1, 2) = -E;
  R(2, 2) = F;
  R(3, 3) = 1;
  return R;
}

inline mat4f rotate(const mat4f& a, float angle, const vec3f& axis) {
  mat4f m;
  float x = axis[0], y = axis[1], z = axis[2];
  float len = 1.0 / length(axis);

  x *= len;
  y *= len;
  z *= len;

  float s = sin(angle), c = cos(angle), t = 1 - c;

  float b00 = x * x * t + c;
  float b01 = y * x * t + z * s;
  float b02 = z * x * t - y * s;
  float b10 = x * y * t - z * s;
  float b11 = y * y * t + c;
  float b12 = z * y * t + x * s;
  float b20 = x * z * t + y * s;
  float b21 = y * z * t - x * s;
  float b22 = z * z * t + c;

  float a00 = a(0, 0);
  float a01 = a(1, 0);
  float a02 = a(2, 0);
  float a03 = a(3, 0);
  float a10 = a(0, 1);
  float a11 = a(1, 1);
  float a12 = a(2, 1);
  float a13 = a(3, 1);
  float a20 = a(0, 2);
  float a21 = a(1, 2);
  float a22 = a(2, 2);
  float a23 = a(3, 2);

  m(0, 0) = a00 * b00 + a10 * b01 + a20 * b02;
  m(1, 0) = a01 * b00 + a11 * b01 + a21 * b02;
  m(2, 0) = a02 * b00 + a12 * b01 + a22 * b02;
  m(3, 0) = a03 * b00 + a13 * b01 + a23 * b02;
  m(0, 1) = a00 * b10 + a10 * b11 + a20 * b12;
  m(1, 1) = a01 * b10 + a11 * b11 + a21 * b12;
  m(2, 1) = a02 * b10 + a12 * b11 + a22 * b12;
  m(3, 1) = a03 * b10 + a13 * b11 + a23 * b12;
  m(0, 2) = a00 * b20 + a10 * b21 + a20 * b22;
  m(1, 2) = a01 * b20 + a11 * b21 + a21 * b22;
  m(2, 2) = a02 * b20 + a12 * b21 + a22 * b22;
  m(3, 2) = a03 * b20 + a13 * b21 + a23 * b22;

  m(0, 3) = a(0, 3);
  m(1, 3) = a(1, 3);
  m(2, 3) = a(2, 3);
  m(3, 3) = a(3, 3);

  return m;
}

inline mat4f translation(float dx, float dy) {
  mat4f T;
  T.eye();

  // compute the transformation in screen space
  vec3f t;
  t[0] = dx;
  t[1] = dy;
  t[2] = 0.0f;

  T(0, 3) = T(0, 0) * t[0] + T(0, 1) * t[1] + T(0, 2) * t[2] + T(0, 3);
  T(1, 3) = T(1, 0) * t[0] + T(1, 1) * t[1] + T(1, 2) * t[2] + T(1, 3);
  T(2, 3) = T(2, 0) * t[0] + T(2, 1) * t[1] + T(2, 2) * t[2] + T(2, 3);
  T(3, 3) = T(3, 0) * t[0] + T(3, 1) * t[1] + T(3, 2) * t[2] + T(3, 3);

  return T;
}

inline mat4f translate(const mat4f& a, const vec3f& t) {
  mat4f m;
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++) m(i, j) = a(i, j);

  m(0, 3) = a(0, 0) * t[0] + a(0, 1) * t[1] + a(0, 2) * t[2] + a(0, 3);
  m(1, 3) = a(1, 0) * t[0] + a(1, 1) * t[1] + a(1, 2) * t[2] + a(1, 3);
  m(2, 3) = a(2, 0) * t[0] + a(2, 1) * t[1] + a(2, 2) * t[2] + a(2, 3);
  m(3, 3) = a(3, 0) * t[0] + a(3, 1) * t[1] + a(3, 2) * t[2] + a(3, 3);

  return m;
}

inline mat4f transpose(const mat4f& a) {
  mat4f m;
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++) m(i, j) = a(j, i);
  return m;
}

inline float det(const mat3f& m) {
  return m(0, 0) * (m(2, 2) * m(1, 1) - m(2, 1) * m(1, 2)) -
         m(1, 0) * (m(2, 2) * m(0, 1) - m(2, 1) * m(0, 2)) +
         m(2, 0) * (m(1, 2) * m(0, 1) - m(1, 1) * m(0, 2));
}

inline float det(const mat4f m) {
  const auto X1_1 = m(0, 0), X1_2 = m(0, 1), X1_3 = m(0, 2), X1_4 = m(0, 3);
  const auto X2_1 = m(1, 0), X2_2 = m(1, 1), X2_3 = m(1, 2), X2_4 = m(1, 3);
  const auto X3_1 = m(2, 0), X3_2 = m(2, 1), X3_3 = m(2, 2), X3_4 = m(2, 3);
  const auto X4_1 = m(3, 0), X4_2 = m(3, 1), X4_3 = m(3, 2), X4_4 = m(3, 3);
  return X1_1 * X2_2 * X3_3 * X4_4 - X1_1 * X2_2 * X3_4 * X4_3 -
         X1_1 * X2_3 * X3_2 * X4_4 + X1_1 * X2_3 * X3_4 * X4_2 +
         X1_1 * X2_4 * X3_2 * X4_3 - X1_1 * X2_4 * X3_3 * X4_2 -
         X1_2 * X2_1 * X3_3 * X4_4 + X1_2 * X2_1 * X3_4 * X4_3 +
         X1_2 * X2_3 * X3_1 * X4_4 - X1_2 * X2_3 * X3_4 * X4_1 -
         X1_2 * X2_4 * X3_1 * X4_3 + X1_2 * X2_4 * X3_3 * X4_1 +
         X1_3 * X2_1 * X3_2 * X4_4 - X1_3 * X2_1 * X3_4 * X4_2 -
         X1_3 * X2_2 * X3_1 * X4_4 + X1_3 * X2_2 * X3_4 * X4_1 +
         X1_3 * X2_4 * X3_1 * X4_2 - X1_3 * X2_4 * X3_2 * X4_1 -
         X1_4 * X2_1 * X3_2 * X4_3 + X1_4 * X2_1 * X3_3 * X4_2 +
         X1_4 * X2_2 * X3_1 * X4_3 - X1_4 * X2_2 * X3_3 * X4_1 -
         X1_4 * X2_3 * X3_1 * X4_2 + X1_4 * X2_3 * X3_2 * X4_1;
}

inline mat3f inverse(const mat3f& m) {
  mat3f minv;
  const auto idetM = 1.0 / det(m);
  const auto a1_1 = m(0, 0);
  const auto a1_2 = m(0, 1);
  const auto a1_3 = m(0, 2);
  const auto a2_1 = m(1, 0);
  const auto a2_2 = m(1, 1);
  const auto a2_3 = m(1, 2);
  const auto a3_1 = m(2, 0);
  const auto a3_2 = m(2, 1);
  const auto a3_3 = m(2, 2);
  minv(0, 0) = (a2_2 * a3_3 - a2_3 * a3_2) * idetM;
  minv(0, 1) = (a1_3 * a3_2 - a1_2 * a3_3) * idetM;
  minv(0, 2) = (a1_2 * a2_3 - a1_3 * a2_2) * idetM;
  minv(1, 0) = (a2_3 * a3_1 - a2_1 * a3_3) * idetM;
  minv(1, 1) = (a1_1 * a3_3 - a1_3 * a3_1) * idetM;
  minv(1, 2) = (a1_3 * a2_1 - a1_1 * a2_3) * idetM;
  minv(2, 0) = (a2_1 * a3_2 - a2_2 * a3_1) * idetM;
  minv(2, 1) = (a1_2 * a3_1 - a1_1 * a3_2) * idetM;
  minv(2, 2) = (a1_1 * a2_2 - a1_2 * a2_1) * idetM;
  return minv;
}

inline mat4f inverse(const mat4f& m) {
  mat4f minv;
  const auto idetM = 1.0 / det(m);
  const auto a1_1 = m(0, 0);
  const auto a1_2 = m(0, 1);
  const auto a1_3 = m(0, 2);
  const auto a1_4 = m(0, 3);
  const auto a2_1 = m(1, 0);
  const auto a2_2 = m(1, 1);
  const auto a2_3 = m(1, 2);
  const auto a2_4 = m(1, 3);
  const auto a3_1 = m(2, 0);
  const auto a3_2 = m(2, 1);
  const auto a3_3 = m(2, 2);
  const auto a3_4 = m(2, 3);
  const auto a4_1 = m(3, 0);
  const auto a4_2 = m(3, 1);
  const auto a4_3 = m(3, 2);
  const auto a4_4 = m(3, 3);

  minv(0, 0) = (a2_2 * a3_3 * a4_4 - a2_2 * a3_4 * a4_3 - a2_3 * a3_2 * a4_4 +
                a2_3 * a3_4 * a4_2 + a2_4 * a3_2 * a4_3 - a2_4 * a3_3 * a4_2) *
               idetM;
  minv(0, 1) = (-a1_2 * a3_3 * a4_4 + a1_2 * a3_4 * a4_3 + a1_3 * a3_2 * a4_4 -
                a1_3 * a3_4 * a4_2 - a1_4 * a3_2 * a4_3 + a1_4 * a3_3 * a4_2) *
               idetM;
  minv(0, 2) = (a1_2 * a2_3 * a4_4 - a1_2 * a2_4 * a4_3 - a1_3 * a2_2 * a4_4 +
                a1_3 * a2_4 * a4_2 + a1_4 * a2_2 * a4_3 - a1_4 * a2_3 * a4_2) *
               idetM;
  minv(0, 3) = (-a1_2 * a2_3 * a3_4 + a1_2 * a2_4 * a3_3 + a1_3 * a2_2 * a3_4 -
                a1_3 * a2_4 * a3_2 - a1_4 * a2_2 * a3_3 + a1_4 * a2_3 * a3_2) *
               idetM;
  minv(1, 0) = (-a2_1 * a3_3 * a4_4 + a2_1 * a3_4 * a4_3 + a2_3 * a3_1 * a4_4 -
                a2_3 * a3_4 * a4_1 - a2_4 * a3_1 * a4_3 + a2_4 * a3_3 * a4_1) *
               idetM;
  minv(1, 1) = (a1_1 * a3_3 * a4_4 - a1_1 * a3_4 * a4_3 - a1_3 * a3_1 * a4_4 +
                a1_3 * a3_4 * a4_1 + a1_4 * a3_1 * a4_3 - a1_4 * a3_3 * a4_1) *
               idetM;
  minv(1, 2) = (-a1_1 * a2_3 * a4_4 + a1_1 * a2_4 * a4_3 + a1_3 * a2_1 * a4_4 -
                a1_3 * a2_4 * a4_1 - a1_4 * a2_1 * a4_3 + a1_4 * a2_3 * a4_1) *
               idetM;
  minv(1, 3) = (a1_1 * a2_3 * a3_4 - a1_1 * a2_4 * a3_3 - a1_3 * a2_1 * a3_4 +
                a1_3 * a2_4 * a3_1 + a1_4 * a2_1 * a3_3 - a1_4 * a2_3 * a3_1) *
               idetM;
  minv(2, 0) = (a2_1 * a3_2 * a4_4 - a2_1 * a3_4 * a4_2 - a2_2 * a3_1 * a4_4 +
                a2_2 * a3_4 * a4_1 + a2_4 * a3_1 * a4_2 - a2_4 * a3_2 * a4_1) *
               idetM;
  minv(2, 1) = (-a1_1 * a3_2 * a4_4 + a1_1 * a3_4 * a4_2 + a1_2 * a3_1 * a4_4 -
                a1_2 * a3_4 * a4_1 - a1_4 * a3_1 * a4_2 + a1_4 * a3_2 * a4_1) *
               idetM;
  minv(2, 2) = (a1_1 * a2_2 * a4_4 - a1_1 * a2_4 * a4_2 - a1_2 * a2_1 * a4_4 +
                a1_2 * a2_4 * a4_1 + a1_4 * a2_1 * a4_2 - a1_4 * a2_2 * a4_1) *
               idetM;
  minv(2, 3) = (-a1_1 * a2_2 * a3_4 + a1_1 * a2_4 * a3_2 + a1_2 * a2_1 * a3_4 -
                a1_2 * a2_4 * a3_1 - a1_4 * a2_1 * a3_2 + a1_4 * a2_2 * a3_1) *
               idetM;
  minv(3, 0) = (-a2_1 * a3_2 * a4_3 + a2_1 * a3_3 * a4_2 + a2_2 * a3_1 * a4_3 -
                a2_2 * a3_3 * a4_1 - a2_3 * a3_1 * a4_2 + a2_3 * a3_2 * a4_1) *
               idetM;
  minv(3, 1) = (a1_1 * a3_2 * a4_3 - a1_1 * a3_3 * a4_2 - a1_2 * a3_1 * a4_3 +
                a1_2 * a3_3 * a4_1 + a1_3 * a3_1 * a4_2 - a1_3 * a3_2 * a4_1) *
               idetM;
  minv(3, 2) = (-a1_1 * a2_2 * a4_3 + a1_1 * a2_3 * a4_2 + a1_2 * a2_1 * a4_3 -
                a1_2 * a2_3 * a4_1 - a1_3 * a2_1 * a4_2 + a1_3 * a2_2 * a4_1) *
               idetM;
  minv(3, 3) = (a1_1 * a2_2 * a3_3 - a1_1 * a2_3 * a3_2 - a1_2 * a2_1 * a3_3 +
                a1_2 * a2_3 * a3_1 + a1_3 * a2_1 * a3_2 - a1_3 * a2_2 * a3_1) *
               idetM;

  return minv;
}

}  // namespace glm

}  // namespace wings