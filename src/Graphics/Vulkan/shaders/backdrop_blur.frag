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
  c += (texture(tex, vUv + unitOffset * 1.0) + texture(tex, vUv - unitOffset * 1.0)) * 0.10856112;
  c += (texture(tex, vUv + unitOffset * 2.0) + texture(tex, vUv - unitOffset * 2.0)) * 0.09669606;
  c += (texture(tex, vUv + unitOffset * 3.0) + texture(tex, vUv - unitOffset * 3.0)) * 0.07973203;
  c += (texture(tex, vUv + unitOffset * 4.0) + texture(tex, vUv - unitOffset * 4.0)) * 0.06086204;
  c += (texture(tex, vUv + unitOffset * 5.0) + texture(tex, vUv - unitOffset * 5.0)) * 0.04300806;
  c += (texture(tex, vUv + unitOffset * 6.0) + texture(tex, vUv - unitOffset * 6.0)) * 0.02813473;
  c += (texture(tex, vUv + unitOffset * 7.0) + texture(tex, vUv - unitOffset * 7.0)) * 0.01703826;
  c += (texture(tex, vUv + unitOffset * 8.0) + texture(tex, vUv - unitOffset * 8.0)) * 0.00955207;
  outColor = c;
}
