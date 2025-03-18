#define FONT_ATLAS_DIM 512
#define FONT_ATLAS_LAYERS 4
#define FONT_ATLAS_LAYER_MAX_LINES 32
#define FONT_ATLAS_LAYER_MAX_ELEM 1024

typedef struct
{
  U64 hash;
  V2 p, dim;
  U32 layer : 2;
  U32 next_plus_one : 30; // index+1 into the collision table
} FA_Entry;

typedef struct
{
  float line_heights[FONT_ATLAS_LAYER_MAX_LINES];
  float line_advances[FONT_ATLAS_LAYER_MAX_LINES];
  FA_Entry hash_table[FONT_ATLAS_LAYER_MAX_ELEM];
  FA_Entry collision_table[FONT_ATLAS_LAYER_MAX_ELEM];
  U32 collision_count;
} FA_Layer;

typedef struct
{
  TTF_Font *font;
  FA_Layer layers[FONT_ATLAS_LAYERS];
  U32 active_layer;
} FA_State;
