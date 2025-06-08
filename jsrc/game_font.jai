FONT_ATLAS_LAYERS :: 4;
FONT_ATLAS_LAYER_MAX_LINES :: 32;
FONT_ATLAS_LAYER_MAX_ELEM :: 1024;

FONT_GlyphRun :: struct
{
  hash: u64;
  rect: Range(Vec2(s16));
  layer2_next_plus_one30: u32; // 2 bits for layer; 30 bits for index+1 into the collision table
  debug_line_index: u32; // @todo remove
};

FONT_Layer :: struct
{
  line_heights:  [FONT_ATLAS_LAYER_MAX_LINES] s16;
  line_advances: [FONT_ATLAS_LAYER_MAX_LINES] s16;
  line_count: u32;

  hash_table: [FONT_ATLAS_LAYER_MAX_ELEM] FONT_GlyphRun;
  collision_table: [FONT_ATLAS_LAYER_MAX_ELEM] FONT_GlyphRun;
  collision_count: u32;
};

FONT_Type :: enum
{
  REGULAR;
  HEADER;
  COUNT;
};

FONT_State :: struct
{
  scale: float;
  scale_changed: bool;

  sizes: [FONT_Type.COUNT] float;
  // ttfs: [FONT_Type.COUNT][2] *TTF_Font;
  texture_dim: s16 = 1;
  margin: s16;

  layers: [FONT_ATLAS_LAYERS] FONT_Layer;
  active_layer: u32;
};

FONT_Init :: ()
{
  // @todo
}
