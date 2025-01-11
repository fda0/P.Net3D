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

static void M_LogObjParser(U32 log_flags, M_ObjParser *p)
{
    S8 s = S8_Skip(p->src, p->at);
    s = S8_Prefix(s, 8);

    M_LOG(log_flags, "[OBJ PARSE] File: %s, at byte: %llu, "
          "snippet (starting from at byte):\n```\n%.*s\n```",
          p->path, p->at,
          (int)s.size, s.str);
}

static void M_LogObjToken(U32 log_flags, M_ObjToken t)
{
    M_LOG(log_flags, "[OBJ PARSE] Token kind: %d, Token text: ```%.*s```",
          t.kind, S8Print(t.text));
}

static M_ObjToken M_ParseObjGetToken(M_ObjParser *p)
{
    S8 s = S8_Skip(p->src, p->at);
    while (s.size && ByteIsWhite(s.str[0]))
    {
        s = S8_Skip(s, 1);
    }
    S8 start_src = s;

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
            M_LogObjParser(M_LogErr, p);
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
                    M_LogObjParser(M_LogErr, p);
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
            M_LogObjParser(M_LogErr, p);
            exit(1);
        }
    }

    res.text = S8_Range(start_src.str, s.str); // get final text range
    p->at += res.text.size + 1; // adv parser

    M_LogObjToken(M_LogObjDebug, res);
    return res;
}

static bool M_IsNumberObjTokenKind(M_ObjTokenKind kind, bool allow_float)
{
    if (kind == M_ObjToken_Int) return true;
    if (allow_float && kind == M_ObjToken_Float) return true;
    return false;
}

static void M_ParseObj(const char *path)
{
    U64 tmp_used = M.tmp->used; // save tmp arena used

    Printer pr_vrt = Pr_Alloc(M.tmp, Megabyte(1));
    Printer pr_ind = Pr_Alloc(M.tmp, Megabyte(1));
    (void)pr_vrt;
    (void)pr_ind;

    M_ObjParser parser = M_LoadObjFile(path);
    ForU64(timeout, 1024*1024)
    {
        M_ObjToken line_type = M_ParseObjGetToken(&parser);
        if (line_type.kind == M_ObjToken_EOF)
            break;

        if (line_type.kind != M_ObjToken_LineType)
        {
            M_LOG(M_LogErr, "[OBJ PARSE] expected LineType token, got:");
            M_LogObjToken(M_LogErr, line_type);
            M_LogObjParser(M_LogErr, &parser);
            exit(1);
        }

        bool is_vrt = S8_Match(line_type.text, S8Lit("v"), 0);
        bool is_ind = S8_Match(line_type.text, S8Lit("f"), 0);
        if (is_vrt || is_ind)
        {
            M_ObjToken num0 = M_ParseObjGetToken(&parser);
            M_ObjToken num1 = M_ParseObjGetToken(&parser);
            M_ObjToken num2 = M_ParseObjGetToken(&parser);
            if (!M_IsNumberObjTokenKind(num0.kind, is_vrt) ||
                !M_IsNumberObjTokenKind(num1.kind, is_vrt) ||
                !M_IsNumberObjTokenKind(num2.kind, is_vrt))
            {
                M_LOG(M_LogErr, "[OBJ PARSE] expected 3 number tokens (%s). "
                      "At least one of these is an invalid number:");
                M_LogObjToken(M_LogErr, num0);
                M_LogObjToken(M_LogErr, num1);
                M_LogObjToken(M_LogErr, num2);
            }

            (void)num0;
            (void)num1;
            (void)num2;
        }
        else
        {
            M_LOG(M_LogErr, "[OBJ PARSE] unsupported LineType: %.*s", S8Print(line_type.text));
            M_LogObjParser(M_LogErr, &parser);
            exit(1);
        }
    }

    Arena_Reset(M.tmp, tmp_used); // restore tmp arena used
}
