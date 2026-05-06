#version 450

layout(location = 0) in vec2 vLocal;
layout(location = 1) in flat uint vInstance;
layout(location = 0) out vec4 outColor;

struct RectInstance {
  vec4 rect;
  vec4 axisX;
  vec4 axisY;
  vec4 radii;
  vec4 fill0;
  vec4 fill1;
  vec4 fill2;
  vec4 fill3;
  vec4 stops;
  vec4 gradient;
  vec4 stroke;
  vec4 params;
};

layout(std430, set = 0, binding = 0) readonly buffer Rects {
  RectInstance instances[];
} rects;

float roundedDistance(vec2 p, vec2 size, vec4 radii) {
  float r = 0.0;
  vec2 c = p;
  if (p.x < radii.x && p.y < radii.x) {
    r = radii.x;
    c = p - vec2(r, r);
  } else if (p.x > size.x - radii.y && p.y < radii.y) {
    r = radii.y;
    c = p - vec2(size.x - r, r);
  } else if (p.x > size.x - radii.z && p.y > size.y - radii.z) {
    r = radii.z;
    c = p - vec2(size.x - r, size.y - r);
  } else if (p.x < radii.w && p.y > size.y - radii.w) {
    r = radii.w;
    c = p - vec2(r, size.y - r);
  } else {
    return -1.0;
  }
  return length(c) - r;
}

vec4 sampleStops(RectInstance r, float t) {
  t = clamp(t, 0.0, 1.0);
  vec4 colors[4] = vec4[](r.fill0, r.fill1, r.fill2, r.fill3);
  float stops[4] = float[](r.stops.x, r.stops.y, r.stops.z, r.stops.w);
  int count = int(r.params.y + 0.5);
  if (count <= 1 || t <= stops[0]) {
    return colors[0];
  }
  for (int i = 1; i < count; ++i) {
    if (t <= stops[i]) {
      float span = max(0.000001, stops[i] - stops[i - 1]);
      return mix(colors[i - 1], colors[i], (t - stops[i - 1]) / span);
    }
  }
  return colors[count - 1];
}

vec4 fillColor(RectInstance r, vec2 p) {
  int type = int(r.params.x + 0.5);
  vec2 uv = p / max(r.rect.zw, vec2(0.000001));
  if (type == 1) {
    vec2 a = r.gradient.xy;
    vec2 b = r.gradient.zw;
    vec2 d = b - a;
    float t = dot(uv - a, d) / max(dot(d, d), 0.000001);
    return sampleStops(r, t);
  }
  if (type == 2) {
    vec2 d = uv - r.gradient.xy;
    float t = length(d) / max(r.gradient.z, 0.000001);
    return sampleStops(r, t);
  }
  if (type == 3) {
    float angle = atan(uv.y - r.gradient.y, uv.x - r.gradient.x) - r.gradient.z;
    float t = fract(angle / 6.28318530718);
    return sampleStops(r, t);
  }
  return r.fill0;
}

void main() {
  RectInstance r = rects.instances[vInstance];
  vec2 size = r.rect.zw;
  if (vLocal.x < 0.0 || vLocal.y < 0.0 || vLocal.x > size.x || vLocal.y > size.y) {
    discard;
  }
  float d = roundedDistance(vLocal, size, r.radii);
  float fillAlpha = 1.0 - smoothstep(-0.5, 0.5, d);
  if (fillAlpha <= 0.0) {
    discard;
  }
  vec4 color = fillColor(r, vLocal);
  float strokeWidth = r.params.z;
  if (strokeWidth > 0.0) {
    vec2 innerP = vLocal - vec2(strokeWidth);
    vec2 innerSize = size - vec2(strokeWidth * 2.0);
    vec4 innerRadii = max(vec4(0.0), r.radii - vec4(strokeWidth));
    bool insideInnerBox = innerP.x >= 0.0 && innerP.y >= 0.0 &&
                          innerP.x <= innerSize.x && innerP.y <= innerSize.y;
    float innerD = insideInnerBox ? roundedDistance(innerP, innerSize, innerRadii) : 1.0;
    float innerAlpha = insideInnerBox ? 1.0 - smoothstep(-0.5, 0.5, innerD) : 0.0;
    float strokeAlpha = fillAlpha * (1.0 - innerAlpha);
    color = mix(color, r.stroke, strokeAlpha);
  }
  color.a *= fillAlpha * r.params.w;
  outColor = color;
}
