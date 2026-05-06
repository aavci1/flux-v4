#version 450

layout(push_constant) uniform Push {
  vec2 viewport;
} pc;

layout(location = 0) out vec2 vUv;
layout(location = 1) out vec4 vColor;

layout(std430, set = 0, binding = 0) readonly buffer Quads {
  vec4 data[];
} quads;

vec2 unitVertex(uint i) {
  vec2 p[6] = vec2[](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0),
    vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0)
  );
  return p[i];
}

void main() {
  uint base = gl_InstanceIndex * 3;
  vec4 rect = quads.data[base + 0];
  vec4 uv = quads.data[base + 1];
  vec4 color = quads.data[base + 2];
  vec2 unit = unitVertex(gl_VertexIndex);
  vec2 pos = rect.xy + unit * rect.zw;
  vec2 ndc = vec2(pos.x / pc.viewport.x * 2.0 - 1.0,
                  pos.y / pc.viewport.y * 2.0 - 1.0);
  gl_Position = vec4(ndc, 0.0, 1.0);
  vUv = mix(uv.xy, uv.zw, unit);
  vColor = color;
}
