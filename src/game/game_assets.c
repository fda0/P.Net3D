static SDL_GPUTexture *TEX_GetGPUTexture(TEX_Kind tex_kind)
{
  (void)tex_kind;
  return APP.gpu.fallback_texture;
}
