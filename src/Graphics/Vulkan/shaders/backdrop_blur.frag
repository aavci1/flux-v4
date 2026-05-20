#version 450

layout(set = 1, binding = 0) uniform sampler2D tex;

layout(location = 0) in vec2 vUv;
layout(location = 1) in vec4 vColor;
layout(location = 2) in vec2 vLocal;
layout(location = 3) in vec2 vSize;
layout(location = 4) in vec4 vRadii;
layout(location = 0) out vec4 outColor;

void main() {
  float radius = max(vRadii.x, 0.0);
  vec2 axis = vRadii.yz;
  if (radius <= 0.01) {
    outColor = texture(tex, vUv);
    return;
  }

  vec2 unitOffset = axis * (1.0 / vec2(textureSize(tex, 0))) * (radius / 8.0);
  vec4 c = texture(tex, vUv) * 0.11283128;
  c += (texture(tex, vUv + unitOffset * 1.4712388) + texture(tex, vUv - unitOffset * 1.4712388)) * 0.20525718;
  c += (texture(tex, vUv + unitOffset * 3.4323776) + texture(tex, vUv - unitOffset * 3.4323776)) * 0.14059407;
  c += (texture(tex, vUv + unitOffset * 5.3954768) + texture(tex, vUv - unitOffset * 5.3954768)) * 0.07114279;
  c += (texture(tex, vUv + unitOffset * 7.3592315) + texture(tex, vUv - unitOffset * 7.3592315)) * 0.02659033;
  outColor = c;
}
