typedef struct
{
    SDL_GPUDevice *device;

    SDL_GPUShader *vertex;
    SDL_GPUShader *fragment;

    SDL_GPUBuffer *buf_vertex;
} Gpu_State;


typedef struct VertexData
{
    float x, y, z; /* 3D data. Vertex range -0.5..0.5 in all axes. Z -0.5 is near, 0.5 is far. */
    float red, green, blue;  /* intensity 0 to 1 (alpha is always 1). */
} VertexData;

static const VertexData g_vertex_data[] =
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
