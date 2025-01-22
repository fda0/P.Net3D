typedef struct
{
    float scale; // set to 1 if equal to 0
    float rot_x, rot_y, rot_z;
} M_ModelSpec;

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

static void M_ParseObj(const char *path, Printer *out, M_ModelSpec spec)
{
    U64 tmp_used = M.tmp->used; // save tmp arena used


    Mat4 transform_mat = Mat4_Diagonal(1.f);
    if (spec.scale)
    {
        transform_mat = Mat4_Mul(Mat4_ScaleF(spec.scale), transform_mat);
    }
    transform_mat = Mat4_Mul(Mat4_Rotation_RH(spec.rot_x, (V3){1,0,0}), transform_mat);
    transform_mat = Mat4_Mul(Mat4_Rotation_RH(spec.rot_y, (V3){0,1,0}), transform_mat);
    transform_mat = Mat4_Mul(Mat4_Rotation_RH(spec.rot_z, (V3){0,0,1}), transform_mat);


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

    SDL_zeroa(M.verts);
    SDL_zeroa(M.inds);
    SDL_zeroa(M.normals);
    U64 vert_count = 0;
    U64 ind_count = 0;

    //Printer pr_vrt = Pr_Alloc(M.tmp, Megabyte(1));
    //Printer pr_ind = Pr_Alloc(M.tmp, Megabyte(1));

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

            if (is_vrt)
            {
                M_AssertAlways(vert_count + 3 <= ArrayCount(M.verts));
                double n0 = M_ParseDouble(num0.text);
                double n1 = M_ParseDouble(num1.text);
                double n2 = M_ParseDouble(num2.text);

                V4 vec = {(float)n0, (float)n1, (float)n2, 1.f};
                vec = V4_MulM4(transform_mat, vec);

                M.verts[vert_count + 0] = vec.x;
                M.verts[vert_count + 1] = vec.y;
                M.verts[vert_count + 2] = vec.z;
                vert_count += 3;
            }
            else
            {
                M_AssertAlways(ind_count + 3 <= ArrayCount(M.inds));
                int n0 = M_ParseInt(num0.text);
                int n1 = M_ParseInt(num1.text);
                int n2 = M_ParseInt(num2.text);
                M_AssertAlways(n0 >= 1 && (n0 & 0xffff) == n0);
                M_AssertAlways(n1 >= 1 && (n1 & 0xffff) == n1);
                M_AssertAlways(n2 >= 1 && (n2 & 0xffff) == n2);
                M.inds[ind_count + 0] = (U16)(n0 - 1);
                M.inds[ind_count + 1] = (U16)(n1 - 1);
                M.inds[ind_count + 2] = (U16)(n2 - 1);
                ind_count += 3;
            }
        }
        else
        {
            M_LOG(M_LogErr, "[OBJ PARSE] unsupported LineType: %.*s", S8Print(line_type.text));
            M_LogObjParser(M_LogErr, &parser);
            exit(1);
        }
    }

    // calculate normals
    for (U64 i = 0;
         i + 3 <= ind_count;
         i += 3)
    {
        I32 i0 = M.inds[i];
        I32 i1 = M.inds[i + 1];
        I32 i2 = M.inds[i + 2];
        I32 vi0 = i0 * 3;
        I32 vi1 = i1 * 3;
        I32 vi2 = i2 * 3;
        M_AssertAlways(vi0 + 3 <= vert_count);
        M_AssertAlways(vi1 + 3 <= vert_count);
        M_AssertAlways(vi2 + 3 <= vert_count);

        V3 vert0 = {M.verts[vi0], M.verts[vi0 + 1], M.verts[vi0 + 2]};
        V3 vert1 = {M.verts[vi1], M.verts[vi1 + 1], M.verts[vi1 + 2]};
        V3 vert2 = {M.verts[vi2], M.verts[vi2 + 1], M.verts[vi2 + 2]};

        V3 u = V3_Sub(vert1, vert0);
        V3 v = V3_Sub(vert2, vert0);

        V3 normal =
        {
            // formulas from: https://www.khronos.org/opengl/wiki/Calculating_a_Surface_Normal#Pseudo-code
            u.y*v.z - u.z*v.y,
            u.z*v.x - u.x*v.z,
            u.x*v.y - u.y*v.x,
        };

        M.normals[vi0] += normal.x;
        M.normals[vi0 + 1] += normal.y;
        M.normals[vi0 + 2] += normal.z;

        M.normals[vi1] += normal.x;
        M.normals[vi1 + 1] += normal.y;
        M.normals[vi1 + 2] += normal.z;

        M.normals[vi2] += normal.x;
        M.normals[vi2 + 1] += normal.y;
        M.normals[vi2 + 2] += normal.z;
    }

    Pr_AddCstr(out, "// Model: "); Pr_Add(out, model_name); Pr_AddCstr(out, "\n");
    Pr_AddCstr(out, "static Rdr_Vertex Model_"); Pr_Add(out, model_name); Pr_AddCstr(out, "_vrt[] =\n{\n");
    for (U64 i = 0;
         i + 3 <= vert_count;
         i += 3)
    {
        Pr_AddCstr(out, "  ");
        // vert
        Pr_AddFloat(out, M.verts[i    ]); Pr_AddCstr(out, "f, ");
        Pr_AddFloat(out, M.verts[i + 1]); Pr_AddCstr(out, "f, ");
        Pr_AddFloat(out, M.verts[i + 2]); Pr_AddCstr(out, "f, ");

        // color
        Pr_Add(out, S8Lit("/*color*/1.f, 1.f, 1.f, "));

        // normals
        Pr_Add(out, S8Lit("/*normal*/"));
        V3 vert_normal = {M.normals[i], M.normals[i + 1], M.normals[i + 2]};
        vert_normal = V3_Normalize(vert_normal);
        Pr_AddFloat(out, vert_normal.x); Pr_AddCstr(out, "f, ");
        Pr_AddFloat(out, vert_normal.y); Pr_AddCstr(out, "f, ");
        Pr_AddFloat(out, vert_normal.z); Pr_AddCstr(out, "f,\n");
    }
    Pr_AddCstr(out, "};\n\n");

    Pr_AddCstr(out, "static U16 Model_"); Pr_Add(out, model_name); Pr_AddCstr(out, "_ind[] =\n{\n");
    for (U64 i = 0;
         i + 3 <= ind_count;
         i += 3)
    {
        Pr_AddCstr(out, "  ");
        // data
        Pr_AddU16(out, M.inds[i    ]); Pr_AddCstr(out, ", ");
        Pr_AddU16(out, M.inds[i + 1]); Pr_AddCstr(out, ", ");
        Pr_AddU16(out, M.inds[i + 2]); Pr_AddCstr(out, ",\n");
    }
    Pr_AddCstr(out, "};\n\n");

    if (out->err)
    {
        M_LOG(M_LogErr, "[OBJ PARSE] Printer out err: %u", out->err);
        exit(1);
    }

    Arena_Reset(M.tmp, tmp_used); // restore tmp arena used
}
