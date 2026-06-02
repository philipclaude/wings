#version 410

flat out int v_id;

void main() {
  gl_Position = vec4(0, 0, 0, 1.0);
  v_id = gl_VertexID;
}