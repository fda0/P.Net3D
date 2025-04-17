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
} FONT_GlyphRun;

typedef struct
{
  I16 line_heights[FONT_ATLAS_LAYER_MAX_LINES];
  I16 line_advances[FONT_ATLAS_LAYER_MAX_LINES];
  U32 line_count;

  FONT_GlyphRun hash_table[FONT_ATLAS_LAYER_MAX_ELEM];
  FONT_GlyphRun collision_table[FONT_ATLAS_LAYER_MAX_ELEM];
  U32 collision_count;
} FONT_Layer;

typedef enum
{
  FONT_Regular,
  FONT_Header,
  FONT_COUNT,
} FONT_Type;

typedef struct
{
  float scale;
  float sizes[FONT_COUNT];
  TTF_Font *fonts[FONT_COUNT][2];
  I16 texture_dim;
  I16 margin;

  FONT_Layer layers[FONT_ATLAS_LAYERS];
  U32 active_layer;
} FONT_State;
