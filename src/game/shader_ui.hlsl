#include "shader_util.hlsl"

struct UI_VertexInput
{
  uint vertex_index : SV_VertexID;
};

struct UI_DxUniform
{
  V2 window_dim;
};

struct UI_VertexToFragment
{
  V4 color : TEXCOORD0;
  V4 border_color : TEXCOORD1;
  V2 tex_uv : TEXCOORD2;
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

cbuffer VertexUniformBuf : register(b0, space1) { UI_DxUniform UniV; };
StructuredBuffer<UI_DxShape> ShapeBuf : register(t0);
StructuredBuffer<UI_DxClip>  ClipBuf  : register(t1);

UI_VertexToFragment UI_DxShaderVS(UI_VertexInput input)
{
  uint corner_index = input.vertex_index & 3u; // 2 bits; [0:1]
  uint shape_index = (input.vertex_index >> 2u) & 0xFFFFu; // 16 bits; [2:17]
  uint clip_index = (input.vertex_index >> 18u) & 0x3FFFu; // 14 bits; [18:31]

  //
  UI_DxShape shape = ShapeBuf[shape_index];

  V2 pos = shape.p_min;
  if (corner_index & 1) pos.x = shape.p_max.x;
  if (corner_index & 2) pos.y = shape.p_max.y;

  //
  UI_VertexToFragment frag;
  frag.color = UnpackColor32(shape.color);
  frag.clip_space_p = V4(2.f*pos / UniV.window_dim - 1.f, 1, 1);
  frag.clip_space_p = V4(2.f*pos / UniV.window_dim - 1.f, 1, 1);
  return frag;
}

float4 UI_DxShaderPS(UI_VertexToFragment frag) : SV_Target0
{
  return frag.color;
}
