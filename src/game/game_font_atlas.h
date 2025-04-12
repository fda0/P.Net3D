#define FONT_ATLAS_LAYERS 4
#define FONT_ATLAS_LAYER_MAX_LINES 32
#define FONT_ATLAS_LAYER_MAX_ELEM 1024

typedef struct
{
  U64 hash;
  V2I16 p;
  V2I16 dim; // @change to pmax?
  U32 layer : 2;
  U32 next_plus_one : 30; // index+1 into the collision table
  U32 debug_line_index;
} FA_GlyphRun;

typedef struct
{
  I16 line_heights[FONT_ATLAS_LAYER_MAX_LINES];
  I16 line_advances[FONT_ATLAS_LAYER_MAX_LINES];
  U32 line_count;

  FA_GlyphRun hash_table[FONT_ATLAS_LAYER_MAX_ELEM];
  FA_GlyphRun collision_table[FONT_ATLAS_LAYER_MAX_ELEM];
  U32 collision_count;
} FA_Layer;

typedef enum
{
  FA_Regular,
  FA_Header,
  FA_Font_COUNT,
} FA_Font;

typedef struct
{
  float sizes[FA_Font_COUNT];
  TTF_Font *fonts[FA_Font_COUNT][2];
  I16 texture_dim;
  I16 margin;

  FA_Layer layers[FONT_ATLAS_LAYERS];
  U32 active_layer;
} FA_State;
