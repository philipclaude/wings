#version 410

layout( location = 0 ) out vec4 fragColor;

uniform samplerBuffer field;
uniform samplerBuffer colormap;

// TODO: make these uniforms
const int ncolor = 256;

uniform float u_umin = 0;
uniform float u_umax = 1;
uniform int u_field_mode;

flat in int id;

void
get_color( float u , inout vec3 color ) {

  float umin = u_umin;
  float umax = u_umax;

  int indx = int(ncolor*(u - umin)/(umax - umin));

  if (indx < 0) indx = 0;
  if (indx > 255) indx = 255;

  color = (1 - u_field_mode) * color + u_field_mode * texelFetch(colormap, indx).xyz;
}

void main() {
  vec3 color = vec3(1, 0, 0);
  float u = texelFetch(field, id).r;
  //get_color(u, color);
  fragColor = vec4(color, 1.0);
}
