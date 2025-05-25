UI_Uniform :: struct
{
  window_dim: V2;
  texture_dim: V2;
};

UI_Shape :: struct
{
  p_min: V2;
  p_max: V2;
  tex_min: V2;
  tex_max: V2;
  tex_layer: float;
  corner_radius: float;
  edge_softness: float;
  border_thickness: float;
  color: u32; // @todo array of 4 colors for gradients
};

UI_Clip :: struct
{
  p_min: V2;
  p_max: V2;
};
