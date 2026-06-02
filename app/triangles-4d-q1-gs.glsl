#version 410 core

layout (points) in;

uniform mat4 u_ModelViewProjectionMatrix;
uniform mat4 u_NormalMatrix;
uniform mat4 u_ModelViewMatrix;

uniform int u_clip;
uniform vec3 u_clip_point;
uniform vec3 u_clip_normal;

uniform int u_width;
uniform int u_height;

uniform samplerBuffer points;
uniform usamplerBuffer index;

uniform vec4 u_hyperplane_center;
uniform vec4 u_hyperplane_normal;
uniform mat4 u_BasisProjectionMatrix;

flat in int[] v_id;
flat out int id;

layout (triangle_strip , max_vertices = 4) out;

float planedot(in vec4 p) {
  return dot(u_hyperplane_center - p, u_hyperplane_normal);
}

int intersect(in float num, in vec4 p, in vec4 q, out vec3 position) {
  float den = dot(q - p, u_hyperplane_normal);
  //if (abs(den) < 1e-6) return -1; // line is parallel
  float s = num / den;
  vec4 r = p + (q - p) * s;
  position = (u_BasisProjectionMatrix * r).xyz;
  return 1;
}

int side(in float dp) {
  return (dp <= 0) ? 0 : 1;
}

void main() {
  uvec3 tri = texelFetch(index, v_id[0]).rgb;

  vec4 x[3];
  x[0] = texelFetch(points, int(tri.r));
  x[1] = texelFetch(points, int(tri.g));
  x[2] = texelFetch(points, int(tri.b));

  // determine the side of each point
  float dp[3];
  dp[0] = planedot(x[0]);
  dp[1] = planedot(x[1]);
  dp[2] = planedot(x[2]);

  int s0 = side(dp[0]);
  int s1 = side(dp[1]);
  int s2 = side(dp[2]);
  int result = s0 + 2 * s1 + 4 * s2;
  if (result == 0 || result == 7) return;

  // use s0, s1, s2 to determine the line points
  vec3 r0, r1;
  if (result == 1 || result == 6) {
    // 0-1 and 0-2
    intersect(dp[0], x[0], x[1], r0);
    intersect(dp[0], x[0], x[2], r1);
  } else if (result == 2 || result == 5) {
    // 0-1 and 1-2
    intersect(dp[0], x[0], x[1], r0);
    intersect(dp[1], x[1], x[2], r1);
  } else if (result == 3 || result == 4) {
    // 0-2 and 1-2
    intersect(dp[0], x[0], x[2], r0);
    intersect(dp[1], x[1], x[2], r1);
  }

  // clip-space and viewport coordinates of line endpoints
  vec4 p0 = u_ModelViewProjectionMatrix * vec4(r0, 1.0);
  vec4 p1 = u_ModelViewProjectionMatrix * vec4(r1, 1.0);
  vec2 q0 = p0.xy / p0.w;
  vec2 q1 = p1.xy / p1.w;

  // line tangent and normal in viewport space
  vec2 t = normalize(vec2(u_width, u_height) * (q1 - q0));
  vec2 n = vec2(-t.y, t.x) / vec2(u_width, u_height);

  const float lw = 2.0; // line width
  gl_Position = vec4((q0 + lw * n) * p0.w, p0.zw);
  id = v_id[0];
  EmitVertex();
  
  gl_Position = vec4((q0 - lw * n) * p0.w, p0.zw);
  id = v_id[0];
  EmitVertex();
  
  gl_Position = vec4((q1 + lw * n) * p1.w, p1.zw);
  id = v_id[0];
  EmitVertex();
  
  gl_Position = vec4((q1 - lw * n) * p1.w, p1.zw);
  id = v_id[0];
  EmitVertex();
  
  EndPrimitive();
}