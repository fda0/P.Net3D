#include "shader_util.hlsl"

struct UI_VertexInput
{
  U32 vertex_index : SV_VertexID;
};

struct UI_DxUniform
{
  V2 window_dim;
  V2 texture_dim;
};

struct UI_DxShape
{
  V2 p_min;
  V2 p_max;
  V2 tex_min;
  V2 tex_max;
  float tex_layer;
  float corner_radius;
  float edge_softness;
  float border_thickness;
  U32 color; // @todo array4
};

struct UI_DxClip
{
  V2 p_min;
  V2 p_max;
};

struct UI_Fragment
{
  V4 color               : TEXCOORD0;
  V3 tex_uv              : TEXCOORD1;
  V2 pos                 : TEXCOORD2;
  V2 center              : TEXCOORD3;
  V2 half_dim            : TEXCOORD4;
  float corner_radius    : TEXCOORD5;
  float edge_softness    : TEXCOORD6;
  float border_thickness : TEXCOORD7;
  V4 vertex_p            : SV_Position;
};

static float RoundedRectSDF(V2 pos, V2 center, V2 half_dim, float r)
{
  V2 d2 = (abs(center - pos) - half_dim + V2(r,r));
  float l = length(max(d2, V2(0.f,0.f)));
  return min(max(d2.x, d2.y), 0.f) + l - r;
}

// Dx resources
cbuffer VertexUniformBuf : register(b0, space1) { UI_DxUniform UniV; };

StructuredBuffer<UI_DxShape> ShapeBuf : register(t0);
StructuredBuffer<UI_DxClip>  ClipBuf  : register(t1);

Texture2DArray<V4> AtlasTexture : register(t0, space2);
SamplerState AtlasSampler : register(s0, space2);

// Shaders
UI_Fragment UI_DxShaderVS(UI_VertexInput input)
{
  U32 corner_index = input.vertex_index & 3u; // 2 bits; [0:1]
  U32 shape_index = (input.vertex_index >> 2u) & 0xFFFFu; // 16 bits; [2:17]
  U32 clip_index = (input.vertex_index >> 18u) & 0x3FFFu; // 14 bits; [18:31]
  UI_DxShape shape = ShapeBuf[shape_index];

  // position
  V2 pos = shape.p_min;
  if (corner_index & 1) pos.x = shape.p_max.x;
  if (corner_index & 2) pos.y = shape.p_max.y;

  // texture uv
  V2 tex_uv = V2(shape.tex_min.x, shape.tex_max.y); // @todo not sure why Y is flipped
  if (corner_index & 1) tex_uv.x = shape.tex_max.x;
  if (corner_index & 2) tex_uv.y = shape.tex_min.y;
  tex_uv /= UniV.texture_dim;

  //
  UI_Fragment frag;
  frag.color = UnpackColor32(shape.color);
  frag.tex_uv = V3(tex_uv, shape.tex_layer);
  frag.pos = pos;
  frag.center   = (shape.p_min + shape.p_max) * 0.5f;
  frag.half_dim = (shape.p_max - shape.p_min) * 0.5f;
  frag.corner_radius    = shape.corner_radius;
  frag.edge_softness    = shape.edge_softness;
  frag.border_thickness = shape.border_thickness;
  frag.vertex_p = V4(2.f*pos / UniV.window_dim - 1.f, 1, 1);
  return frag;
}

V4 UI_DxShaderPS(UI_Fragment frag) : SV_Target0
{
  V4 color = frag.color;
  if (frag.tex_uv.z >= 0.f)
  {
    V4 tex_color = AtlasTexture.Sample(AtlasSampler, frag.tex_uv);
    color *= tex_color;
  }

  float softness_padding = frag.edge_softness * 2 - 1;
  V2 soft_pad = V2(softness_padding, softness_padding);

  // outter-rect
  {
    float dist = RoundedRectSDF(frag.pos, frag.center,
                                frag.half_dim - soft_pad,
                                frag.corner_radius);
    float rect_factor = 1.f - smoothstep(0, 2*frag.edge_softness, dist);
    color.a *= rect_factor;
  }

  // border rendering: removing inner-rect
  if (frag.border_thickness > 0.f)
  {
    V2 inner_half_dim = frag.half_dim - V2(frag.border_thickness, frag.border_thickness);
    float inner_r_coef = min(inner_half_dim.x / frag.half_dim.x,
                             inner_half_dim.y / frag.half_dim.y);
    float inner_r = frag.corner_radius * inner_r_coef * inner_r_coef;
    float inner_dist = RoundedRectSDF(frag.pos, frag.center,
                                      inner_half_dim - soft_pad,
                                      inner_r);
    float inner_factor = smoothstep(0, 2*frag.edge_softness, inner_dist);
    color.a *= inner_factor;
  }

  return color;
}
