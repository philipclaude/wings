#version 410

uniform mat4 u_ModelViewMatrix;
uniform mat4 u_ModelViewProjectionMatrix;
uniform usamplerBuffer aux;
uniform samplerBuffer points;
uniform usamplerBuffer group;

uniform int u_width;
uniform int u_height;

out vec3 v_Barycentric;
out vec3 v_Position;
flat out vec3 v_Normal;
noperspective out vec3 v_Altitude;

flat out int v_TriangleID;
flat out int v_CellNumber;
flat out int v_Group;

#define FAR 1000000

void main() {

  v_TriangleID = int(gl_VertexID / 3);
  uint auxData = texelFetch(aux, v_TriangleID).r;
  int localVertexIndex = gl_VertexID % 3;
  v_Group = int(texelFetch(group, v_TriangleID).r);

  // object-space coordinates of all triangle vertices
  vec3 x[3] = vec3[](
    texelFetch(points, 3 * v_TriangleID).rgb,
    texelFetch(points, 3 * v_TriangleID + 1).rgb,
    texelFetch(points, 3 * v_TriangleID + 2).rgb
  );

  // clip space coordinates of all triangle vertices
  vec4 p[3] = vec4[](
    u_ModelViewProjectionMatrix * vec4(x[0], 1),
    u_ModelViewProjectionMatrix * vec4(x[1], 1),
    u_ModelViewProjectionMatrix * vec4(x[2], 1)
  );

  // clip-space and camera-space coordinates of the vertex
  gl_Position = p[localVertexIndex];
  v_Position = vec3(u_ModelViewMatrix * vec4(x[localVertexIndex], 1.0));

  vec2 viewport = vec2(u_width, u_height);

  vec3 tx = mat3(u_ModelViewMatrix) * (x[1] - x[0]);
  vec3 ty = mat3(u_ModelViewMatrix) * (x[2] - x[0]);
  v_Normal = normalize(cross(tx, ty));

  vec2 q0 = p[0].xy / p[0].w;
  vec2 q1 = p[1].xy / p[1].w;
  vec2 q2 = p[2].xy / p[2].w;

  // edge lengths in screen space
  vec2 v[3] = vec2[](viewport * (q2 - q1),
                     viewport * (q2 - q0),
                     viewport * (q1 - q0));
  
  // twice the area in screen space and altitude (h)
  float area = abs(v[0].x * v[1].y - v[0].y * v[1].x);
  float h = area / length(v[localVertexIndex]);

  // edge visibility code for all three edges of the triangle
  int edgeData = int(auxData & 7u);

  // visibility of the edge opposite the vertex (& with 1, 2, 4)
  int visible[3] = int[](
    int((edgeData & 1) != 0),
    int((edgeData & 2) != 0),
    int((edgeData & 4) != 0)
  );

  // final altitude for wireframe rendering
  v_Altitude = vec3(
    visible[0] == 0 ? FAR : 0,
    visible[1] == 0 ? FAR : 0,
    visible[2] == 0 ? FAR : 0
  );
  v_Altitude[localVertexIndex] = (1 - visible[localVertexIndex]) * FAR + visible[localVertexIndex] * h;

  // barycentric coordinates for interpolation
  v_Barycentric = vec3(0.0);
  v_Barycentric[localVertexIndex] = 1.0;

  // which cell this triangle comes from
  v_CellNumber = int(auxData >> 3);
}
