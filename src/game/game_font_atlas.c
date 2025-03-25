static U64 FA_TextHash(FA_Font font, S8 text)
{
  AssertBounds(font, APP.atlas.fonts);
  float point_size = TTF_GetFontSize(APP.atlas.fonts[font][0]);
  U64 hash = U64_Hash(font, &point_size, sizeof(point_size));
  hash = S8_Hash(hash, text);
  return hash;
}

static FA_GlyphRun *FA_FindGlyphRunInLayer(U32 layer_index, U64 hash, bool create_mode)
{
  AssertBounds(layer_index, APP.atlas.layers);
  FA_Layer *layer = APP.atlas.layers + layer_index;

  U32 key = hash % ArrayCount(layer->hash_table);
  FA_GlyphRun *slot = layer->hash_table + key;

  for (;;)
  {
    if (!slot->hash) // it's empty
      break;

    if (slot->hash == hash) // found glyph run
      break;

    if (slot->next_plus_one) // follow next index
    {
      U32 next = slot->next_plus_one - 1;
      AssertBounds(next, layer->collision_table);
      slot = layer->collision_table + next;
      continue;
    }

    // hash isn't matching, no next slot in chain
    if (!create_mode) // exit if not in create mode
      break;

    // chain new element from collision table
    if (layer->collision_count < ArrayCount(layer->collision_table))
    {
      slot->next_plus_one = layer->collision_count + 1;
      slot = layer->collision_table + layer->collision_count;
      layer->collision_count += 1;
      break;
    }

    // all hope is lost, collision table is full
    break;
  }

  return slot;
}

static FA_GlyphRun *FA_CreateGlyphRunInLayer(U32 layer_index, U64 hash, I16 orig_width, I16 orig_height)
{
  AssertBounds(layer_index, APP.atlas.layers);
  FA_Layer *layer = APP.atlas.layers + layer_index;

  I32 margin = APP.atlas.margin;
  I32 margin2 = margin * 2;

  I16 width = orig_width + margin2;
  I16 height = orig_height + margin2;
  I16 max_dim = APP.atlas.texture_dim;
  I16 all_lines_height = 0;

  // find best line
  U32 best_line_index = U32_MAX;
  ForU32(line_index, layer->line_count)
  {
    I16 line_height = layer->line_heights[line_index];
    I16 line_advance = layer->line_advances[line_index];
    I16 horizontal_left_in_line = max_dim - line_advance;

    all_lines_height += line_height;

    bool can_use_line = true;
    if (line_height < height)            can_use_line = false;
    if (horizontal_left_in_line < width) can_use_line = false;

    if (can_use_line)
    {
      if (best_line_index >= FONT_ATLAS_LAYER_MAX_LINES) // first acceptable line found
        best_line_index = line_index;
      else if (layer->line_heights[best_line_index] < line_height) // check if this line is better than current best line
        best_line_index = line_index;
    }
  }

  float height_left = max_dim - all_lines_height;
  if (best_line_index < FONT_ATLAS_LAYER_MAX_LINES && height_left >= height)
  {
    // heuristics for: should we start a new line if using "best line" wastes a lot of height space
    I16 best_line_height = layer->line_heights[best_line_index];
    I16 waste = best_line_height - height;
    float waste_share = (float)waste / (float)best_line_height;

    if (waste > 4 && waste_share > 0.2f)
      best_line_height = FONT_ATLAS_LAYER_MAX_LINES;
  }

  // if no best line - try to create a create new line
  if (best_line_index >= FONT_ATLAS_LAYER_MAX_LINES && height_left >= height)
  {
    if (layer->line_count < FONT_ATLAS_LAYER_MAX_LINES)
    {
      best_line_index = layer->line_count;
      layer->line_count += 1;

      layer->line_heights[best_line_index] = height;
    }
  }

  // We have best line line with free space - try to allocate a slot in hash table
  if (best_line_index < FONT_ATLAS_LAYER_MAX_LINES)
  {
    FA_GlyphRun *slot = FA_FindGlyphRunInLayer(layer_index, hash, true);
    if (!slot->hash)
    {
      I16 line_y_offset = 0;
      ForU32(line_index, best_line_index)
        line_y_offset += layer->line_heights[line_index];

      slot->hash = hash;
      slot->p.x = layer->line_advances[best_line_index] + APP.atlas.margin;
      slot->p.y = line_y_offset + APP.atlas.margin;
      slot->dim.x = orig_width;
      slot->dim.y = orig_height;
      slot->layer = layer_index;
      slot->debug_line_index = best_line_index;

      layer->line_advances[best_line_index] += width;
      return slot;
    }
  }

  // Failed to allocate glyph run in this layer
  return 0;
}

