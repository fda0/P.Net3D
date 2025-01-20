typedef struct
{
    SDL_GPUTexture *tex_depth, *tex_msaa, *tex_resolve;
    U32 draw_width, draw_height;
} Gpu_WindowState;

typedef struct
{
    SDL_GPUDevice *device;
    SDL_GPUBuffer *model_vert_buf;
    SDL_GPUBuffer *model_indx_buf;
    SDL_GPUBuffer *model_instance_buf;

    SDL_GPUBuffer *wall_vert_buf;
    SDL_GPUBuffer *wall_indx_buf;
    SDL_GPUBuffer *wall_instance_buf;

    SDL_GPUSampleCount sample_count;
    SDL_GPUGraphicsPipeline *pipeline;

    Gpu_WindowState win_state;
} Gpu_State;

typedef struct VertexData
{
    float x, y, z; /* 3D data. Vertex range -0.5..0.5 in all axes. Z -0.5 is near, 0.5 is far. */
    float red, green, blue;  /* intensity 0 to 1 (alpha is always 1). */
} VertexData;

static VertexData g_vertex_data[] =
{
    /* Front face. */
    /* Bottom left */
    { -0.5,  0.5, -0.5, 1.0, 0.0, 0.0 }, /* red */
    {  0.5, -0.5, -0.5, 0.0, 0.0, 1.0 }, /* blue */
    { -0.5, -0.5, -0.5, 0.0, 1.0, 0.0 }, /* green */

    /* Top right */
    { -0.5, 0.5, -0.5, 1.0, 0.0, 0.0 }, /* red */
    { 0.5,  0.5, -0.5, 1.0, 1.0, 0.0 }, /* yellow */
    { 0.5, -0.5, -0.5, 0.0, 0.0, 1.0 }, /* blue */

    /* Left face */
    /* Bottom left */
    { -0.5,  0.5,  0.5, 1.0, 1.0, 1.0 }, /* white */
    { -0.5, -0.5, -0.5, 0.0, 1.0, 0.0 }, /* green */
    { -0.5, -0.5,  0.5, 0.0, 1.0, 1.0 }, /* cyan */

    /* Top right */
    { -0.5,  0.5,  0.5, 1.0, 1.0, 1.0 }, /* white */
    { -0.5,  0.5, -0.5, 1.0, 0.0, 0.0 }, /* red */
    { -0.5, -0.5, -0.5, 0.0, 1.0, 0.0 }, /* green */

    /* Top face */
    /* Bottom left */
    { -0.5, 0.5,  0.5, 1.0, 1.0, 1.0 }, /* white */
    {  0.5, 0.5, -0.5, 1.0, 1.0, 0.0 }, /* yellow */
    { -0.5, 0.5, -0.5, 1.0, 0.0, 0.0 }, /* red */

    /* Top right */
    { -0.5, 0.5,  0.5, 1.0, 1.0, 1.0 }, /* white */
    {  0.5, 0.5,  0.5, 0.0, 0.0, 0.0 }, /* black */
    {  0.5, 0.5, -0.5, 1.0, 1.0, 0.0 }, /* yellow */

    /* Right face */
    /* Bottom left */
    { 0.5,  0.5, -0.5, 1.0, 1.0, 0.0 }, /* yellow */
    { 0.5, -0.5,  0.5, 1.0, 0.0, 1.0 }, /* magenta */
    { 0.5, -0.5, -0.5, 0.0, 0.0, 1.0 }, /* blue */

    /* Top right */
    { 0.5,  0.5, -0.5, 1.0, 1.0, 0.0 }, /* yellow */
    { 0.5,  0.5,  0.5, 0.0, 0.0, 0.0 }, /* black */
    { 0.5, -0.5,  0.5, 1.0, 0.0, 1.0 }, /* magenta */

    /* Back face */
    /* Bottom left */
    {  0.5,  0.5, 0.5, 0.0, 0.0, 0.0 }, /* black */
    { -0.5, -0.5, 0.5, 0.0, 1.0, 1.0 }, /* cyan */
    {  0.5, -0.5, 0.5, 1.0, 0.0, 1.0 }, /* magenta */

    /* Top right */
    {  0.5,  0.5,  0.5, 0.0, 0.0, 0.0 }, /* black */
    { -0.5,  0.5,  0.5, 1.0, 1.0, 1.0 }, /* white */
    { -0.5, -0.5,  0.5, 0.0, 1.0, 1.0 }, /* cyan */

    /* Bottom face */
    /* Bottom left */
    { -0.5, -0.5, -0.5, 0.0, 1.0, 0.0 }, /* green */
    {  0.5, -0.5,  0.5, 1.0, 0.0, 1.0 }, /* magenta */
    { -0.5, -0.5,  0.5, 0.0, 1.0, 1.0 }, /* cyan */

    /* Top right */
    { -0.5, -0.5, -0.5, 0.0, 1.0, 0.0 }, /* green */
    {  0.5, -0.5, -0.5, 0.0, 0.0, 1.0 }, /* blue */
    {  0.5, -0.5,  0.5, 1.0, 0.0, 1.0 } /* magenta */
};



/*
 * Simulates desktop's glRotatef. The matrix is returned in column-major
 * order.
 */
static void
rotate_matrix(float turns, float x, float y, float z, float *r)
{
    SinCosResult sc = SinCosF(turns);
    float c1 = 1.0f - sc.cos;

    float length = (float)SDL_sqrt(x * x + y * y + z * z);

    float u[3];
    u[0] = x / length;
    u[1] = y / length;
    u[2] = z / length;

    for (int i = 0; i < 16; i++)
    {
        r[i] = 0.0;
    }

    r[15] = 1.0;

    for (int i = 0; i < 3; i++)
    {
        r[i * 4 + (i + 1) % 3] =  u[(i + 2) % 3] * sc.sin;
        r[i * 4 + (i + 2) % 3] = -u[(i + 1) % 3] * sc.sin;
    }

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            r[i * 4 + j] += c1 * u[i] * u[j] + (i == j ? sc.cos : 0.0f);
        }
    }
}

/*
 * Simulates gluPerspectiveMatrix
 */
static void
perspective_matrix(float fovy, float aspect, float znear, float zfar, float *r)
{
    memset(r, 0, sizeof(float)*16);
    float f = 1.0f/SDL_tanf(fovy * 0.5f);

    r[0] = f / aspect;
    r[5] = f;
    r[10] = (znear + zfar) / (znear - zfar);
    r[11] = -1.0f;
    r[14] = (2.0f * znear * zfar) / (znear - zfar);
    r[15] = 0.0f;
}

/*
 * Multiplies lhs by rhs and writes out to r. All matrices are 4x4 and column
 * major. In-place multiplication is supported.
 */
static void
multiply_matrix(float *lhs, float *rhs, float *r)
{
    int i, j, k;
    float tmp[16];

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            tmp[j * 4 + i] = 0.0;

            for (k = 0; k < 4; k++) {
                tmp[j * 4 + i] += lhs[k * 4 + i] * rhs[j * 4 + k];
            }
        }
    }

    for (i = 0; i < 16; i++) {
        r[i] = tmp[i];
    }
}



