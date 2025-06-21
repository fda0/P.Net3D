BK_TEX_LoadPaths :: (pie_material: *PIE_Material, paths: [] string)
{
  auto_release_temp();
  push_allocator(temp);

  BK_TexFile :: struct
  {
    BK_MIPMAPS_MAX :: 16;
    surfs: [BK_MIPMAPS_MAX] *SDL_Surface;
  };
  files := NewArray(paths.count, BK_TexFile);

  errors := false;
  for * files
  {
    path := paths[it_index];
    it.surfs[0] = IMG_Load(temp_c_string(path));
    if !it.surfs[0]
    {
      log_error("Failed to open texture file %", path);
      errors = true;
      break;
    }
  }

  if !errors
  {
    // Check that all files have the same dimensions
    orig_width := files[0].surfs[0].w;
    orig_height := files[0].surfs[0].h;
    for files
      Check(it.surfs[0].w == orig_width && it.surfs[0].h == orig_height);

    // Convert to RGBA if needed
    target_format := SDL_PixelFormat.SDL_PIXELFORMAT_RGBA32;
    for * files
    {
      current_surface := it.surfs[0];
      if current_surface.format != target_format
      {
        it.surfs[0] = SDL_ConvertSurface(current_surface, target_format);
        Check(it.surfs[0] != null);
        SDL_DestroySurface(current_surface);
      }
    }

    // Prepare mipmap sufraces
    lods_count: s32 = 1;
    if B.tex.format == .BC7_RGBA
    {
      lods_count = CalculateMipMapCount(orig_width, orig_height);
      Check(BK_TexFile.BK_MIPMAPS_MAX >= lods_count);
    }

    for * files
    {
      lod_dim := Vec2(s32).{orig_width, orig_height};
      for lod_index: MakeRange(lods_count)
      {
        Check(lod_dim.x > 1 || lod_dim.y > 1);
        lod_dim = max(lod_dim / 2, 1);

        it.surfs[lod_index] = SDL_CreateSurface(lod_dim.x, lod_dim.y, target_format);
        Check(it.surfs[lod_index].w == lod_dim.x);
        Check(it.surfs[lod_index].h == lod_dim.y);

        SDL_BlitSurfaceScaled(it.surfs[lod_index-1], null, it.surfs[lod_index], null, .SDL_SCALEMODE_LINEAR);
      }
    }

    // Set material texture values
    Check(!pie_material.tex.format);
    Check(B.tex.format != .Empty);
    pie_material.tex.format = B.tex.format;
    pie_material.tex.width = orig_width;
    pie_material.tex.height = orig_height;
    pie_material.tex.lods = lods_count;
    pie_material.tex.layers = xx paths.count;

    pie_textures_count := pie_material.tex.lods * pie_material.tex.layers;
    pie_textures := PIE_ListAllocArray(*pie_material.tex.sections, pie_textures_count, PIE_MaterialTexSection);

  //   PIE_ListStart(&build.file, &pie_material.tex.full_data, TYPE_U8);

  //   // Iterate over fliles
  //   if (pie_material.tex.format == PIE_Tex_R8G8B8A8)
  //   {
  //     U32 pie_texture_index = 0;
  //     ForU32(file_index, paths_count)
  //     {
  //       BK_TexFile *file = files + file_index;

  //       // Iterate over lods
  //       ForU32(lod_index, lods_count)
  //       {
  //         SDL_Surface *surf = file.surfs[lod_index];

  //         M_Check(pie_texture_index < pie_textures_count);
  //         PIE_MaterialTexSection *pie_tex = pie_textures + pie_texture_index;
  //         pie_texture_index += 1;

  //         // Calc
  //         U32 pie_data_size = surf.w * surf.h * sizeof(U32);

  //         // Fill texture data
  //         pie_tex.width = surf.w;
  //         pie_tex.height = surf.h;
  //         pie_tex.lod = lod_index;
  //         pie_tex.layer = file_index;
  //         pie_tex.data_offset = build.file.used - pie_material.tex.full_data.offset;
  //         pie_tex.data_size = pie_data_size;

  //         // Alloc data buffer
  //         U8 *pie_data = PIE_Reserve(&build.file, U8, pie_data_size);

  //         // Copy data to .pie file buffer
  //         if (surf.w * sizeof(U32) == surf.pitch)
  //         {
  //           Memcpy(pie_data, surf.pixels, pie_data_size);
  //         }
  //         else
  //         {
  //           M_Check(false && !"not implemnted");
  //         }

  //         SDL_DestroySurface(surf);
  //       }
  //     }
  //   }
  //   else if (pie_material.tex.format == PIE_Tex_BC7_RGBA)
  //   {
  //     U32 pie_texture_index = 0;
  //     ForU32(file_index, paths_count)
  //     {
  //       BK_TexFile *file = files + file_index;

  //       // Iterate over lods
  //       ForU32(lod_index, lods_count)
  //       {
  //         SDL_Surface *surf = file.surfs[lod_index];

  //         M_Check(pie_texture_index < pie_textures_count);
  //         PIE_MaterialTexSection *pie_tex = pie_textures + pie_texture_index;
  //         pie_texture_index += 1;

  //         // Calculate data buffer dimensions
  //         I32 lod_chunks_per_w = (surf.w + M_TEX_BC7_BLOCK_DIM - 1) / M_TEX_BC7_BLOCK_DIM;
  //         I32 lod_chunks_per_h = (surf.h + M_TEX_BC7_BLOCK_DIM - 1) / M_TEX_BC7_BLOCK_DIM;
  //         I32 pie_data_size = lod_chunks_per_w*lod_chunks_per_h * sizeof(U64)*2;

  //         // Fill texture data
  //         pie_tex.width = surf.w;
  //         pie_tex.height = surf.h;
  //         pie_tex.lod = lod_index;
  //         pie_tex.layer = file_index;
  //         pie_tex.data_offset = build.file.used - pie_material.tex.full_data.offset;
  //         pie_tex.data_size = pie_data_size;

  //         // Alloc data buffer
  //         U8 *pie_data = PIE_Reserve(&build.file, U8, pie_data_size);
  //         U8 *pie_data_end = pie_data + pie_data_size;
  //         U8 *pie_pixels = pie_data;

  //         // Iterate over chunks of the current lod.
  //         // Fetch 4x4 chunks and compress them into memory allocated in .pie file.
  //         ForI32(chunk_y, lod_chunks_per_h)
  //         {
  //           I32 lod_y = chunk_y * M_TEX_BC7_BLOCK_DIM;
  //           I32 lod_h_left = surf.h - lod_y;
  //           I32 copy_h = Min(M_TEX_BC7_BLOCK_DIM, lod_h_left);

  //           ForI32(chunk_x, lod_chunks_per_w)
  //           {
  //             I32 lod_x = chunk_x * M_TEX_BC7_BLOCK_DIM;
  //             I32 lod_w_left = surf.w - lod_x;
  //             I32 copy_w = Min(M_TEX_BC7_BLOCK_DIM, lod_w_left);

  //             SDL_Rect src_rect =
  //             {
  //               .x = lod_x, .y = lod_y,
  //               .w = copy_w, .h = copy_h,
  //             };
  //             SDL_Rect dst_rect =
  //             {
  //               .x = 0, .y = 0,
  //               .w = copy_w, .h = copy_h,
  //             };

  //             SDL_Surface *block_surf = BAKER.tex.bc7_block_surf;
  //             SDL_BlitSurfaceTiled(surf, &src_rect, block_surf, &dst_rect);

  //             bc7enc_compress_block(pie_pixels, block_surf.pixels, &BAKER.tex.params);
  //             pie_pixels += sizeof(U64)*2;
  //           }
  //         }

  //         M_Check(pie_pixels == pie_data_end);
  //         SDL_DestroySurface(surf);
  //       }
  //     }
  //   }

  //   PIE_ListEnd(&build.file, &pie_material.tex.full_data);
  }

  // if (errors)
  // {
  //   ForU32(file_index, paths_count)
  //   {
  //     BK_TexFile *file = files + file_index;
  //     if (file.surfs[0]) SDL_DestroySurface(file.surfs[0]);
  //   }
  // }
}
