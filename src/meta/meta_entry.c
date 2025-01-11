#define SDL_ASSERT_LEVEL 2
#include <SDL3/SDL.h>
#include "stdio.h"
#include "base_types.h"
#include "base_string.h"
#include "base_arena.h"
#include "base_math.h"
#include "de_render_vertex.h"

typedef struct
{
    U32 log_filter;
} Meta_State;
static Meta_State M;

enum
{
    M_LogIdk       = (1 << 0),
    M_LogErr       = (1 << 1),
    M_LogObj       = (1 << 2),
};

// logging enable/disable
#if 1
    #define M_LOG(FLAGS, ...) do{ if(((FLAGS) & M.log_filter) == (FLAGS)){ SDL_Log("[META] " __VA_ARGS__); }}while(0)
#else
    #define M_LOG(FLAGS, ...) do{ (void)(FLAGS); if(0){ printf("[META] " __VA_ARGS__); }}while(0)
#endif

static S8 M_LoadFile(const char *file_path, bool exit_on_err)
{
    U64 size = 0;
    void *data = SDL_LoadFile(file_path, &size);
    if (exit_on_err && !data)
    {
        M_LOG(M_LogErr, "Failed to load file %s, exiting", file_path);
        exit(1);
    }

    S8 result =
    {
        (U8 *)data,
        size
    };
    return result;
}

typedef struct
{
    const char *path;
    S8 src;
    U64 at;
} M_ObjParser;

typedef enum
{
    M_ObjToken_EOF,
    M_ObjToken_LineType,
    M_ObjToken_Float,
    M_ObjToken_Int,
} M_ObjTokenKind;

typedef struct
{
    M_ObjTokenKind kind;
    S8 text;
} M_ObjToken;

static void M_LogObjParser(M_ObjParser *p)
{
    S8 s = S8_Skip(p->src, p->at);
    s = S8_Prefix(s, 8);

    M_LOG(M_LogObj, "[PARSER LOC] File: %s, at byte: %llu, "
          "snippet (starting from at byte): ```\n%.*s\n```",
          p->path, p->at,
          (int)s.size, s.str);
}

static M_ObjToken M_ParseObjGetToken(M_ObjParser *p)
{
    S8 s = S8_Skip(p->src, p->at);
    while (s.size && ByteIsWhite(s.str[0]))
    {
        s = S8_Skip(s, 1);
    }

    M_ObjToken res = {};
    if (!s.size)
        return res;

    if (ByteIsAlpha(s.str[0]))
    {
        res.kind = M_ObjToken_LineType;
        while (s.size && ByteIsAlpha(s.str[0]))
        {
            s = S8_Skip(s, 1);
        }

        if (!s.size || s.str[0] != ' ')
        {
            M_LOG(M_LogErr, "[OBJ PARSE] Expected ' ' space after LineType. ");
            M_LogObjParser(p);
            exit(1);
        }
    }
    else if (ByteIsDigit(s.str[0]) || s.str[0] == '-')
    {
    }
}

int main()
{
    // init
    M.log_filter = ~0u;

    // work
    {
        S8 teapot = M_LoadFile("../res/models/teapot.obj", true);


        (void)teapot;
    }

    // exit
    M_LOG(M_LogIdk, "Success");
    return 0;
}
