#version 450

layout(set = 1, binding = 0) uniform sampler2D tex;

layout(location = 0) in vec2 vUv;
layout(location = 1) in vec4 vColor;
layout(location = 2) in vec2 vLocal;
layout(location = 3) in vec2 vSize;
layout(location = 4) in vec4 vRadii;
layout(location = 0) out vec4 outColor;

float roundedAlpha(vec2 p, vec2 size, vec4 radii) {
  float r = 0.0;
  vec2 cornerCenter = vec2(0.0);
  if (p.x < radii.x && p.y < radii.x) {
    r = radii.x;
    cornerCenter = vec2(r, r);
  } else if (p.x > size.x - radii.y && p.y < radii.y) {
    r = radii.y;
    cornerCenter = vec2(size.x - r, r);
  } else if (p.x > size.x - radii.z && p.y > size.y - radii.z) {
    r = radii.z;
    cornerCenter = vec2(size.x - r, size.y - r);
  } else if (p.x < radii.w && p.y > size.y - radii.w) {
    r = radii.w;
    cornerCenter = vec2(r, size.y - r);
  }
  if (r <= 0.0) {
    return 1.0;
  }
  float d = length(p - cornerCenter) - r;
  return clamp(0.5 - d, 0.0, 1.0);
}

void main() {
  float mask = roundedAlpha(vLocal, vSize, vRadii);
  if (mask <= 0.0) {
    discard;
  }
  outColor = texture(tex, vUv) * vColor * vec4(1.0, 1.0, 1.0, mask);
}
