// UI - User Interface
typedef struct
{
  V2 window_dim;
  V2 texture_dim;
} UI_Uniform;

typedef struct
{
  V2 p_min;
  V2 p_max;
  V2 tex_min;
  V2 tex_max;
  float tex_layer;
  float corner_radius;
  float edge_softness;
  float border_thickness;
  U32 color; // @todo array of 4 colors for gradients
} UI_Shape;

typedef struct
{
  V2 p_min;
  V2 p_max;
} UI_Clip;
