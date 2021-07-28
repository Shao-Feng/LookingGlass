#version 300 es

layout(location = 0) in vec3 vertexPosition_modelspace;
layout(location = 1) in vec2 vertexUV;

uniform vec4 position;
uniform float rotate;

out highp vec2 uv;

void main()
{
  vec4 p = vec4(0.0, 0.0, 0.0, 1.0);
  p.xyz = vertexPosition_modelspace;
  mat4 rotateMatrix = mat4(cos(rotate), sin(rotate), 0.0, 0.0,
                         -sin(rotate), cos(rotate), 0.0, 0.0,
                         0.0, 0.0, 1.0, 0.0,
                         0.0, 0.0, 0.0, 1.0);
  gl_Position = rotateMatrix * p;
  gl_Position.x  -= position.x;
  gl_Position.y  -= position.y;
  gl_Position.x  *= position.z;
  gl_Position.y  *= position.w;

  uv = vertexUV;
}
