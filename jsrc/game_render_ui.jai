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
  color: Color32; // @todo array of 4 colors for gradients
};

UI_Clip :: struct
{
  p_min: V2;
  p_max: V2;
};

UI_ActiveClipIndex :: () -> u32
{
  using G.gpu.ui;
  clip_index := clip_stack[clip_stack_index];
  return clip_index;
}

UI_ActiveClip :: () -> UI_Clip
{
  return G.gpu.ui.clips[UI_ActiveClipIndex()];
}

UI_PushClip :: (clip_: UI_Clip)
{
  using G.gpu.ui;
  if clip_stack_index + 1 >= clip_stack.count return; // @todo increase clip_stack so PopClip does correct thing; fallback to no clip
  if clips_count >= clips.count return;

  clip := clip_;
  current := UI_ActiveClip();
  // intersection of clip & current
  if (current.p_min.x > clip.p_min.x) clip.p_min.x = current.p_min.x;
  if (current.p_min.y > clip.p_min.y) clip.p_min.y = current.p_min.y;
  if (current.p_max.x < clip.p_max.x) clip.p_max.x = current.p_max.x;
  if (current.p_max.y < clip.p_max.y) clip.p_max.y = current.p_max.y;

  clip_index := clips_count;
  clips_count += 1;
  clips[clip_index] = clip;

  clip_stack_index += 1;
  clip_stack[clip_stack_index] = clip_index;
}

UI_PopClip :: ()
{
  using G.gpu.ui;
  if clip_stack_index == 0 return;
  clip_stack_index -= 1;
}

UI_DrawRaw :: (shape: UI_Shape)
{
  using G.gpu.ui;
  if G.gpu.ui.indices_count + 6 > G.gpu.ui.indices.count return; // @todo log errors?
  if G.gpu.ui.shapes_count + 6 > G.gpu.ui.shapes.count return;

  clip_i := UI_ActiveClipIndex();

  shape_i := shapes_count;
  shapes_count += 1;
  shapes[shape_i] = shape;

  index_i := indices_count;
  indices_count += 6;
  encoded := ((shape_i << 2) | (clip_i << 18));
  indices[index_i + 0] = 0 | encoded;
  indices[index_i + 1] = 1 | encoded;
  indices[index_i + 2] = 2 | encoded;
  indices[index_i + 3] = 2 | encoded;
  indices[index_i + 4] = 1 | encoded;
  indices[index_i + 5] = 3 | encoded;
}

UI_DrawRect :: (shape_: UI_Shape)
{
  shape := shape_;
  shape.tex_layer = -1.0;
  UI_DrawRaw(shape);
}

UI_RenderInit :: ()
{
  tex_info := SDL_GPUTextureCreateInfo.{
    type = .SDL_GPU_TEXTURETYPE_2D_ARRAY,
    format = .SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
    width = 512,
    height = 512,
    layer_count_or_depth = 1,
    num_levels = 1,
    usage = SDL_GPU_TEXTUREUSAGE_SAMPLER
  };
  G.gpu.ui.gpu_atlas_texture = SDL_CreateGPUTexture(G.gpu.device, *tex_info);

  UI_RenderPostFrame();
}

UI_RenderPostFrame :: ()
{
  using G.gpu.ui;
  indices_count = 0;
  shapes_count = 0;
  clips[0] = UI_Clip.{.{0, 0}, .{FLOAT32_MAX, FLOAT32_MAX}};
  clips_count = 1;
  clip_stack[0] = 0;
  clip_stack_index = 0;
}
