typedef struct
{
    Rdr_ModelVertex rdr;
    bool filled;
} M_VertexEntry;

typedef struct
{
    U32 log_filter;
    Arena *tmp;

    M_VertexEntry vertex_table[1024*1024];
} Meta_State;
static Meta_State M;

enum M_LogType
{
    M_LogIdk = (1 << 0),
    M_LogErr = (1 << 1),
    M_LogObjDebug = (1 << 2),
};

// logging enable/disable
#if 1
#define M_LOG(FLAGS, ...) do{ if(((FLAGS) & M.log_filter) == (FLAGS)){ SDL_Log("[META] " __VA_ARGS__); }}while(0)
#else
#define M_LOG(FLAGS, ...) do{ (void)(FLAGS); if(0){ printf("[META] " __VA_ARGS__); }}while(0)
#endif

#define M_AssertAlways(...) Assert(__VA_ARGS__)

static S8 M_LoadFile(const char *file_path, bool exit_on_err)
{
    U64 size = 0;
    void *data = SDL_LoadFile(file_path, &size);
    if (exit_on_err && !data)
    {
        M_LOG(M_LogErr, "Failed to load file %s", file_path);
        exit(1);
    }

    S8 result =
    {
        (U8 *)data,
        size
    };
    return result;
}

static void M_SaveFile(const char *file_path, S8 data)
{
    bool success = SDL_SaveFile(file_path, data.str, data.size);
    if (!success)
    {
        M_LOG(M_LogErr, "Failed to save to file %s", file_path);
        exit(1);
    }
}
