#version 410 core

uniform mat4 u_ModelViewProjectionMatrix;
uniform mat4 u_NormalMatrix;
uniform mat4 u_ModelViewMatrix;

uniform int u_width;
uniform int u_height;
uniform int u_type; // 0 for tetrahedra, 1 for pentatopes

uniform samplerBuffer points;
uniform usamplerBuffer index;
uniform usamplerBuffer aux;
uniform isamplerBuffer hidden;

uniform vec4 u_hyperplane_center;
uniform vec4 u_hyperplane_normal;
uniform mat4 u_BasisProjectionMatrix;

// inputs
flat in int[] v_id;

// outputs
out vec3 v_Position;
noperspective out vec3 v_Altitude;
flat out int v_CellNumber;
out vec3 v_Normal;
flat out int v_Group;

layout (points) in;
layout (triangle_strip , max_vertices = 6) out;

#define LARGE_DISTANCE 1000000

void make_triangle(vec3 x0 , vec3 x1 , vec3 x2, int d0, int d1, int d2, int cell_id, int group_id) {

  vec2 viewport = vec2(u_width, u_height);

  vec3 u = x1 - x0;
  vec3 v = x2 - x0;
  vec3 n = normalize(mat3(u_NormalMatrix) * cross(u, v));
  //if (n[2] < 0) return; // normal is facing away

  vec4 p0 = u_ModelViewProjectionMatrix * vec4(x0, 1);
  vec4 p1 = u_ModelViewProjectionMatrix * vec4(x1, 1);
  vec4 p2 = u_ModelViewProjectionMatrix * vec4(x2, 1);

  vec2 q0 = p0.xy / p0.w;
  vec2 q1 = p1.xy / p1.w;
  vec2 q2 = p2.xy / p2.w;

  vec2 v1 = viewport * (q1 - q0);
  vec2 v2 = viewport * (q2 - q0);
  vec2 v3 = viewport * (q2 - q1);

  float area = abs(v1.x * v2.y - v1.y * v2.x); // twice the area

  float a0 = area / length(v3);
  float a1 = area / length(v2);
  float a2 = area / length(v1);

  float h0 = (1 - d0) * LARGE_DISTANCE + d0 * a0;
  float h1 = (1 - d1) * LARGE_DISTANCE + d1 * a1;
  float h2 = (1 - d2) * LARGE_DISTANCE + d2 * a2;

  gl_Position = p0;
  v_Position  = (u_ModelViewMatrix * vec4(x0, 1)).xyz;
  v_Altitude    = vec3(h0, 0, 0);
  v_CellNumber = cell_id;
  v_Group = group_id;
  v_Normal = n;
  EmitVertex();

  gl_Position = p1;
  v_Position  = (u_ModelViewMatrix * vec4(x1, 1)).xyz;
  v_Altitude    = vec3(0, h1, 0);
  v_CellNumber = cell_id;
  v_Group = group_id;
  v_Normal = n;
  EmitVertex();

  gl_Position = p2;
  v_Position  = (u_ModelViewMatrix * vec4(x2, 1)).xyz;
  v_Altitude    = vec3(0, 0, h2);
  v_CellNumber = cell_id;
  v_Group = group_id;
  v_Normal = n;

  gl_PrimitiveID = gl_PrimitiveIDIn;
  EmitVertex();

  EndPrimitive();
}

float planedot(in vec4 p) {
  return dot(u_hyperplane_center - p, u_hyperplane_normal);
}

int side(in float dp) {
  return int(sign(dp) + 1 - 1e-6);
  //return (dp <= 0) ? 0 : 1;
}

int intersect(in float num, in vec4 p, in vec4 q, out vec3 position) {
  float den = dot(q - p, u_hyperplane_normal);
  //if (abs(den) < 1e-6) return -1; // line is parallel
  float s = num / den;
  vec4 r = p + (q - p) * s;
  position = (u_BasisProjectionMatrix * r).xyz;
  return 1;
}

