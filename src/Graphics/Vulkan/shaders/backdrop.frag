#version 450

layout(set = 1, binding = 0) uniform sampler2D tex;

layout(location = 0) in vec2 vUv;
layout(location = 1) in vec4 vColor;
layout(location = 2) in vec2 vLocal;
layout(location = 3) in vec2 vSize;
layout(location = 4) in vec4 vRadii;
layout(location = 0) out vec4 outColor;

void main() {
  float mask = roundedAlpha(vLocal, vSize, vRadii);
  if (mask <= 0.0) {
    discard;
  }
  vec4 c = texture(tex, vUv);
  vec4 tint = vec4(vColor.rgb * vColor.a, vColor.a);
  outColor = (tint + c * (1.0 - tint.a)) * mask;
}
