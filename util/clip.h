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

#include <string>

namespace wings {

namespace detail {

static std::string clip_vs = R"(
#version 330
void main() {
  gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}

)";

static std::string clip_gs = R"(
#version 330 core

layout (points) in;
uniform mat4 u_ModelViewProjectionMatrix;
uniform mat4 u_ModelViewMatrix;

uniform vec2 u_ViewportSize = vec2(800,600);

noperspective out vec3 altitude;

layout (triangle_strip , max_vertices = 6) out;

uniform vec3 u_x0;
uniform vec3 u_x1;
uniform vec3 u_x2;
uniform vec3 u_x3;

#define LARGE_DISTANCE 1000000

void
make_triangle( vec4 x0 , vec4 x1 , vec4 x2 , int d1 , int d2 , int d3 ) {

  vec4 u = x1 - x0;
  vec4 v = x2 - x0;

  vec4 p0 = u_ModelViewProjectionMatrix * x0;
  vec4 p1 = u_ModelViewProjectionMatrix * x1;
  vec4 p2 = u_ModelViewProjectionMatrix * x2;

  vec2 q0 = p0.xy / p0.w;
  vec2 q1 = p1.xy / p1.w;
  vec2 q2 = p2.xy / p2.w;

  vec2 v1 = u_ViewportSize * (q1 - q0);
  vec2 v2 = u_ViewportSize * (q2 - q0);
  vec2 v3 = u_ViewportSize * (q2 - q1);

  float area = abs(v1.x * v2.y - v1.y * v2.x); // twice the area

  float a1 = area / length(v3);
  float a2 = area / length(v2);
  float a3 = area / length(v1);

  float h1 = (1 - d1) * LARGE_DISTANCE + d1 * a1;
  float h2 = (1 - d2) * LARGE_DISTANCE + d2 * a2;
  float h3 = (1 - d3) * LARGE_DISTANCE + d3 * a3;

  gl_Position = p0;
  altitude    = vec3(h1, 0, 0);
  EmitVertex();

  gl_Position = p1;
  altitude    = vec3(0, h2, 0);
  EmitVertex();

  gl_Position = p2;
  altitude    = vec3(0, 0, h3);

  gl_PrimitiveID = gl_PrimitiveIDIn;
  EmitVertex();

  EndPrimitive();
}

void main() {

  vec4 x0 = vec4(u_x0, 1.0);
  vec4 x1 = vec4(u_x1, 1.0);
  vec4 x2 = vec4(u_x2, 1.0);
  vec4 x3 = vec4(u_x3, 1.0);

  make_triangle(x0, x1, x2, 1, 0, 1);
  make_triangle(x0, x2, x3, 1, 1, 0);
})";

static std::string clip_fs = R"(
#version 330

layout( location = 0 ) out vec4 fragColor;

noperspective in vec3 altitude;

uniform float u_alpha = 0.1;

void main() {
  float d = min(min(altitude[0], altitude[1]), altitude[2]);
  float intensity = exp2(-0.1 * d * d);
  vec3 color = vec3(0.8, 0.2, 0.2);
  fragColor =
      intensity * vec4(color, 0.9) + (1.0 - intensity) * vec4(color, u_alpha);
})";

}  // namespace detail

}  // namespace wings