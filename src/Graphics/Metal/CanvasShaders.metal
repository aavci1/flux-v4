// SDF shaders adapted from aavci1/flux (GLSL → Metal).

#include <metal_stdlib>
using namespace metal;

// -----------------------------------------------------------------------------
// Rounded rect (sdf_quad.vert + rect.frag)
// -----------------------------------------------------------------------------

struct RectInstance {
  float4 rect;
  float4 corners;
  float4 fillColor;
  float4 strokeColor;
  float2 strokeWidthOpacity;
  float2 viewport;
  float4 rotationPad;
};

struct RectVertexOut {
  /// Clip-space output (NDC in xy).
  float4 clip [[position]];
  float2 fragLocalPos;
  float2 fragHalfSize;
  float4 fragCorners;
  float4 fragFillColor;
  float4 fragStrokeColor;
  float fragStrokeWidth;
  float fragOpacity;
  float2 fragCenter;
  float fragAngle;
  float2 fragViewport;
};

/// Same layout as `RectVertexOut`, but `clip` has no `[[position]]` so the fragment can also take `float4 [[position]]` for pixel coords (Metal allows only one `[[position]]` per fragment signature when using `[[stage_in]]`).
struct RectFragmentIn {
  float4 clip;
  float2 fragLocalPos;
  float2 fragHalfSize;
  float4 fragCorners;
  float4 fragFillColor;
  float4 fragStrokeColor;
  float fragStrokeWidth;
  float fragOpacity;
  float2 fragCenter;
  float fragAngle;
  float2 fragViewport;
};

vertex RectVertexOut rect_sdf_vert(uint vid [[vertex_id]], uint iid [[instance_id]],
                                   constant float2* quad [[buffer(0)]],
                                   constant RectInstance* instances [[buffer(1)]]) {
  RectInstance inst = instances[iid];
  float2 halfSize = inst.rect.zw * 0.5f;
  float2 center = inst.rect.xy + halfSize;
  float pad = max(inst.strokeWidthOpacity.x, 1.0f);
  float2 paddedHalf = halfSize + pad;
  float2 localOffset = quad[vid] * paddedHalf;
  float cr = cos(inst.rotationPad.x);
  float sr = sin(inst.rotationPad.x);
  float2 worldOffset =
      float2(localOffset.x * cr - localOffset.y * sr, localOffset.x * sr + localOffset.y * cr);
  float2 screenPos = center + worldOffset;
  float2 ndc = (screenPos / inst.viewport) * 2.0f - 1.0f;
  ndc.y = -ndc.y;

  RectVertexOut out;
  out.clip = float4(ndc, 0.0f, 1.0f);
  out.fragLocalPos = localOffset;
  out.fragHalfSize = halfSize;
  out.fragCorners = inst.corners;
  out.fragFillColor = inst.fillColor;
  out.fragStrokeColor = inst.strokeColor;
  out.fragStrokeWidth = inst.strokeWidthOpacity.x;
  out.fragOpacity = inst.strokeWidthOpacity.y;
  out.fragCenter = center;
  out.fragAngle = inst.rotationPad.x;
  out.fragViewport = inst.viewport;
  return out;
}

static float roundedRectSDF(float2 p, float2 halfSize, float4 corners) {
  float r = (p.x > 0.0f) ? ((p.y > 0.0f) ? corners.z : corners.y) : ((p.y > 0.0f) ? corners.w : corners.x);
  float2 q = abs(p) - halfSize + r;
  return min(max(q.x, q.y), 0.0f) + length(max(q, float2(0.0f))) - r;
}

fragment float4 rect_sdf_frag(RectFragmentIn in [[stage_in]], float4 fragCoord [[position]]) {
  // Built-in fragment position: pixel coordinates in the render target (same space as vertex `screenPos` / inst.rect).
  float2 pixel = fragCoord.xy;
  float2 delta = pixel - in.fragCenter;
  float ca = cos(-in.fragAngle);
  float sa = sin(-in.fragAngle);
  float2 p = float2(delta.x * ca - delta.y * sa, delta.x * sa + delta.y * ca);
  float d = roundedRectSDF(p, in.fragHalfSize, in.fragCorners);
  float fillCoverage = 1.0f - smoothstep(-0.75f, 0.75f, d);
  float strokeCoverage = 0.0f;
  if (in.fragStrokeWidth > 0.0f) {
    float sd = abs(d) - in.fragStrokeWidth * 0.5f;
    strokeCoverage = 1.0f - smoothstep(-0.75f, 0.75f, sd);
  }
  float fillA = in.fragFillColor.a * fillCoverage;
  float strokeA = in.fragStrokeColor.a * strokeCoverage;
  float4 fillP = float4(in.fragFillColor.rgb * fillA, fillA);
  float4 strokeP = float4(in.fragStrokeColor.rgb * strokeA, strokeA);
  float4 blended = strokeP + fillP * (1.0f - strokeP.a);
  float outA = blended.a * in.fragOpacity;
  if (outA < 0.001f)
    discard_fragment();
  // Premultiplied RGBA; blend state uses (One, OneMinusSourceAlpha) like glyph pipeline.
  return float4(blended.rgb * in.fragOpacity, outA);
}

// -----------------------------------------------------------------------------
// Line capsule (sdf_line.vert + line.frag)
// -----------------------------------------------------------------------------