// result -> edge table
// 0: 0-1
// 1: 0-2
// 2: 0-3
// 3: 1-2
// 4: 1-3
// 5: 2-3
const int rtab[64] = int[](
  -1, -1, -1, -1, // 0 (0)
  0, 1, 2, 0, // 1 (3)
  0, 3, 4, 0, // 2 (3)
  4, 3, 2, 1, // 3 (4)
  1, 3, 5, 1, // 4 (3)
  5, 3, 2, 0, // 5 (4)
  5, 4, 1, 0, // 6 (4)
  4, 2, 5, 4, // 7 (3)
  4, 2, 5, 4, // 8 (3)
  5, 4, 1, 0, // 9 (4)
  5, 3, 2, 0, // 10 (4)
  1, 3, 5, 1, // 11 (3)
  4, 3, 2, 1, // 12 (4)
  0, 3, 4, 0, // 13 (3)
  0, 1, 2, 0, // 14 (3)
  -1, -1, -1, -1 // 15 (0)
);

// shape table (0: none, 1: triangle, 2: quad)
const int stab[16] = int[](
  0, // 0
  1, // 1
  1, // 2
  2, // 3
  1, // 4
  2, // 5
  2, // 6 
  1, // 7
  1, // 8
  2, // 9
  2, // 10
  1, // 11
  2, // 12
  1, // 13
  1, // 14
  0  // 15
);

// number of intersections table
const int itab[3] = int[](0, 3, 4);

// edge table
const int etab[12] = int[](
  0, 1, // 0
  0, 2, // 1
  0, 3, // 2
  1, 2, // 3
  1, 3, // 4
  2, 3  // 5
);

// visibility table
const int vtab[18] = int[](
  0, 0, 0, 0, 0, 0,
  1, 1, 1, 0, 0, 0,
  0, 1, 1, 1, 0, 1
);

void main() {

  // index of the tetrahedron being processed
  int tet_id = v_id[0];

  // the aux texture either holds:
  // 1. cell id when u_type is 1 (pentatopes)
  // 2. group_id when u_type is 0 (tetrahedra)
  int aux_data = int(texelFetch(aux, tet_id).r);
  int cell_id = u_type * aux_data + (1 - u_type) * tet_id;
  int group_id = (1 - u_type) * aux_data + u_type * cell_id;

  if (group_id >= 0) {
    int h = texelFetch(hidden, group_id).r;
    if (h == 1) return;
  }

  // tet vertex indices and coordinates
  uvec4 tet = texelFetch(index, tet_id).rgba;
  vec4 x[4];
  x[0] = texelFetch(points, int(tet.r));
  x[1] = texelFetch(points, int(tet.g));
  x[2] = texelFetch(points, int(tet.b));
  x[3] = texelFetch(points, int(tet.a));

  // determine the side of each point
  float dp[4];
  dp[0] = planedot(x[0]);
  dp[1] = planedot(x[1]);
  dp[2] = planedot(x[2]);
  dp[3] = planedot(x[3]);
  int s0 = side(dp[0]);
  int s1 = side(dp[1]);
  int s2 = side(dp[2]);
  int s3 = side(dp[3]);

  // decimal representation of the four-bit result of the intersection
  int result = s0 + (s1 << 1) + (s2 << 2) + (s3 << 3);

  // check for: (no intersection) or (triangle and out of range)
  int shape = stab[result];
  int v_intersect = itab[shape];
  if (v_intersect == 0) return;

  // get the vertices in the intersection
  int edge = rtab[4 * result + 0];
  vec3 pa;
  intersect(dp[etab[2 * edge]], x[etab[2 * edge]], x[etab[2 * edge + 1]], pa);

  edge = rtab[4 * result + 1];
  vec3 pb;
  intersect(dp[etab[2 * edge]], x[etab[2 * edge]], x[etab[2 * edge + 1]], pb);

  edge = rtab[4 * result + 2];
  vec3 pc;
  intersect(dp[etab[2 * edge]], x[etab[2 * edge]], x[etab[2 * edge + 1]], pc);

  int v0 = vtab[6 * shape];
  int v1 = vtab[6 * shape + 1];
  int v2 = vtab[6 * shape + 2];
  make_triangle(pa, pb, pc, v0, v1, v2, cell_id, group_id);
  //if (v_intersect == 3) return; // triangle

  vec3 pd;
  edge = rtab[4 * result + 3];
  intersect(dp[etab[2 * edge]], x[etab[2 * edge]], x[etab[2 * edge + 1]], pd);

  v0 = vtab[6 * shape + 3];
  v1 = vtab[6 * shape + 4];
  v2 = vtab[6 * shape + 5];
  make_triangle(pb, pd, pc, v0, v1, v2, cell_id, group_id);
}
