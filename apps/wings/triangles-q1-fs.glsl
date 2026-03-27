#version 330

layout(location = 0) out vec4 fragColor;

in vec3 v_Position;
in vec3 v_Barycentric;
flat in int v_TriangleID;
flat in vec3 v_EdgeVisibility;
flat in int v_CellNumber;
in vec3 v_Altitude;

uniform sampler2D image;
uniform samplerBuffer colormap;
uniform samplerBuffer field;

uniform float u_PixelScale;
uniform int u_lighting;
uniform int u_field_mode;
uniform int u_edges;
uniform float u_alpha;

uniform int u_group;
uniform int u_min_group;
uniform int u_max_group;
uniform int u_max_cell;

const int ncolor = 256;
uniform float u_umin = 0;
uniform float u_umax = 1;

vec3 get_color(float u, float umin, float umax) {
  int indx = int(ncolor * (u - umin) / (umax - umin));
  indx = max(indx, 0);
  indx = min(indx, 255);
  return texelFetch(colormap, indx).xyz;
}

vec3 shading(in vec3 l, in vec3 n, in vec3 color) {
  float ambient = 0.2;
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

  float alpha = u_alpha;

  vec3 position = normalize(v_Position);
  vec3 tx = dFdx(v_Position);
  vec3 ty = dFdy(v_Position);
  vec3 normal = normalize(cross(tx, ty));

  // vec3 barycentric = clamp(v_Barycentric, 0, 1);
  // vec3 altitude = mix(vec3(1.0), barycentric, v_EdgeVisibility);
  // float d = min(min(altitude.x, altitude.y), altitude.z);
  // float w = clamp(fwidth(d), 0.0001, 0.1);
  // float intensity = u_edges * (1.0 - smoothstep(0, 2 * w, d));

  vec3 altitude = mix(vec3(1e10), v_Altitude, v_EdgeVisibility);
  float d = min(min(altitude[0], altitude[1]), altitude[2]);
  float intensity = 1.0 - smoothstep(0, 2, d * u_PixelScale * gl_FragCoord.w);

  vec3 color_constant = vec3(0.8);
  vec3 color_field = color_constant;
  vec3 color_group = get_color(u_group, u_min_group, u_max_group);
  vec3 color_cell = get_color(int(hash(uint(v_CellNumber)) % uint(u_max_cell + 1)), 0, u_max_cell);

  /*
  #if ORDER == 0

  float u = texelFetch(field, v_TriangleID).r;
  color_field = get_color(u, u_umin, u_umax);

  #elif ORDER == 1

  float u0 =  texelFetch(field, 3 * v_TriangleID    ).r;
  float u1 =  texelFetch(field, 3 * v_TriangleID + 1).r;
  float u2 =  texelFetch(field, 3 * v_TriangleID + 2).r;

  float s = v_Barycentric[0];
  float t = v_Barycentric[1];

  float u = (1.0 - s - t) * u0 + s * u1 + t * u2;
  color_field = get_color(u, u_umin, u_umax);

  #elif ORDER == -1

  float s = v_Barycentric[0];
  float t = v_Barycentric[1];

  vec2 uv0 = texelFetch(texcoord, 3 * v_TriangleID    ).xy;
  vec2 uv1 = texelFetch(texcoord, 3 * v_TriangleID + 1).xy;
  vec2 uv2 = texelFetch(texcoord, 3 * v_TriangleID + 2).xy;

  vec2 uv = (1.0 - s - t) * uv0 + s * uv1 + t * uv2;

  // float f = 200.0;
  // if (sin(f * uv[0]) * sin(f * uv[1]) > 0.0)
  //   color = vec3(0, 0, 0);
  // else
  //   color = vec3(1, 1, 1);

  color_field = texture(image, uv).rgb;
  #endif
  */

  int m_constant = int(u_field_mode == 0);
  int m_group = int(u_field_mode == 1);
  int m_cell = int(u_field_mode == 2);
  int m_field = int(u_field_mode == 3);

  vec3 color = m_constant * color_constant + m_group * color_group + m_cell * color_cell + m_field * color_field;

  color = shading(-position, normal, color);

  fragColor = intensity * vec4(0, 0, 0, 1) + (1.0 - intensity) * vec4(color, u_alpha);
}
