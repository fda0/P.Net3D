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

static void BK_TEX_Load(TEX_Kind tex_kind, BREAD_TexFormat format)
{
  BREAD_Builder *bb = &BAKER.bb;

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
  U32 lods_count = 1;
  if (format == BREAD_Tex_BC7_RGBA)
  {
    lods_count = BK_TEX_CalculateMipMapCount(orig_width, orig_height);
    M_Check(BK_MIPMAPS_MAX >= lods_count);
  }

  ForArray(file_index, files)
  {
    BK_MaterialFile *file = files + file_index;

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
  // Allocate and prepare BREAD_Material & array of BREAD_Texture
  bb->materials_count += 1;
  BREAD_Material *br_material = BREAD_Reserve(&bb->materials, BREAD_Material, 1);
  br_material->format = format;
  br_material->width = orig_width;
  br_material->height = orig_height;
  br_material->lods = lods_count;
  br_material->layers = ArrayCount(files);

  U32 br_sections_count = br_material->lods * br_material->layers;
  BREAD_MaterialSection *br_sections = BREAD_ListReserve(&bb->file, &br_material->sections,
                                                         BREAD_MaterialSection, br_sections_count);

  BREAD_ListStart(&bb->file, &br_material->full_data, TYPE_U8);

  // Iterate over fliles
  if (format == BREAD_Tex_R8G8B8A8)
  {
    U32 br_section_index = 0;
    ForArray(file_index, files)
    {
      BK_MaterialFile *file = files + file_index;

      // Iterate over lods
      ForU32(lod_index, lods_count)
      {
        SDL_Surface *surf = file->surfs[lod_index];

        M_Check(br_section_index < br_sections_count);
        BREAD_MaterialSection *br_sect = br_sections + br_section_index;
        br_section_index += 1;

        // Calc
        U32 br_data_size = surf->w * surf->h * sizeof(U32);

        // Fill section data
        br_sect->width = surf->w;
        br_sect->height = surf->h;
        br_sect->lod = lod_index;
        br_sect->layer = file_index;
        br_sect->data_offset = bb->file.used - br_material->full_data.offset;
        br_sect->data_size = br_data_size;

        // Alloc data buffer
        U8 *br_data = BREAD_Reserve(&bb->file, U8, br_data_size);

        // Copy data to .bread file buffer
        if (surf->w * sizeof(U32) == surf->pitch)
        {
          Memcpy(br_data, surf->pixels, br_data_size);
        }
        else
        {
          M_Check(false && !"not implemnted");
        }

        SDL_DestroySurface(surf);
      }
    }
  }
  else if (format == BREAD_Tex_BC7_RGBA)
  {
    U32 br_section_index = 0;
    ForArray(file_index, files)
    {
      BK_MaterialFile *file = files + file_index;

      // Iterate over lods
      ForU32(lod_index, lods_count)
      {
        SDL_Surface *surf = file->surfs[lod_index];

        M_Check(br_section_index < br_sections_count);
        BREAD_MaterialSection *br_sect = br_sections + br_section_index;
        br_section_index += 1;

        // Calculate data buffer dimensions
        I32 lod_chunks_per_w = (surf->w + M_TEX_BC7_BLOCK_DIM - 1) / M_TEX_BC7_BLOCK_DIM;
        I32 lod_chunks_per_h = (surf->h + M_TEX_BC7_BLOCK_DIM - 1) / M_TEX_BC7_BLOCK_DIM;
        I32 br_data_size = lod_chunks_per_w*lod_chunks_per_h * sizeof(U64)*2;

        // Fill section data
        br_sect->width = surf->w;
        br_sect->height = surf->h;
        br_sect->lod = lod_index;
        br_sect->layer = file_index;
        br_sect->data_offset = bb->file.used - br_material->full_data.offset;
        br_sect->data_size = br_data_size;

        // Alloc data buffer
        U8 *br_data = BREAD_Reserve(&bb->file, U8, br_data_size);
        U8 *br_data_end = br_data + br_data_size;
        U8 *br_pixels = br_data;

        // Iterate over chunks of the current lod.
        // Fetch 4x4 chunks and compress them into memory allocated in .bread file.
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

            bc7enc_compress_block(br_pixels, block_surf->pixels, &BAKER.tex.params);
            br_pixels += sizeof(U64)*2;
          }
        }

        M_Check(br_pixels == br_data_end);
        SDL_DestroySurface(surf);
      }
    }
  }

  BREAD_ListEnd(&bb->file, &br_material->full_data);
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
