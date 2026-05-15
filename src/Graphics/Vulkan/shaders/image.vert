#version 450

layout(push_constant) uniform Push {
  vec2 viewport;
  vec2 translation;
} pc;

layout(location = 0) out vec2 vUv;
layout(location = 1) out vec4 vColor;
layout(location = 2) out vec2 vLocal;
layout(location = 3) out vec2 vSize;
layout(location = 4) out vec4 vRadii;

struct QuadInstance {
  vec4 rect;
  vec4 axisX;
  vec4 axisY;
  vec4 uv;
  vec4 color;
  vec4 radii;
};

layout(std430, set = 0, binding = 0) readonly buffer Quads {
  QuadInstance instances[];
} quads;

vec2 unitVertex(uint i) {
  vec2 p[6] = vec2[](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0),
    vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0)
  );
  return p[i];
}

void main() {
  QuadInstance q = quads.instances[gl_InstanceIndex];
  vec2 unit = unitVertex(gl_VertexIndex);
  vec2 pos = q.axisX.xy + unit.x * q.axisX.zw + unit.y * q.axisY.xy + pc.translation;
  vec2 ndc = vec2(pos.x / pc.viewport.x * 2.0 - 1.0,
                  pos.y / pc.viewport.y * 2.0 - 1.0);
  gl_Position = vec4(ndc, 0.0, 1.0);
  vUv = mix(q.uv.xy, q.uv.zw, unit);
  vColor = q.color;
  vLocal = unit * q.rect.zw;
  vSize = q.rect.zw;
  vRadii = q.radii;
}
