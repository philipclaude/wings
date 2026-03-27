#version 330
layout (location = 0) in vec3 a_Position;
layout (location = 1) in float a_Altitude;

uniform mat4 u_ModelViewMatrix;
uniform mat4 u_ModelViewProjectionMatrix;
uniform usamplerBuffer aux;

out vec3 v_Barycentric;
out vec3 v_Position;
flat out int v_TriangleID;
flat out vec3 v_EdgeVisibility;
flat out int v_CellNumber;
out vec3 v_Altitude;

void main() {
  gl_Position = u_ModelViewProjectionMatrix * vec4(a_Position, 1.0);
  v_Position = vec3(u_ModelViewMatrix * vec4(a_Position, 1.0));
  v_TriangleID = int(gl_VertexID / 3);
  v_Barycentric = vec3(0.0);
  v_Barycentric[gl_VertexID % 3] = 1.0;
  uint aux_data = texelFetch(aux, v_TriangleID).r;
  int edge_data = int(aux_data & uint(7));
  v_EdgeVisibility[0] = float((edge_data & 1) != 0);
  v_EdgeVisibility[1] = float((edge_data & 2) != 0);
  v_EdgeVisibility[2] = float((edge_data & 4) != 0);
  v_CellNumber = int(aux_data >> 3);
  v_Altitude = vec3(0);
  v_Altitude[gl_VertexID % 3] = a_Altitude;
}
