#version 450

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inViewport;

layout(location = 0) out vec4 vColor;

void main() {
  vec2 clip = vec2((inPos.x / inViewport.x) * 2.0 - 1.0, (inPos.y / inViewport.y) * 2.0 - 1.0);
  gl_Position = vec4(clip, 0.0, 1.0);
  vColor = inColor;
}
