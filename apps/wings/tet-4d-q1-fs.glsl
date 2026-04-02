#version 410

layout(location = 0) out vec4 fragColor;

noperspective in vec3 v_Altitude;
in vec3 v_Position;
flat in int id;
in vec3 v_Normal;

uniform samplerBuffer colormap;

const int ncolor = 256;
uniform float u_umin = 0;
uniform float u_umax = 1;

uniform int u_edges;
uniform float u_alpha;
uniform int u_lighting;
uniform int u_field_mode;

vec3 get_color(float u, float umin, float umax) {
  int indx = int(ncolor * (u - umin) / (umax - umin));
  indx = max(indx, 0);
  indx = min(indx, 255);
  return texelFetch(colormap, indx).xyz;
}

vec3 shade(in vec3 l, in vec3 n, in vec3 color) {
  float ambient = 0.8;
  float diffuse = u_lighting * abs(dot(l, n));
  float specular = u_lighting * pow(max(0.0, dot(-reflect(l, n), l)), 128.0);
  return color * (ambient + diffuse + specular);
}

uint hash(uint x) {
  uint s = x * 747796405u + 2891336453u;
  uint w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
  return (w >> 22u) ^ w;
}

void main() {
  float d = min(min(v_Altitude[0], v_Altitude[1]), v_Altitude[2]);
  float intensity = u_edges * exp2(-0.25 * d * d);

  vec3 position = normalize(v_Position);
  vec3 normal = normalize(v_Normal);

  vec3 color = shade(-position, normal, vec3(0.8));
  fragColor = intensity * vec4(0, 0, 0, 1.0) + (1.0 - intensity) * vec4(color, u_alpha);
}
