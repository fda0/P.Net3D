typedef struct
{
    SDL_GPUBuffer *vert_buf;
    SDL_GPUBuffer *ind_buf;
    SDL_GPUBuffer *inst_buf;
    U32 ind_count;
} Gpu_ModelBuffers;

typedef struct
{
    SDL_GPUDevice *device;

    // window state
    SDL_GPUTexture *tex_depth;
    SDL_GPUTexture *tex_msaa;
    SDL_GPUTexture *tex_resolve;
    U32 draw_width, draw_height;

    // pipeline, sample settings
    SDL_GPUGraphicsPipeline *model_pipeline;
    SDL_GPUGraphicsPipeline *wall_pipeline;
    SDL_GPUSampleCount sample_count;

    // resources
    Gpu_ModelBuffers models[RdrModel_COUNT];
    SDL_GPUBuffer *wall_vert_buf;
    SDL_GPUSampler *wall_sampler;
    SDL_GPUTexture *tex_wall;

    SDL_PropertiesID clear_depth_props;
    SDL_PropertiesID clear_color_props;
} Gpu_State;