static FA_GlyphRun FA_GetGlyphRun(FA_Font font, S8 text)
{
  FA_GlyphRun result = {};
  if (APP.headless)
    return result;

  I32 margin = APP.atlas.margin;
  I32 margin2 = margin * 2;

  AssertBounds(font, APP.atlas.fonts);
  U64 hash = FA_TextHash(font, text);

  // Find already existing glyph run
  U32 layer_count_minus_one = ArrayCount(APP.atlas.layers) - 1;
  ForU32(layer_offset, layer_count_minus_one)
  {
    U32 layer_index = (APP.atlas.active_layer + layer_offset) % ArrayCount(APP.atlas.layers);
    FA_GlyphRun *glyph_run = FA_FindGlyphRunInLayer(layer_index, hash, false);
    if (glyph_run->hash == hash)
      return *glyph_run;
  }

  // Create surface, now we have width x height
  SDL_Color color = {255,255,255,255};
  TTF_Font *ttf_font = APP.atlas.fonts[font][0];
  SDL_Surface *surf = TTF_RenderText_Blended(ttf_font, (char *)text.str, text.size, color);
  if (!surf)
    return result;

  // Reject surfaces that are too big
  if (surf->w + margin2 > APP.atlas.texture_dim ||
      surf->h + margin2 > APP.atlas.texture_dim)
  {
    SDL_DestroySurface(surf);
    return result;
  }

  FA_GlyphRun *slot = 0;
  ForU32(layer_offset, layer_count_minus_one)
  {
    U32 layer_index = (APP.atlas.active_layer + layer_offset) % ArrayCount(APP.atlas.layers);
    slot = FA_CreateGlyphRunInLayer(layer_index, hash, surf->w, surf->h);
    if (slot)
      break;
  }

  if (!slot) // couldn't find space in any layer
  {
    // advance active_layer index (backwards) and clear active layer
    APP.atlas.active_layer += ArrayCount(APP.atlas.layers) - 1;
    APP.atlas.active_layer %= ArrayCount(APP.atlas.layers);
    SDL_zerop(APP.atlas.layers + APP.atlas.active_layer);

    slot = FA_CreateGlyphRunInLayer(APP.atlas.active_layer, hash, surf->w, surf->h);
  }

  // Upload surface to GPU (if free slot was found)
  if (slot)
  {
    I32 comp_size = 4; // @todo query format size or assert on format
    I32 surf_with_margin_size = (surf->h + margin2) * (surf->w + margin2) * comp_size;

    {
      SDL_GPUTransferBufferCreateInfo trans_cpu =
      {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = surf_with_margin_size
      };
      SDL_GPUTransferBuffer *buf_transfer = SDL_CreateGPUTransferBuffer(APP.gpu.device, &trans_cpu);

      // CPU memory -> GPU memory
      {
        void *mapped_memory = SDL_MapGPUTransferBuffer(APP.gpu.device, buf_transfer, false);
        memset(mapped_memory, 0, surf_with_margin_size);

        ForI32(y, surf->h)
        {
          U8 *dst = (U8 *)mapped_memory + (y * (surf->w + margin2) * comp_size);
          U8 *src = (U8 *)surf->pixels  + (y * surf->pitch);
          memcpy(dst, src, surf->w*comp_size);
        }

        SDL_UnmapGPUTransferBuffer(APP.gpu.device, buf_transfer);
      }

      // GPU memory -> GPU texture
      {
        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(APP.gpu.device);
        SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);

        SDL_GPUTextureTransferInfo trans_gpu =
        {
          .transfer_buffer = buf_transfer,
          .offset = 0,
        };

        Assert(slot->p.x >= margin);
        Assert(slot->p.y >= margin);
        Assert(slot->p.x + slot->dim.x + margin <= APP.atlas.texture_dim);
        Assert(slot->p.y + slot->dim.y + margin <= APP.atlas.texture_dim);
        SDL_GPUTextureRegion dst_region =
        {
          .texture = APP.gpu.ui_atlas_tex,
          .layer = slot->layer,
          .x = slot->p.x - margin,
          .y = slot->p.y - margin,
          .w = slot->dim.x + margin2,
          .h = slot->dim.y + margin2,
          .d = 1,
        };
        SDL_UploadToGPUTexture(copy_pass, &trans_gpu, &dst_region, false);

        SDL_EndGPUCopyPass(copy_pass);
        SDL_SubmitGPUCommandBuffer(cmd);
      }

      SDL_ReleaseGPUTransferBuffer(APP.gpu.device, buf_transfer);
    }

    result = *slot;
  }

  SDL_DestroySurface(surf);
  return result;
}

