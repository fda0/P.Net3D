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

static void M_Pr_AddObjTokenNumber(Printer *pr, M_ObjToken t)
{
    Pr_Add(pr, t.text);
    if (t.kind == M_ObjToken_Float)
        Pr_Add(pr, S8Lit("f"));
}

static void M_ParseObj(const char *path, Printer *out)
{
    U64 tmp_used = M.tmp->used; // save tmp arena used

    S8 model_name = S8_MakeScanCstr(path);
    {
        S8_FindResult slash = S8_Find(model_name, S8Lit("/"), 0, S8Match_FindLast|S8Match_SlashInsensitive);
        if (slash.found)
        {
            model_name = S8_Skip(model_name, slash.index + 1);
        }
        S8_FindResult dot = S8_Find(model_name, S8Lit("."), 0, 0);
        if (dot.found)
        {
            model_name = S8_Prefix(model_name, dot.index);
        }
    }

    Printer pr_vrt = Pr_Alloc(M.tmp, Megabyte(1));
    Printer pr_ind = Pr_Alloc(M.tmp, Megabyte(1));

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

            Printer *pr = (is_vrt ? &pr_vrt : &pr_ind);
            Pr_Add(pr, S8Lit("  "));
            M_Pr_AddObjTokenNumber(pr, num0);
            Pr_Add(pr, S8Lit(", "));
            M_Pr_AddObjTokenNumber(pr, num1);
            Pr_Add(pr, S8Lit(", "));
            M_Pr_AddObjTokenNumber(pr, num2);
            Pr_Add(pr, S8Lit(","));
            if (is_vrt)
            {
                Pr_Add(pr, S8Lit(" /* colors: */ "));
                Pr_Add(pr, S8Lit("0.9f, 0.2f, "));

                S8 values[] =
                {
                    S8Lit("1.f,"),
                    S8Lit("0.9f,"),
                    S8Lit("0.8f,"),
                    S8Lit("0.7f,"),
                    S8Lit("0.6f,"),
                    S8Lit("0.5f,"),
                    S8Lit("0.4f,"),
                    S8Lit("0.3f,"),
                    S8Lit("0.2f,"),
                    S8Lit("0.1f,"),
                    S8Lit("0.0f,"),
                };
                S8 value = values[M.debug_color_index++ % ArrayCount(values)];
                Pr_Add(pr, value);
            }
            Pr_Add(pr, S8Lit("\n"));
        }
        else
        {
            M_LOG(M_LogErr, "[OBJ PARSE] unsupported LineType: %.*s", S8Print(line_type.text));
            M_LogObjParser(M_LogErr, &parser);
            exit(1);
        }
    }

    Pr_AddCstr(out, "// Model: "); Pr_Add(out, model_name); Pr_AddCstr(out, "\n");
    Pr_AddCstr(out, "static Rdr_Vertex Model_"); Pr_Add(out, model_name); Pr_AddCstr(out, "_vrt[] =\n{\n");
    Pr_AddPrinter(out, &pr_vrt);
    Pr_AddCstr(out, "};\n\n");
    Pr_AddCstr(out, "static U16 Model_"); Pr_Add(out, model_name); Pr_AddCstr(out, "_ind[] =\n{\n");
    Pr_AddPrinter(out, &pr_ind);
    Pr_AddCstr(out, "};\n\n");

    if (pr_vrt.err || pr_ind.err || out->err)
    {
        M_LOG(M_LogErr, "[OBJ PARSE] Printer vrt err: %u, Printer ind err: %u, Printer out err: %u",
              pr_vrt.err, pr_ind.err, out->err);
        exit(1);
    }

    Arena_Reset(M.tmp, tmp_used); // restore tmp arena used
}
