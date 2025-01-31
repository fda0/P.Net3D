typedef struct
{
    SDL_GPUTexture *tex_depth, *tex_msaa, *tex_resolve;
    U32 draw_width, draw_height;
} Gpu_WindowState;

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

    Gpu_ModelBuffers models[RdrModel_COUNT];

    SDL_GPUBuffer *wall_vert_buf;
    SDL_GPUBuffer *wall_instance_buf;
    SDL_GPUSampler *wall_sampler;
    SDL_GPUTexture *tex_wall;

    SDL_GPUGraphicsPipeline *model_pipeline;
    SDL_GPUGraphicsPipeline *wall_pipeline;

    SDL_GPUSampleCount sample_count;
    Gpu_WindowState win_state;
} Gpu_State;