static void FA_ProcessWindowResize(bool init)
{
  if (APP.window_resized || init)
  {
    float scale = APP.window_height / 16.f;
    TTF_SetFontSize(APP.atlas.fonts[FA_Regular][0], scale);
    TTF_SetFontSize(APP.atlas.fonts[FA_Regular][1], scale);

    I32 new_texture_dim = U32_CeilPow2(APP.window_height); // @todo consider window area instead of height
    new_texture_dim = Max(256, new_texture_dim);

    bool resize_texture = false;
    resize_texture |= new_texture_dim > APP.atlas.texture_dim; // texture grew
    resize_texture |= new_texture_dim*2 < APP.atlas.texture_dim; // texure shrank by at least 4x

    if (resize_texture || init)
    {
      LOG(Log_GPU, "Font Atlas resize from %d to %d", (I32)APP.atlas.texture_dim, new_texture_dim);

      APP.atlas.texture_dim = new_texture_dim;

      // Initialize atlas layers bookkeeping structs
      {
        SDL_zeroa(APP.atlas.layers); // clear layers

        // Pre-seed layers to be full.
        // This isn't necessary but will result in more optimal fill order.
        ForArray(i, APP.atlas.layers)
        {
          FA_Layer *layer = APP.atlas.layers + i;
          layer->line_count = 1;
          layer->line_heights[0] = APP.atlas.texture_dim;
          layer->line_advances[0] = APP.atlas.texture_dim;
        }
      }

      if (APP.gpu.ui_atlas_tex)
        SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.ui_atlas_tex);

      SDL_GPUTextureCreateInfo tex_info =
      {
        .type = SDL_GPU_TEXTURETYPE_2D_ARRAY,
        .format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
        .width = APP.atlas.texture_dim,
        .height = APP.atlas.texture_dim,
        .layer_count_or_depth = FONT_ATLAS_LAYERS,
        .num_levels = GPU_MipMapCount(APP.atlas.texture_dim, APP.atlas.texture_dim),
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COLOR_TARGET
      };
      APP.gpu.ui_atlas_tex = SDL_CreateGPUTexture(APP.gpu.device, &tex_info);
      SDL_SetGPUTextureName(APP.gpu.device, APP.gpu.ui_atlas_tex, "UI Atlas Texture");
    }
  }
}

static void FA_Init()
{
  APP.atlas.margin = 1;
  float default_size = 120.f;

  // load font
  {
    TTF_Font *emoji_font = TTF_OpenFont("../res/fonts/NotoColorEmoji-Regular.ttf", default_size);
    TTF_Font *font = TTF_OpenFont("../res/fonts/Jacquard24-Regular.ttf", default_size);
    TTF_AddFallbackFont(font, emoji_font);
    APP.atlas.fonts[FA_Regular][0] = font;
    APP.atlas.fonts[FA_Regular][1] = emoji_font;
  }

  FA_ProcessWindowResize(true);
}
