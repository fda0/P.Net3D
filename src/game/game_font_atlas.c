static U64 FA_TextHash(FA_Font font, S8 text)
{
  AssertBounds(font, APP.atlas.fonts);
  float point_size = TTF_GetFontSize(APP.atlas.fonts[font]);
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

static FA_GlyphRun *FA_CreateGlyphRunInLayer(U32 layer_index, U64 hash, float orig_width, float orig_height)
{
  AssertBounds(layer_index, APP.atlas.layers);
  FA_Layer *layer = APP.atlas.layers + layer_index;

  float width = orig_width + 2 * APP.atlas.margin;
  float height = orig_height + 2 * APP.atlas.margin;
  float max_dim = APP.atlas.texture_dim;
  float all_lines_height = 0.f;

  // find best line
  U32 best_line_index = U32_MAX;
  ForU32(line_index, layer->line_count)
  {
    float line_height = layer->line_heights[line_index];
    float line_advance = layer->line_advances[line_index];
    float horizontal_left_in_line = max_dim - line_advance;

    all_lines_height += line_height;

    bool can_use_line = true;
    if (line_height < height)            can_use_line = false;
    if (horizontal_left_in_line < width) can_use_line = false;

    if (can_use_line)
    {
      if (best_line_index >= FONT_ATLAS_LAYER_MAX_LINES) // first acceptable line found
        best_line_index = line_index;
      else if (layer->line_heights[best_line_index] < line_height) // check if this line is better
        best_line_index = line_height;
    }
  }

  float height_left = max_dim - all_lines_height;
  if (best_line_index < FONT_ATLAS_LAYER_MAX_LINES && height_left >= height)
  {
    // heuristics for: should we start a new line if using "best line" wastes a lot of height space
    float best_line_height = layer->line_heights[best_line_index];
    float waste = best_line_height - height;
    float waste_share = waste / best_line_height;

    if (waste > 4.f && waste_share > 0.2f)
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
      float line_y_offset = 0.f;
      ForU32(line_index, best_line_index)
        line_y_offset += layer->line_heights[line_index];

      slot->hash = hash;
      slot->p.x = layer->line_advances[best_line_index] + APP.atlas.margin;
      slot->p.y = line_y_offset + APP.atlas.margin;
      slot->dim.x = orig_width;
      slot->dim.y = orig_height;
      slot->layer = layer_index;

      layer->line_advances[best_line_index] += width;
      return slot;
    }
  }

  // Failed to allocate glyph run in this layer
  return 0;
}

static FA_GlyphRun FA_GetGlyphRun(FA_Font font, S8 text)
{
  U64 hash = FA_TextHash(font, text);

  // Find already existing glyph run
  U32 layer_count_minus_one = ArrayCount(APP.atlas.layers);
  ForU32(layer_offset, layer_count_minus_one)
  {
    U32 layer_index = (APP.atlas.active_layer + layer_offset) % ArrayCount(APP.atlas.layers);
    FA_GlyphRun *glyph_run = FA_FindGlyphRunInLayer(layer_index, hash, false);
    if (glyph_run->hash == hash)
      return *glyph_run;
  }

  // Create surface, now we have width x height
  // @todo
  // @todo reject something that doesnt fit in texture dim

  // Find slot for new glyph run
  // @todo
  // 1. try 3 layers in order
  // 2. try to fit glyph in the best slot
  // 3. if we can't find any slot then advance active_layer and use that
}

static void FA_ProcessWindowResize()
{
  float scale = APP.window_height / 16.f;
  TTF_SetFontSize(APP.atlas.fonts[FA_Regular], scale);

  U32 new_texture_dim = U32_CeilPow2(APP.window_height);
  new_texture_dim = Min(256, new_texture_dim);

  bool resize_texture = false;
  resize_texture |= new_texture_dim > APP.atlas.texture_dim; // texture grew
  resize_texture |= new_texture_dim*2 < APP.atlas.texture_dim; // texure shrank by at least 4x

  if (resize_texture)
  {
    APP.atlas.texture_dim = new_texture_dim;
    SDL_zeroa(APP.atlas.layers);

    if (APP.gpu.font_atlas_tex)
      SDL_ReleaseGPUTexture(APP.gpu.device, APP.gpu.font_atlas_tex);

    SDL_GPUTextureCreateInfo tex_info =
    {
      .type = SDL_GPU_TEXTURETYPE_2D_ARRAY,
      .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
      .width = APP.atlas.texture_dim,
      .height = APP.atlas.texture_dim,
      .layer_count_or_depth = FONT_ATLAS_LAYERS,
      .num_levels = GPU_MipMapCount(APP.atlas.texture_dim, APP.atlas.texture_dim),
      .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COLOR_TARGET
    };
    APP.gpu.font_atlas_tex = SDL_CreateGPUTexture(APP.gpu.device, &tex_info);
    SDL_SetGPUTextureName(APP.gpu.device, APP.gpu.font_atlas_tex, "Font Atlas Texture");
  }
}

static void FA_Init()
{
  APP.atlas.margin = 1;
  float default_size = 16.f;

  // load font
  {
    TTF_Font *emoji_font = TTF_OpenFont("../res/fonts/NotoColorEmoji-Regular.ttf", default_size);
    TTF_Font *font = TTF_OpenFont("../res/fonts/Jacquard24-Regular.ttf", default_size);
    TTF_AddFallbackFont(font, emoji_font);
    APP.atlas.fonts[FA_Regular] = font;

#if 0
    SDL_Color color = {255,255,255,255};
    S8 text = S8Lit("hello łabędź! 🦢");
    int width = 0;
    bool res = TTF_MeasureString(font, (char *)text.str, text.size, 0, &width, 0);
    SDL_Surface *surf = TTF_RenderText_Blended(font, (char *)text.str, text.size, color);
    int a = 1;
    a += 1;
#endif
  }
}
