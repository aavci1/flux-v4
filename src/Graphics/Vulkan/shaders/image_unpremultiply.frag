#version 450

layout(set = 1, binding = 0) uniform sampler2D tex;

layout(location = 0) in vec2 vUv;
layout(location = 1) in vec4 vColor;
layout(location = 2) in vec2 vLocal;
layout(location = 3) in vec2 vSize;
layout(location = 4) in vec4 vRadii;
layout(location = 0) out vec4 outColor;

float roundedRectSDF(vec2 p, vec2 halfSize, vec4 radii) {
  float r = (p.x > 0.0)
              ? ((p.y > 0.0) ? radii.z : radii.y)
              : ((p.y > 0.0) ? radii.w : radii.x);
  vec2 q = abs(p) - halfSize + vec2(r);
  return min(max(q.x, q.y), 0.0) + length(max(q, vec2(0.0))) - r;
}

float roundedAlpha(vec2 p, vec2 size, vec4 radii) {
  if (max(max(radii.x, radii.y), max(radii.z, radii.w)) <= 0.0) {
    return 1.0;
  }
  vec2 halfSize = size * 0.5;
  float d = roundedRectSDF(p - halfSize, halfSize, radii);
  float aa = max(0.75 * length(vec2(dFdx(d), dFdy(d))), 0.0001);
  return 1.0 - smoothstep(-aa, aa, d);
}

void main() {
  float mask = roundedAlpha(vLocal, vSize, vRadii);
  if (mask <= 0.0) {
    discard;
  }
  vec4 sampleColor = texture(tex, vUv);
  float alpha = sampleColor.a * vColor.a * mask;
  vec3 straightRgb = sampleColor.a > 0.0 ? clamp(sampleColor.rgb / sampleColor.a, 0.0, 1.0) : vec3(0.0);
  outColor = vec4(straightRgb * vColor.rgb, alpha);
}
