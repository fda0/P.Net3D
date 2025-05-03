static void BAKER_CompressTexture(BREAD_Builder *bb, TEX_Kind tex_kind)
{
  Assert(tex_kind < TEX_COUNT);
  S8 tex_name = TEX_GetName(tex_kind);

  struct
  {
    S8 postfix;
    U8 path_buffer[1024];
    SDL_Surface *surface;
  } files[] =
  {
    {S8Lit("Color.jpg")},
    {S8Lit("NormalGL.jpg")},
    {S8Lit("Roughness.jpg")},
    {S8Lit("Displacement.jpg")},
  };

  ForArray(i, files)
  {
    Printer p = Pr_Make(files[i].path_buffer, sizeof(files[i].path_buffer));
    Pr_S8(&p, S8Lit("../res/tex/"));
    Pr_S8(&p, tex_name);
    Pr_S8(&p, S8Lit("/"));
    Pr_S8(&p, tex_name);
    Pr_S8(&p, S8Lit("_1K-JPG_"));
    Pr_S8(&p, files[i].postfix);

    const char *path = Pr_AsCstr(&p);
    files[i].surface = IMG_Load(path);
    M_Check(files[i].surface);
  }

  // Convert to RGBA if needed
  ForArray(i, files)
  {
    SDL_Surface *current_surface = files[i].surface;
    if (current_surface->format != SDL_PIXELFORMAT_RGBA32)
    {
      files[i].surface = SDL_ConvertSurface(current_surface, SDL_PIXELFORMAT_RGBA32);
      M_Check(files[i].surface);
      SDL_DestroySurface(current_surface);
    }
  }

  // Check that all files have the same dimensions
  I32 width = files[0].surface->w;
  I32 height = files[0].surface->h;
  ForArray(i, files)
  {
    M_Check(files[i].surface->w == width &&
            files[i].surface->h == height);
  }

  // Allocate and prepare BREAD_Material
  BREAD_Material *br_material = BREAD_Reserve(&bb->materials, BREAD_Material, 1);
  bb->materials_count += 1;

  br_material->width = width;
  br_material->height = height;
  br_material->layers = ArrayCount(files);

  // Iterate over chunks and compress them
#define M_TEX_CHUNK_DIM 4
#define M_TEX_CHUNK_PITCH (M_TEX_CHUNK_DIM * 4)
  M_Check(width % M_TEX_CHUNK_DIM == 0);
  M_Check(height % M_TEX_CHUNK_DIM == 0);
  I32 chunks_count_x = width / M_TEX_CHUNK_DIM;
  I32 chunks_count_y = height / M_TEX_CHUNK_DIM;
  I32 chunks_total_count = chunks_count_x * chunks_count_y;

  BREAD_RangeStart(&bb->file, &br_material->bc7_blocks);

  ForArray(file_index, files)
  {
    U32 br_pixel_adv = 2;
    U32 br_alloc_count = chunks_total_count * br_pixel_adv;
    U64 *br_file = BREAD_Reserve(&bb->file, U64, br_alloc_count);
    U64 *br_file_end = br_file + br_alloc_count;
    U64 *br_pixels = br_file;

    U8 *row = (U8 *)files[file_index].surface->pixels;
    I32 pitch = files[file_index].surface->pitch;
    U8 *file_end = (U8 *)files[file_index].surface->pixels + (pitch * height);

    ForI32(chunk_y, chunks_count_y)
    {
      U8 *chunk_start = row;
      ForI32(chunk_x, chunks_count_x)
      {
        M_Check(chunk_start < file_end);

        // Copy chunk into temporary 4x4 continuous buffer
        {
          U8 rgba_block[M_TEX_CHUNK_PITCH * M_TEX_CHUNK_PITCH];
          U8 *rgba_block_end = rgba_block + sizeof(rgba_block);

          ForI32(row_index, M_TEX_CHUNK_DIM)
          {
            U8 *dst = rgba_block + row_index*M_TEX_CHUNK_PITCH;
            U8 *src = chunk_start + row_index*pitch;
            I32 copy_size = sizeof(U32)*M_TEX_CHUNK_DIM;
            M_Check(dst + copy_size <= rgba_block_end);
            M_Check(src + copy_size <= file_end);
            Memcpy(dst, src, copy_size);
          }

          M_Check(br_pixels < br_file_end);
          bc7enc_bool has_alpha = bc7enc_compress_block(br_pixels, rgba_block, &BAKER.tex.params);
          (void)has_alpha;
          br_pixels += br_pixel_adv;
        }

        chunk_start += M_TEX_CHUNK_PITCH;
      }

      row += pitch * M_TEX_CHUNK_DIM;
    }
  }

  U32 bc7_total_block_count = chunks_count_x * chunks_count_y * br_material->layers * 2;
  BREAD_RangeEnd(&bb->file, &br_material->bc7_blocks, bc7_total_block_count);
}
