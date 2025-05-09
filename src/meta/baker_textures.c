// @todo:
// - Mipmapping
// - Explore other texture compression formats (BC5 for normal maps?)

static U32 BK_CalculateMipMapCount(U32 width, U32 height)
{
  U32 max_dim = Max(width, height);
  I32 msb = MostSignificantBitU32(max_dim);
  if (msb < 0) return 0;
  return msb;
}

static void BK_CompressTexture(BREAD_Builder *bb, TEX_Kind tex_kind)
{
  Assert(tex_kind < TEX_COUNT);
  S8 tex_name = TEX_GetName(tex_kind);

  typedef struct
  {
#define BK_MIPMAPS_MAX 16
    S8 postfix;
    U8 path_buffer[1024];
    SDL_Surface *surfs[BK_MIPMAPS_MAX];
  } BK_MaterialFile;

  BK_MaterialFile files[] =
  {
    {S8Lit("Color.jpg")},
    {S8Lit("NormalGL.jpg")},
    {S8Lit("Roughness.jpg")},
    {S8Lit("Displacement.jpg")},
  };

  ForArray(file_index, files)
  {
    BK_MaterialFile *file = files + file_index;

    Printer p = Pr_Make(file->path_buffer, sizeof(file->path_buffer));
    Pr_S8(&p, S8Lit("../res/tex/"));
    Pr_S8(&p, tex_name);
    Pr_S8(&p, S8Lit("/"));
    Pr_S8(&p, tex_name);
    Pr_S8(&p, S8Lit("_1K-JPG_"));
    Pr_S8(&p, file->postfix);

    const char *path = Pr_AsCstr(&p);
    file->surfs[0] = IMG_Load(path);
    M_Check(file->surfs[0]);
  }

  // Check that all files have the same dimensions
  I32 orig_width = files[0].surfs[0]->w;
  I32 orig_height = files[0].surfs[0]->h;
  ForArray(file_index, files)
  {
    BK_MaterialFile *file = files + file_index;
    M_Check(file->surfs[0]->w == orig_width && file->surfs[0]->h == orig_height);
  }

  // Convert to RGBA if needed
  SDL_PixelFormat target_format = SDL_PIXELFORMAT_RGBA32;
  ForArray(file_index, files)
  {
    BK_MaterialFile *file = files + file_index;

    SDL_Surface *current_surface = file->surfs[0];
    if (current_surface->format != target_format)
    {
      file->surfs[0] = SDL_ConvertSurface(current_surface, target_format);
      M_Check(file->surfs[0]);
      SDL_DestroySurface(current_surface);
    }
  }

  // Prepare mipmap sufraces
  U32 lods_count = BK_CalculateMipMapCount(orig_width, orig_height);
  M_Check(BK_MIPMAPS_MAX >= lods_count);

  ForArray(file_index, files)
  {
    BK_MaterialFile *file = files + file_index;

    V2U32 lod_dim = (V2U32){orig_width, orig_height};
    for (U32 lod_index = 1; lod_index < lods_count; lod_index += 1)
    {
      lod_dim = V2U32_DivScalar(lod_dim, 2);
      M_Check(lod_dim.x >= 1 && lod_dim.y >= 1);
      file->surfs[lod_index] = SDL_CreateSurface(lod_dim.x, lod_dim.y, target_format);
      SDL_BlitSurfaceScaled(file->surfs[0], 0, file->surfs[lod_index], 0, SDL_SCALEMODE_LINEAR);
    }
  }

  //
  //
  //
#define M_TEX_CHUNK_DIM 4
#define M_TEX_CHUNK_PITCH (M_TEX_CHUNK_DIM * 4)

  // Allocate and prepare BREAD_Material & array of BREAD_Texture
  BREAD_Material *br_material = BREAD_Reserve(&bb->materials, BREAD_Material, 1);
  bb->materials_count += 1;
  BREAD_Texture *br_textures = BREAD_ListReserve(&bb->file, &br_material->textures, BREAD_Texture, ArrayCount(files));

  SDL_Surface *block_surf = BAKER.tex.bc7_block_surf;

  // Iterate over fliles
  ForArray(file_index, files)
  {
    BK_MaterialFile *file = files + file_index;
    BREAD_Texture *br_tex = br_textures + file_index;

    // Alloc lods
    BREAD_TextureLod *br_lods = BREAD_ListReserve(&bb->file, &br_tex->lods, BREAD_TextureLod, lods_count);
    // Iterate over lods
    ForU32(lod_index, lods_count)
    {
      SDL_Surface *surf = file->surfs[lod_index];
      BREAD_TextureLod *br_lod = br_lods + lod_index;
      br_lod->width = surf->w;
      br_lod->height = surf->h;

      // Shrink constants
      I32 shrink_factor = orig_width / surf->w;
      M_Check(shrink_factor == orig_height / surf->h);
      I32 orig_sample_dim = M_TEX_CHUNK_DIM * shrink_factor;
      float shrink_scale = 1.f / (float)shrink_factor;

      // Alloc output data buf
      I32 lod_chunks_per_w = (surf->w + M_TEX_CHUNK_DIM - 1) / M_TEX_CHUNK_DIM;
      I32 lod_chunks_per_h = (surf->h + M_TEX_CHUNK_DIM - 1) / M_TEX_CHUNK_DIM;
      I32 block_data_size = lod_chunks_per_w*lod_chunks_per_h * sizeof(U64)*2;
      U8 *br_data = BREAD_ListReserve(&bb->file, &br_lod->data, U8, block_data_size);
      U8 *br_pixels = br_data;

      // Iterate over chunks of the current lod
      ForI32(chunk_y, lod_chunks_per_h)
      {
        I32 lod_y = chunk_y * M_TEX_CHUNK_DIM;
        I32 orig_y = lod_y * shrink_factor;
        I32 lod_h_left = surf->h - lod_y;

        ForI32(chunk_x, lod_chunks_per_w)
        {
          I32 lod_x = chunk_x * M_TEX_CHUNK_DIM;
          I32 orig_x = lod_x * shrink_factor;
          I32 lod_w_left = surf->w - lod_x;

          SDL_Rect src_rect =
          {
            .x = orig_x, .y = orig_y,
            .w = orig_sample_dim,
            .h = orig_sample_dim,
          };
          SDL_Rect dst_rect =
          {
            .x = 0, .y = 0,
            .w = Min(lod_w_left, M_TEX_CHUNK_DIM),
            .h = Min(lod_h_left, M_TEX_CHUNK_DIM),
          };
          bool res = SDL_BlitSurfaceTiledWithScale(surf, &src_rect, // @TODO I SHOULD NOT BE SCALING ANYTHING HERE; IM HALF ASLEEP AND DUMB
                                                   shrink_scale, SDL_SCALEMODE_LINEAR,
                                                   block_surf, &dst_rect);
          M_Check(res);

          M_Check(br_pixels < br_data + block_data_size);
          bc7enc_bool has_alpha = bc7enc_compress_block(br_pixels, block_surf->pixels, &BAKER.tex.params);
          (void)has_alpha;
          br_pixels += sizeof(U64)*2;
        }
      }

      SDL_DestroySurface(surf);
    }
  }
}