vertex RectVertexOut line_sdf_vert(uint vid [[vertex_id]], uint iid [[instance_id]],
                                   constant float2* quad [[buffer(0)]],
                                   constant RectInstance* instances [[buffer(1)]]) {
  RectInstance inst = instances[iid];
  float2 halfSize = inst.rect.zw * 0.5f;
  float2 center = inst.rect.xy + halfSize;
  float pad = max(inst.strokeWidthOpacity.x, 1.0f);
  float2 paddedHalf = halfSize + pad;
  float2 localOffset = quad[vid] * paddedHalf;
  float cosA = inst.corners.x;
  float sinA = inst.corners.y;
  float2 lineRotated =
      float2(localOffset.x * cosA - localOffset.y * sinA, localOffset.x * sinA + localOffset.y * cosA);
  float cr = cos(inst.rotationPad.x);
  float sr = sin(inst.rotationPad.x);
  float2 rotatedOffset =
      float2(lineRotated.x * cr - lineRotated.y * sr, lineRotated.x * sr + lineRotated.y * cr);
  float2 screenPos = center + rotatedOffset;
  float2 ndc = (screenPos / inst.viewport) * 2.0f - 1.0f;
  ndc.y = -ndc.y;

  RectVertexOut out;
  out.clip = float4(ndc, 0.0f, 1.0f);
  out.fragLocalPos = localOffset;
  out.fragHalfSize = halfSize;
  out.fragCorners = inst.corners;
  out.fragFillColor = inst.fillColor;
  out.fragStrokeColor = inst.strokeColor;
  out.fragStrokeWidth = inst.strokeWidthOpacity.x;
  out.fragOpacity = inst.strokeWidthOpacity.y;
  out.fragCenter = center;
  out.fragAngle = inst.rotationPad.x;
  out.fragViewport = inst.viewport;
  return out;
}

fragment float4 line_sdf_frag(RectFragmentIn in [[stage_in]], float4 fragCoord [[position]]) {
  // Capsule stroke in line space: +u along segment, v perpendicular.
  // Real half-length is `fragCorners.z` (device px). `fragHalfSize.x` includes quad padding and must
  // not be used for the SDF or ink extends past the true endpoints.
  float2 pixel = fragCoord.xy;
  float2 delta = pixel - in.fragCenter;
  float cosA = in.fragCorners.x;
  float sinA = in.fragCorners.y;
  float cr = cos(-in.fragAngle);
  float sr = sin(-in.fragAngle);
  float2 dRot = float2(delta.x * cr - delta.y * sr, delta.x * sr + delta.y * cr);
  float u = dRot.x * cosA + dRot.y * sinA;
  float v = -dRot.x * sinA + dRot.y * cosA;
  float halfLen = in.fragCorners.z;
  float halfW = in.fragStrokeWidth * 0.5f;
  float2 p = float2(u, v);
  float2 pa = p - float2(-halfLen, 0.0f);
  float2 ba = float2(2.0f * halfLen, 0.0f);
  float denom = dot(ba, ba);
  float h = denom > 1e-8f ? clamp(dot(pa, ba) / denom, 0.0f, 1.0f) : 0.0f;
  float d = length(pa - ba * h) - halfW;
  float alpha = (1.0f - smoothstep(-0.75f, 0.75f, d)) * in.fragStrokeColor.a * in.fragOpacity;
  if (alpha < 0.001f)
    discard_fragment();
  return float4(in.fragStrokeColor.rgb * alpha, alpha);
}

// -----------------------------------------------------------------------------
// Path mesh (triangulated fill / expanded stroke — same convention as upstream flux path shaders)
// -----------------------------------------------------------------------------

struct PathVertexIn {
  float2 pos [[attribute(0)]];
  float4 color [[attribute(1)]];
  float2 viewport [[attribute(2)]];
};

struct PathVertexOut {
  float4 clip [[position]];
  float4 color;
};

vertex PathVertexOut path_vert(PathVertexIn in [[stage_in]]) {
  PathVertexOut out;
  float2 ndc = (in.pos / in.viewport) * 2.0f - 1.0f;
  ndc.y = -ndc.y;
  out.clip = float4(ndc, 0.0f, 1.0f);
  out.color = in.color;
  return out;
}

fragment float4 path_frag(PathVertexOut in [[stage_in]]) {
  float4 c = in.color;
  return float4(c.rgb * c.a, c.a);
}

// -----------------------------------------------------------------------------
// Glyph atlas (R8 coverage × premultiplied run color)
// -----------------------------------------------------------------------------

struct GlyphVertexIn {
  float2 pos [[attribute(0)]];
  float2 uv [[attribute(1)]];
  float4 color [[attribute(2)]];
};

struct GlyphVertexOut {
  float4 clip [[position]];
  float2 uv;
  float4 color;
};

vertex GlyphVertexOut glyph_vert(GlyphVertexIn in [[stage_in]], constant float2* viewport [[buffer(1)]]) {
  float2 ndc = (in.pos / viewport[0]) * 2.0f - 1.0f;
  ndc.y = -ndc.y;
  GlyphVertexOut out;
  out.clip = float4(ndc, 0.0f, 1.0f);
  out.uv = in.uv;
  out.color = in.color;
  return out;
}

fragment float4 glyph_frag(GlyphVertexOut in [[stage_in]], texture2d<float> atlas [[texture(0)]],
                           sampler atlasSmpl [[sampler(0)]]) {
  // in.color is premultiplied (rgb already multiplied by alpha * opacity).
  // Scale by R8 coverage and return; the pipeline uses (One, OneMinusSrcAlpha) premul blending.
  float cov = atlas.sample(atlasSmpl, in.uv).r;
  if (cov < 0.001f)
    discard_fragment();
  return float4(in.color.rgb * cov, in.color.a * cov);
}
