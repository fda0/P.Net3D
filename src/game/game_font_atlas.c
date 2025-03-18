static U64 FA_TextHash(FA_Font font, S8 text)
{
  AssertBounds(font, APP.atlas.fonts);
  float point_size = TTF_GetFontSize(APP.atlas.fonts[font]);
  U64 hash = U64_Hash(font, &point_size, sizeof(point_size));
  hash = S8_Hash(hash, text);
  return hash;
}

static void FA_ProcessWindowResize()
{
  float scale = APP.window_height / 16.f;
  TTF_SetFontSize(APP.atlas.fonts[FA_Regular], scale);

  U32 new_texture_dim = U32_CeilPow2(APP.window_height);
  new_texture_dim = Min(256, new_texture_dim);

  if (APP.atlas.texture_dim != new_texture_dim)
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
