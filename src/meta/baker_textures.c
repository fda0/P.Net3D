// @todo:
// - Multithread texture loading
// - Cache texture compression results
//   path: baker_cache/<hash_of_compressor_config>/file_name.bc7
//   This could work simply using OS timestamp.
//   If bc7 file exists and has timestamp that's newer than asset file -> load it.
// - Explore other texture compression formats (BC5 for normal maps?)

#define M_TEX_BC7_BLOCK_DIM 4

static U32 BK_TEX_CalculateMipMapCount(U32 width, U32 height)
{
  U32 max_dim = Max(width, height);
  I32 msb = MostSignificantBitU32(max_dim);
  if (msb < 0) return 0;
  return msb;
}

static void BK_TEX_Load(S8 tex_name, PIE_TexFormat format)
{
  PIE_Builder *build = &BAKER.pie_builder;

  typedef struct
  {
#define BK_MIPMAPS_MAX 16
    S8 postfix;
    U8 path_buffer[1024];
    SDL_Surface *surfs[BK_MIPMAPS_MAX];
  } BK_PBRTextureFile;

  BK_PBRTextureFile files[] =
  {
    {S8Lit("Color.jpg")},
    {S8Lit("NormalGL.jpg")},
    {S8Lit("Roughness.jpg")},
    //{S8Lit("Displacement.jpg")},
  };

  bool load_errors = false;
  ForArray(file_index, files)
  {
    BK_PBRTextureFile *file = files + file_index;

    Printer p = Pr_Make(file->path_buffer, sizeof(file->path_buffer));
    Pr_S8(&p, S8Lit("../res/tex/"));
    Pr_S8(&p, tex_name);
    Pr_S8(&p, S8Lit("/"));
    Pr_S8(&p, tex_name);
    Pr_S8(&p, S8Lit("_1K-JPG_"));
    Pr_S8(&p, file->postfix);

    const char *path = Pr_AsCstr(&p);
    file->surfs[0] = IMG_Load(path);
    if (!file->surfs[0])
    {
      M_LOG(M_TEXWarning, "Failed to open texture file %s", path);
      load_errors = true;
      break;
    }
  }

  if (load_errors)
  {
    ForArray(file_index, files)
    {
      BK_PBRTextureFile *file = files + file_index;
      if (file->surfs[0]) SDL_DestroySurface(file->surfs[0]);
    }
    return;
  }

  // Check that all files have the same dimensions
  I32 orig_width = files[0].surfs[0]->w;
  I32 orig_height = files[0].surfs[0]->h;
  ForArray(file_index, files)
  {
    BK_PBRTextureFile *file = files + file_index;
    M_Check(file->surfs[0]->w == orig_width && file->surfs[0]->h == orig_height);
  }

  // Convert to RGBA if needed
  SDL_PixelFormat target_format = SDL_PIXELFORMAT_RGBA32;
  ForArray(file_index, files)
  {
    BK_PBRTextureFile *file = files + file_index;

    SDL_Surface *current_surface = file->surfs[0];
    if (current_surface->format != target_format)
    {
      file->surfs[0] = SDL_ConvertSurface(current_surface, target_format);
      M_Check(file->surfs[0]);
      SDL_DestroySurface(current_surface);
    }
  }

  // Prepare mipmap sufraces
  U32 lods_count = 1;
  if (format == PIE_Tex_BC7_RGBA)
  {
    lods_count = BK_TEX_CalculateMipMapCount(orig_width, orig_height);
    M_Check(BK_MIPMAPS_MAX >= lods_count);
  }

  ForArray(file_index, files)
  {
    BK_PBRTextureFile *file = files + file_index;

    V2I32 lod_dim = (V2I32){orig_width, orig_height};
    for (U32 lod_index = 1; lod_index < lods_count; lod_index += 1)
    {
      lod_dim = V2I32_DivScalar(lod_dim, 2);
      M_Check(lod_dim.x >= 1 && lod_dim.y >= 1);

      file->surfs[lod_index] = SDL_CreateSurface(lod_dim.x, lod_dim.y, target_format);
      M_Check(file->surfs[lod_index]->w == lod_dim.x);
      M_Check(file->surfs[lod_index]->h == lod_dim.y);

      SDL_BlitSurfaceScaled(file->surfs[lod_index-1], 0, file->surfs[lod_index], 0, SDL_SCALEMODE_LINEAR);
    }
  }

  //
  //
  //
  // Allocate and prepare PIE_Material & array of PIE_Texture
  build->materials_count += 1;
  PIE_Material *pie_material = PIE_Reserve(&build->materials, PIE_Material, 1);
  BK_SetDefaultsPIEMaterial(pie_material);

  PIE_ListStart(&build->file, &pie_material->name, TYPE_U8);
  Pr_Cstr(&build->file, "tex.");
  Pr_S8(&build->file, tex_name);
  PIE_ListEnd(&build->file, &pie_material->name);

  pie_material->tex.format = format;
  pie_material->tex.width = orig_width;
  pie_material->tex.height = orig_height;
  pie_material->tex.lods = lods_count;
  pie_material->tex.layers = ArrayCount(files);

  PIE_Aling(&build->file, _Alignof(PIE_MaterialTexSection));
  U32 pie_textures_count = pie_material->tex.lods * pie_material->tex.layers;
  PIE_MaterialTexSection *pie_textures = PIE_ListReserve(&build->file, &pie_material->tex.sections,
                                                        PIE_MaterialTexSection, pie_textures_count);

  PIE_ListStart(&build->file, &pie_material->tex.full_data, TYPE_U8);

  // Iterate over fliles
  if (format == PIE_Tex_R8G8B8A8)
  {
    U32 pie_texture_index = 0;
    ForArray(file_index, files)
    {
      BK_PBRTextureFile *file = files + file_index;

      // Iterate over lods
      ForU32(lod_index, lods_count)
      {
        SDL_Surface *surf = file->surfs[lod_index];

        M_Check(pie_texture_index < pie_textures_count);
        PIE_MaterialTexSection *pie_tex = pie_textures + pie_texture_index;
        pie_texture_index += 1;

        // Calc
        U32 pie_data_size = surf->w * surf->h * sizeof(U32);

        // Fill texture data
        pie_tex->width = surf->w;
        pie_tex->height = surf->h;
        pie_tex->lod = lod_index;
        pie_tex->layer = file_index;
        pie_tex->data_offset = build->file.used - pie_material->tex.full_data.offset;
        pie_tex->data_size = pie_data_size;

        // Alloc data buffer
        U8 *pie_data = PIE_Reserve(&build->file, U8, pie_data_size);

        // Copy data to .pie file buffer
        if (surf->w * sizeof(U32) == surf->pitch)
        {
          Memcpy(pie_data, surf->pixels, pie_data_size);
        }
        else
        {
          M_Check(false && !"not implemnted");
        }

        SDL_DestroySurface(surf);
      }
    }
  }
  else if (format == PIE_Tex_BC7_RGBA)
  {
    U32 pie_texture_index = 0;
    ForArray(file_index, files)
    {
      BK_PBRTextureFile *file = files + file_index;

      // Iterate over lods
      ForU32(lod_index, lods_count)
      {
        SDL_Surface *surf = file->surfs[lod_index];

        M_Check(pie_texture_index < pie_textures_count);
        PIE_MaterialTexSection *pie_tex = pie_textures + pie_texture_index;
        pie_texture_index += 1;

        // Calculate data buffer dimensions
        I32 lod_chunks_per_w = (surf->w + M_TEX_BC7_BLOCK_DIM - 1) / M_TEX_BC7_BLOCK_DIM;
        I32 lod_chunks_per_h = (surf->h + M_TEX_BC7_BLOCK_DIM - 1) / M_TEX_BC7_BLOCK_DIM;
        I32 pie_data_size = lod_chunks_per_w*lod_chunks_per_h * sizeof(U64)*2;

        // Fill texture data
        pie_tex->width = surf->w;
        pie_tex->height = surf->h;
        pie_tex->lod = lod_index;
        pie_tex->layer = file_index;
        pie_tex->data_offset = build->file.used - pie_material->tex.full_data.offset;
        pie_tex->data_size = pie_data_size;

        // Alloc data buffer
        U8 *pie_data = PIE_Reserve(&build->file, U8, pie_data_size);
        U8 *pie_data_end = pie_data + pie_data_size;
        U8 *pie_pixels = pie_data;

        // Iterate over chunks of the current lod.
        // Fetch 4x4 chunks and compress them into memory allocated in .pie file.
        ForI32(chunk_y, lod_chunks_per_h)
        {
          I32 lod_y = chunk_y * M_TEX_BC7_BLOCK_DIM;
          I32 lod_h_left = surf->h - lod_y;
          I32 copy_h = Min(M_TEX_BC7_BLOCK_DIM, lod_h_left);

          ForI32(chunk_x, lod_chunks_per_w)
          {
            I32 lod_x = chunk_x * M_TEX_BC7_BLOCK_DIM;
            I32 lod_w_left = surf->w - lod_x;
            I32 copy_w = Min(M_TEX_BC7_BLOCK_DIM, lod_w_left);

            SDL_Rect src_rect =
            {
              .x = lod_x, .y = lod_y,
              .w = copy_w, .h = copy_h,
            };
            SDL_Rect dst_rect =
            {
              .x = 0, .y = 0,
              .w = copy_w, .h = copy_h,
            };

            SDL_Surface *block_surf = BAKER.tex.bc7_block_surf;
            SDL_BlitSurfaceTiled(surf, &src_rect, block_surf, &dst_rect);

            bc7enc_compress_block(pie_pixels, block_surf->pixels, &BAKER.tex.params);
            pie_pixels += sizeof(U64)*2;
          }
        }

        M_Check(pie_pixels == pie_data_end);
        SDL_DestroySurface(surf);
      }
    }
  }

  PIE_ListEnd(&build->file, &pie_material->tex.full_data);
}

static void BK_TEX_Init()
{
  // Init bc7enc texture compressor
  {
    bc7enc_compress_block_init();
    bc7enc_compress_block_params_init(&BAKER.tex.params);
    // There are params fields that we might want modify like m_uber_level.

    // comment out below to increase quality
    BAKER.tex.params.m_max_partitions_mode = 0; // lowest quality - highest speed

    // Learn which one should be picked.
    // Perhaps diffuse needs perpecetual and data linear? Idk
    bc7enc_compress_block_params_init_linear_weights(&BAKER.tex.params);
    //bc7enc_compress_block_params_init_perceptual_weights(&BAKER.tex.params);
  }

  BAKER.tex.bc7_block_surf = SDL_CreateSurface(M_TEX_BC7_BLOCK_DIM, M_TEX_BC7_BLOCK_DIM, SDL_PIXELFORMAT_RGBA32);
  M_Check(BAKER.tex.bc7_block_surf->w * sizeof(U32) == BAKER.tex.bc7_block_surf->pitch);
}
