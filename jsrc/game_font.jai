FONT_ATLAS_LAYERS :: 4;
FONT_ATLAS_LAYER_MAX_LINES :: 64;
FONT_ATLAS_LAYER_MAX_GLYPHS :: 4096;
FONT_ATLAS_MARGIN :: 1;

#scope_file
#import "freetype-2.12.1";
#scope_export

FONT_Glyph :: struct
{
  hash: u64;
  p: Vec2(u16);
  dim: Vec2(u16);
  layer: u8;
  next: u16;
};

FONT_Layer :: struct
{
  line_heights:  [FONT_ATLAS_LAYER_MAX_LINES] u16;
  line_advances: [FONT_ATLAS_LAYER_MAX_LINES] u16;
  line_count: u16;

  hash_table:      [FONT_ATLAS_LAYER_MAX_GLYPHS] FONT_Glyph;
  collision_array: [FONT_ATLAS_LAYER_MAX_GLYPHS] FONT_Glyph;
  collision_count: u16;
};

FONT_Style :: enum
{
  REGULAR;
  HEADER;
};

FONT_Family :: enum u8
{
  Jacquard24;
  PlayfairDisplay;
  NotoColorEmoji;
};

FONT_State :: struct
{
  err: s32;

  texture_dim: u16 = 1024;

  layers: [FONT_ATLAS_LAYERS] FONT_Layer;
  active_layer_index: u8;

  ft_library: FT_Library;
  ft_faces: [#run EnumCount(FONT_Family)] FT_Face;
};

FONT_StyleMetrics :: struct
{
  pixel_height: u32;
  font: FONT_Family;
};

FONT_StyleToMetrics :: (style: FONT_Style) -> FONT_StyleMetrics
{
  return .{16, .PlayfairDisplay}; // @impl
}

FONT_Init :: ()
{
  using G.font;
  if err then return;

  err = FT_Init_FreeType(*ft_library);
  for enum_values_as_enum(FONT_Family)
  {
    path := "";
    if it == { // @todo this should be put into .PIE file
      case .Jacquard24; path = "../res/fonts/Jacquard24-Regular.ttf";
      case .PlayfairDisplay; path = "../res/fonts/PlayfairDisplay-Regular.ttf";
      case .NotoColorEmoji; path = "../res/fonts/NotoColorEmoji-Regular.ttf";
    }
    if !err then err = FT_New_Face(ft_library, temp_c_string(path), 0, *ft_faces[it]);
    // if !err then FT_Set_Char_Size(ft_faces[it], 0, 16*64, 96, 96);
    if !err then FT_Set_Pixel_Sizes(ft_faces[it], 0, 32);
  }

  glyph_index := FT_Get_Char_Index(ft_faces[1], #char "R");
  error := FT_Load_Glyph(ft_faces[1], glyph_index, 0);
  print("ft glyph (1): %\n", formatStruct(ft_faces[1].glyph.*, use_newlines_if_long_form=true));
  error = FT_Render_Glyph(ft_faces[1].glyph, .FT_RENDER_MODE_NORMAL);
  print("ft glyph (2): %\n", formatStruct(ft_faces[1].glyph.*, use_newlines_if_long_form=true));

  glyph := ft_faces[1].glyph;
  bitmap := glyph.bitmap;
  a := 1;
  a += 1;
  FONT_ProcessWindowResize(true);
}

FONT_CodepointHash :: (font: FONT_Family, pixel_height: u32, codepoint: u32) -> u64
{
  hash := Hash64Any(font, pixel_height, codepoint);
  return hash;
}

FONT_FindGlyphInLayer :: (layer: *FONT_Layer, hash: u64, create_mode: bool) -> *FONT_Glyph
{
  key := hash % layer.hash_table.count;
  slot := *layer.hash_table[key];

  while true
  {
    if !slot.hash // it's empty
      break;

    if slot.hash == hash // found glyph run
      break;

    if slot.next // follow next index
    {
      slot = *layer.collision_array[slot.next];
      continue;
    }

    // hash isn't matching, no next slot in chain
    if !create_mode // exit if not in create mode
      break;

    // chain new element from collision table
    if layer.collision_count < layer.collision_array.count
    {
      slot.next = layer.collision_count;
      layer.collision_count += 1;
      slot = *layer.collision_array[slot.next];
      break;
    }

    // all hope is lost, collision table is full
    break;
  }

  return slot;
}

FONT_CreateGlyphInLayer :: (layer: *FONT_Layer, hash: u64, orig_width: u16, orig_height: u16) -> *FONT_Glyph
{
  using G.font;

  margin: u16 = FONT_ATLAS_MARGIN;
  margin2 := margin * 2;

  width := orig_width + margin2;
  height := orig_height + margin2;
  max_dim := G.font.texture_dim;
  all_lines_height: u16;

  // find best line
  best_line_index := U32_MAX;
  for line_index: MakeRange(layer.line_count)
  {
    line_height := layer.line_heights[line_index];
    line_advance := layer.line_advances[line_index];
    horizontal_left_in_line := max_dim - line_advance;

    all_lines_height += line_height;

    can_use_line := true;
    if line_height < height            then can_use_line = false;
    if horizontal_left_in_line < width then can_use_line = false;

    if can_use_line
    {
      if best_line_index >= FONT_ATLAS_LAYER_MAX_LINES // first acceptable line found
        best_line_index = line_index;
      else if layer.line_heights[best_line_index] < line_height // check if this line is better than current best line
        best_line_index = line_index;
    }
  }

  height_left := max_dim - all_lines_height;
  if best_line_index < FONT_ATLAS_LAYER_MAX_LINES && height_left >= height
  {
    // We have a line that can be used. But it's still possible to create a new line.
    // Let's check if we should create a new line - it's a heuristic that tries to minimize
    // avoid wasting space in lines that are too big for this glyph run.
    best_line_height := layer.line_heights[best_line_index];
    waste_px := best_line_height - height;
    waste_share := waste_px.(float) / best_line_height.(float);

    // If layer is almost full the threshold for starting a new line is very big.
    used_height_share := all_lines_height / max_dim;
    share_threshold := 0.2 + 0.8*used_height_share;

    if waste_px > 8 && waste_share > share_threshold
      best_line_index = FONT_ATLAS_LAYER_MAX_LINES;
  }

  // if no best line - try to create a create new line
  if best_line_index >= FONT_ATLAS_LAYER_MAX_LINES && height_left >= height
  {
    if layer.line_count < FONT_ATLAS_LAYER_MAX_LINES
    {
      best_line_index = layer.line_count;
      layer.line_count += 1;

      layer.line_heights[best_line_index] = height;
    }
  }

  // We have best line line with free space - try to allocate a slot in hash table
  if best_line_index < FONT_ATLAS_LAYER_MAX_LINES
  {
    slot := FONT_FindGlyphInLayer(layer, hash, true);
    if !slot.hash
    {
      line_y_offset: u16;
      for line_index: MakeRange(best_line_index)
        line_y_offset += layer.line_heights[line_index];

      slot.hash = hash;
      slot.p = .{
        x = layer.line_advances[best_line_index] + margin,
        y = line_y_offset + margin,
      };
      slot.dim = .{
        x = orig_width,
        y = orig_height,
      };
      slot.layer = FONT_LayerIndexFromPointer(layer);

      layer.line_advances[best_line_index] += width;
      return slot;
    }
  }

  // Failed to allocate glyph run in this layer
  return null;
}

FONT_GetGlyph :: (style: FONT_Style, codepoint: u32) -> FONT_Glyph
{
  metrics := FONT_StyleToMetrics(style);
  return FONT_GetGlyph(metrics.font, metrics.pixel_height, codepoint);
}

FONT_LayerIndexFromPointer :: (layer: *FONT_Layer) -> u8
{
  using G.font;
  index := (layer.(u64) - (*layers[0]).(u64)) / size_of(FONT_Layer);
  return index.(u8);
}

FONT_GetGlyph :: (font: FONT_Family, pixel_height: u32, codepoint: u32) -> FONT_Glyph
{
  using G.font;
  assert(!G.headless);

  margin: u16 = FONT_ATLAS_MARGIN;
  margin2 := margin * 2;

  hash := FONT_CodepointHash(font, pixel_height, codepoint);

  // Find already existing glyph run
  for layer_offset: 0 .. layers.count-2
  {
    layer_index := (active_layer_index + layer_offset) % layers.count;
    glyph_run := FONT_FindGlyphInLayer(*layers[layer_index], hash, false);
    if (glyph_run.hash == hash)
      return glyph_run.*;
  }

  // Render glyph
  ft_face := ft_faces[font];
  FT_Set_Pixel_Sizes(ft_face, 0, pixel_height);
  glyph_index := FT_Get_Char_Index(ft_faces[1], codepoint);
  if !glyph_index
  {
     // @todo Instead look into fallback font faces (emoji).
     //       If codepoint is not found anywhere use fallback glyph character (dot?).
    return .{};
  }

  FT_Load_Glyph(ft_face, glyph_index, 0);
  FT_Render_Glyph(ft_face.glyph, .FT_RENDER_MODE_NORMAL);

  glyph := ft_face.glyph;
  bitmap := glyph.bitmap;

  // Reject surfaces that are too big
  if bitmap.width + margin2 > texture_dim || bitmap.rows + margin2 > texture_dim
    return .{};

  slot: *FONT_Glyph;
  for layer_offset: 0 .. layers.count-2
  {
    layer_index := (active_layer_index + layer_offset) % layers.count;
    slot = FONT_CreateGlyphInLayer(*layers[layer_index], hash, xx bitmap.width, xx bitmap.rows);
    if slot then break;
  }

  if !slot // couldn't find space in any layer
  {
    // advance active_layer index (backwards) and clear active layer
    active_layer_index += layers.count - 1;
    active_layer_index %= layers.count;

    active_layer := *layers[active_layer_index];
    ClearType(active_layer);

    slot = FONT_CreateGlyphInLayer(active_layer, hash, xx bitmap.width, xx bitmap.rows);
  }

  // Upload surface to GPU (if free slot was found)
  if slot
  {
    comp_size: s32 = 4;
    bitmap_dim: Vec2(s32) = .{xx bitmap.width, xx bitmap.rows};
    bitmap_with_margin_size := (bitmap_dim.x + margin2) * (bitmap_dim.y + margin2) * comp_size;

  //   {
  //     SDL_GPUTransferBufferCreateInfo trans_cpu =
  //     {
  //       .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
  //       .size = surf_with_margin_size
  //     };
  //     SDL_GPUTransferBuffer *buf_transfer = SDL_CreateGPUTransferBuffer(G.gpu.device, &trans_cpu);

  //     // CPU memory . GPU memory
  //     {
  //       void *mapped_memory = SDL_MapGPUTransferBuffer(G.gpu.device, buf_transfer, false);
  //       memset(mapped_memory, 0, surf_with_margin_size);

  //       ForI32(y, surf.h)
  //       {
  //         U8 *dst = (U8 *)mapped_memory + (y * (surf.w + margin2) * comp_size);
  //         U8 *src = (U8 *)surf.pixels  + (y * surf.pitch);
  //         memcpy(dst, src, surf.w*comp_size);
  //       }

  //       SDL_UnmapGPUTransferBuffer(G.gpu.device, buf_transfer);
  //     }

  //     // GPU memory . GPU texture
  //     {
  //       SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(G.gpu.device);
  //       SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);

  //       SDL_GPUTextureTransferInfo trans_gpu =
  //       {
  //         .transfer_buffer = buf_transfer,
  //         .offset = 0,
  //       };

  //       Assert(slot.p.x >= margin);
  //       Assert(slot.p.y >= margin);
  //       Assert(slot.p.x + slot.dim.x + margin <= G.font.texture_dim);
  //       Assert(slot.p.y + slot.dim.y + margin <= G.font.texture_dim);
  //       SDL_GPUTextureRegion dst_region =
  //       {
  //         .texture = G.gpu.ui.gpu.atlas_texture,
  //         .layer = slot.layer,
  //         .x = slot.p.x - margin,
  //         .y = slot.p.y - margin,
  //         .w = slot.dim.x + margin2,
  //         .h = slot.dim.y + margin2,
  //         .d = 1,
  //       };
  //       SDL_UploadToGPUTexture(copy_pass, &trans_gpu, &dst_region, false);

  //       SDL_EndGPUCopyPass(copy_pass);
  //       SDL_SubmitGPUCommandBuffer(cmd);
  //     }

  //     SDL_ReleaseGPUTransferBuffer(G.gpu.device, buf_transfer);
  //   }

  //   result = *slot;
  }

  return .{};
}

FONT_ProcessWindowResize :: (init: bool)
{
  // if (G.window_resized || init)
  // {
  //   float scale = Max(1.5f, FRound(G.window_height / 128.f)*0.5f - 2.f);
  //   if (scale != G.font.scale || init)
  //     G.font.scale_changed = true;

  //   G.font.scale = scale;
  // }

  // if (G.font.scale_changed)
  // {
  //   G.font.sizes[FONT_Regular] = FRound(G.font.scale * 14.f);
  //   G.font.sizes[FONT_Header] = FRound(G.font.scale * 14.f * 1.5f);

  //   ForU32(i, FONT_COUNT)
  //   {
  //     float dpi = 72.f * 0.5f;
  //     TTF_SetFontSizeDPI(G.font.ttfs[i][0], G.font.sizes[i], dpi, dpi);
  //     TTF_SetFontSizeDPI(G.font.ttfs[i][1], G.font.sizes[i], dpi, dpi);
  //   }

  //   I32 new_texture_dim = U32_CeilPow2(G.window_height); // @todo consider window area instead of height
  //   new_texture_dim = Max(256, new_texture_dim);

  //   bool resize_texture = false;
  //   resize_texture |= new_texture_dim > G.font.texture_dim; // texture grew
  //   resize_texture |= new_texture_dim*2 < G.font.texture_dim; // texure shrank by at least 4x

  //   if (resize_texture || init)
  //   {
  //     LOG(LOG_GPU, "Font Atlas resize from %d to %d", (I32)G.font.texture_dim, new_texture_dim);

  //     G.font.texture_dim = new_texture_dim;

  //     // Initialize atlas layers bookkeeping structs
  //     {
  //       SDL_zeroa(G.font.layers); // clear layers

  //       // Pre-seed layers to be full.
  //       // This isn't necessary but will result in more optimal fill order.
  //       ForArray(i, G.font.layers)
  //       {
  //         FONT_Layer *layer = G.font.layers + i;
  //         layer.line_count = 1;
  //         layer.line_heights[0] = G.font.texture_dim;
  //         layer.line_advances[0] = G.font.texture_dim;
  //       }
  //     }

  //     SDL_ReleaseGPUTexture(G.gpu.device, G.gpu.ui.gpu.atlas_texture);

  //     SDL_GPUTextureCreateInfo tex_info =
  //     {
  //       .type = SDL_GPU_TEXTURETYPE_2D_ARRAY,
  //       .format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
  //       .width = G.font.texture_dim,
  //       .height = G.font.texture_dim,
  //       .layer_count_or_depth = FONT_ATLAS_LAYERS,
  //       .num_levels = CalculateMipMapCount(G.font.texture_dim, G.font.texture_dim),
  //       .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COLOR_TARGET
  //     };
  //     G.gpu.ui.gpu.atlas_texture = SDL_CreateGPUTexture(G.gpu.device, &tex_info);
  //     SDL_SetGPUTextureName(G.gpu.device, G.gpu.ui.gpu.atlas_texture, "UI Atlas Texture");
  //   }
  // }
}
