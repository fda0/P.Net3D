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

static M_ObjParser M_LoadObjFile(const char *path)
{
    S8 src = M_LoadFile(path, true);
    M_ObjParser p = {};
    p.path = path;
    p.src = src;
    return p;
}

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
            M_LOG(M_LogErr, "[OBJ PARSE] Expected ' ' space after LineType.");
            M_LogObjParser(p);
            exit(1);
        }
    }
    else if (ByteIsDigit(s.str[0]) || s.str[0] == '-')
    {
        bool has_dot = false;
        bool has_minus = (s.str[0] == '-');
        if (has_minus)
            s = S8_Skip(s, 1);

        bool pre_dot_number = false;
        bool post_dot_number = false;

        while (s.size)
        {
            if (ByteIsDigit(s.str[0]))
            {
                if (has_dot) post_dot_number = true;
                else         pre_dot_number = true;
            }
            else if (s.str[0] == '.')
            {
                has_dot = true;
            }
            else
            {
                if (!ByteIsWhite(s.str[0]))
                {
                    M_LOG(M_LogErr, "[OBJ PARSE] Expected white char after number.");
                    M_LogObjParser(p);
                    exit(1);
                }
                break;
            }

            s = S8_Skip(s, 1);
        }

        res.kind = (has_dot ? M_ObjToken_Float : M_ObjToken_Int);

        bool invalid = !pre_dot_number || (has_dot && !post_dot_number);
        if (invalid)
        {
            M_LOG(M_LogErr, "[OBJ PARSE] Parsed number is invalid.");
            M_LogObjParser(p);
            exit(1);
        }
    }

    res.text = S8_Range(p->src.str, s.str); // get final text range
    p->src = S8_Skip(p->src, res.text.size); // adv parser
    return res;
}

int main()
{
    // init
    M.log_filter = ~0u;

    // work
    {
        M_ObjParser teapot = M_LoadObjFile("../res/models/teapot.obj");
        ForU64(timeout, 1024)
        {
            M_ObjToken t = M_ParseObjGetToken(&teapot);
            (void)t;
        }
    }

    // exit
    M_LOG(M_LogIdk, "Success");
    return 0;
}
