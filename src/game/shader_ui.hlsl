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

struct UI_VertexToFragment
{
  V4 color : TEXCOORD0;
  V4 border_color : TEXCOORD1;
  V3 tex_uv : TEXCOORD2;
  V2 screen_p : TEXCOORD3;
  V2 box_center : TEXCOORD4;
  V2 box_dim : TEXCOORD5;
  float box_border : TEXCOORD6;
  V2 box_radius[4] : TEXCOORD7;
  V4 clip_space_p : SV_Position;
};

struct UI_DxShape
{
  V2 p_min;
  V2 p_max;
  V2 tex_min;
  V2 tex_max;
  float tex_layer;
  U32 color;
  U32 border_color;
  float border_radius[4];
};

struct UI_DxClip
{
  V2 p_min;
  V2 p_max;
};

// Dx resources
cbuffer VertexUniformBuf : register(b0, space1) { UI_DxUniform UniV; };

StructuredBuffer<UI_DxShape> ShapeBuf : register(t0);
StructuredBuffer<UI_DxClip>  ClipBuf  : register(t1);

Texture2DArray<V4> AtlasTexture : register(t0, space2);
SamplerState AtlasSampler : register(s0, space2);

// Shaders
UI_VertexToFragment UI_DxShaderVS(UI_VertexInput input)
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
  UI_VertexToFragment frag;
  frag.color = UnpackColor32(shape.color);
  frag.tex_uv = V3(tex_uv, shape.tex_layer);
  frag.clip_space_p = V4(2.f*pos / UniV.window_dim - 1.f, 1, 1);
  return frag;
}

V4 UI_DxShaderPS(UI_VertexToFragment frag) : SV_Target0
{
  V4 tex_color = AtlasTexture.Sample(AtlasSampler, frag.tex_uv);
  V4 color = frag.color;
  color *= tex_color;
  return color;
}
