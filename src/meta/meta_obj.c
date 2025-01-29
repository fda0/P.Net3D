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

static void M_ParseEatWhite(M_ObjParser *p)
{
    while (p->at < p->src.size && ByteIsWhite(p->src.str[p->at]))
    {
        p->at += 1;
    }
}

static void M_ParseEatWhiteAndComments(M_ObjParser *p)
{
    M_ParseEatWhite(p);
    while (p->at < p->src.size && p->src.str[p->at] == '#')
    {
        while (p->at < p->src.size)
        {
            bool was_new_line = p->src.str[p->at] == '\n';
            p->at += 1;
            if (was_new_line)
                break;
        }

        M_ParseEatWhite(p);
    }
}

static M_ObjToken M_ParseObjGetToken(M_ObjParser *p)
{
    M_ParseEatWhiteAndComments(p);

    S8 s = S8_Skip(p->src, p->at);
    S8 start_src = s;

    M_ObjToken res = {};
    if (!s.size)
        return res;

    if (s.str[0] == '/')
    {
        res.kind = M_ObjToken_Slash;
    }
    else if (ByteIsAlpha(s.str[0]))
    {
        res.kind = M_ObjToken_String;
        while (s.size && ByteIsAlpha(s.str[0]))
        {
            s = S8_Skip(s, 1);
        }
    }
    else if (ByteIsDigit(s.str[0]) || s.str[0] == '-')
    {
        bool has_dot = false;
        while (s.size)
        {
            if (ByteIsWhite(s.str[0]))
                break;

            if (s.str[0] == '.')
                has_dot = true;

            s = S8_Skip(s, 1);
        }
        res.kind = (has_dot ? M_ObjToken_Float : M_ObjToken_Int);
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

static M_ObjFace M_ParseObjFaceTokens(M_ObjParser *p)
{
    M_ObjFace res = {};
    M_ObjParser p_copy = *p;

    M_ObjToken token = M_ParseObjGetToken(p);
    // abort if token is not a number
    if (!M_IsNumberObjTokenKind(token.kind, false))
    {
        *p = p_copy;
        return res;
    }

    res.ind = M_ParseInt(token.text);

    p_copy = *p;
    token = M_ParseObjGetToken(p);
    // exit early if token is not a slash
    if (token.kind != M_ObjToken_Slash)
    {
        *p = p_copy;
        return res;
    }

    token = M_ParseObjGetToken(p);
    if (token.kind != M_ObjToken_Slash)
    {
        if (!M_IsNumberObjTokenKind(token.kind, false))
        {
            M_LOG(M_LogErr, "[OBJ PARSE] expected second face number, got:");
            M_LogObjToken(M_LogErr, token);
            M_LogObjParser(M_LogErr, p);
            exit(1);
        }

        res.tex = M_ParseInt(token.text);

        token = M_ParseObjGetToken(p);
        if (token.kind != M_ObjToken_Slash)
        {
            M_LOG(M_LogErr, "[OBJ PARSE] expected second slash, got:");
            M_LogObjToken(M_LogErr, token);
            M_LogObjParser(M_LogErr, p);
            exit(1);
        }
    }

    token = M_ParseObjGetToken(p);
    if (!M_IsNumberObjTokenKind(token.kind, false))
    {
        M_LOG(M_LogErr, "[OBJ PARSE] expected third face number, got:");
        M_LogObjToken(M_LogErr, token);
        M_LogObjParser(M_LogErr, p);
        exit(1);
    }

    res.nrm = M_ParseInt(token.text);
    return res;
}

static void M_ParseObj(const char *path, Printer *out, M_ModelSpec spec)
{
    // Prepare transformation matrices
    Mat4 rotation_mat = Mat4_Rotation_RH(spec.rot_x, (V3){1,0,0});
    rotation_mat = Mat4_Mul(Mat4_Rotation_RH(spec.rot_y, (V3){0,1,0}), rotation_mat);
    rotation_mat = Mat4_Mul(Mat4_Rotation_RH(spec.rot_z, (V3){0,0,1}), rotation_mat);

    Mat4 vert_mat = Mat4_Diagonal(1.f);
    if (spec.scale)
    {
        vert_mat = Mat4_Mul(Mat4_ScaleF(spec.scale), vert_mat);
    }
    vert_mat = Mat4_Mul(rotation_mat, vert_mat);

    // Load .obj file
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

    // Prepare tmp memory
    SDL_zeroa(M.verts);
    SDL_zeroa(M.inds);
    SDL_zeroa(M.normals);
    SDL_zeroa(M.materials);
    U64 vert_count = 0;
    U64 normal_count = 0;
    U64 ind_count = 0;
    U64 material_count = 0;

    //
    // Input - parsing .obj file
    //
    M_ObjParser parser = M_LoadObjFile(path);
    ForU64(timeout, 1024*1024)
    {
        M_ObjToken line_type = M_ParseObjGetToken(&parser);
        if (line_type.kind == M_ObjToken_EOF)
            break;

        bool is_vert = S8_Match(line_type.text, S8Lit("v"), 0);
        bool is_normal = S8_Match(line_type.text, S8Lit("vn"), 0);
        bool is_ind = S8_Match(line_type.text, S8Lit("f"), 0);
        bool is_material = S8_Match(line_type.text, S8Lit("usemtl"), 0);
        bool is_smoothing_group = S8_Match(line_type.text, S8Lit("s"), 0);

        if (!is_vert && !is_ind && !is_normal && !is_material && !is_smoothing_group)
        {
            M_LOG(M_LogErr, "[OBJ PARSE] unsupported line type: %.*s", S8Print(line_type.text));
            M_LogObjParser(M_LogErr, &parser);
            exit(1);
        }

        if (is_smoothing_group)
        {
            // idk what smoothing group is
            M_ObjToken num = M_ParseObjGetToken(&parser);
            if (!M_IsNumberObjTokenKind(num.kind, false))
            {
                M_LOG(M_LogErr, "[OBJ PARSE] Expected number in smoothing group, got:");
                M_LogObjToken(M_LogErr, num);
                exit(1);
            }
        }

        if (is_material)
        {
            M_ObjToken mat = M_ParseObjGetToken(&parser);
            if (mat.kind != M_ObjToken_String)
            {
                M_LOG(M_LogErr, "[OBJ PARSE] expected String token when parsin material name, got:");
                M_LogObjToken(M_LogErr, mat);
                M_LogObjParser(M_LogErr, &parser);
                exit(1);
            }

            material_count += 1;
            M_AssertAlways(material_count < ArrayCount(M.materials));

            // init material
            M.materials[material_count].name = mat.text;
            M.materials[material_count].index_range.min = ind_count;
            M.materials[material_count].index_range.max = ind_count;
        }

        if (is_vert || is_normal)
        {
            M_ObjToken num0 = M_ParseObjGetToken(&parser);
            M_ObjToken num1 = M_ParseObjGetToken(&parser);
            M_ObjToken num2 = M_ParseObjGetToken(&parser);

            if (!M_IsNumberObjTokenKind(num0.kind, true) ||
                !M_IsNumberObjTokenKind(num1.kind, true) ||
                !M_IsNumberObjTokenKind(num2.kind, true))
            {
                M_LOG(M_LogErr, "[OBJ PARSE] expected 3 number tokens. "
                      "At least one of these is an invalid number:");
                M_LogObjToken(M_LogErr, num0);
                M_LogObjToken(M_LogErr, num1);
                M_LogObjToken(M_LogErr, num2);
                exit(1);
            }

            if (is_vert || is_normal)
            {
                M_AssertAlways(vert_count + 3 <= ArrayCount(M.verts));
                M_AssertAlways(normal_count + 3 <= ArrayCount(M.normals));
                double n0 = M_ParseDouble(num0.text);
                double n1 = M_ParseDouble(num1.text);
                double n2 = M_ParseDouble(num2.text);
                V4 vec = {(float)n0, (float)n1, (float)n2, 1.f};

                if (is_vert)
                {
                    vec = V4_MulM4(vert_mat, vec);

                    M.verts[vert_count + 0] = vec.x;
                    M.verts[vert_count + 1] = vec.y;
                    M.verts[vert_count + 2] = vec.z;
                    vert_count += 3;
                }

                if (is_normal)
                {
                    vec = V4_MulM4(rotation_mat, vec);

                    M.normals[normal_count + 0] = vec.x;
                    M.normals[normal_count + 1] = vec.y;
                    M.normals[normal_count + 2] = vec.z;
                    normal_count += 3;
                }
            }
        }

        if (is_ind)
        {
            M_ObjFace faces[8] = {};
            U32 face_count = 0;

            for (;;)
            {
                M_ObjFace face = M_ParseObjFaceTokens(&parser);
                if (!face.ind)
                    break;

                if (face_count >= ArrayCount(faces))
                {
                    M_LOG(M_LogErr, "[OBJ PARSE] face_count overflow! face_count == %u.", face_count);
                    M_LogObjParser(M_LogErr, &parser);
                    exit(1);
                }

                faces[face_count] = face;
                face_count += 1;
            }

            if (face_count < 3)
            {
                M_LOG(M_LogErr, "[OBJ PARSE] face_count underflow! "
                      "3 faces are needed at minimum. face_count == %u.", face_count);
                M_LogObjParser(M_LogErr, &parser);
                exit(1);
            }

            U32 triangle_count = face_count - 2;

            ForU32(triangle_i, triangle_count)
            {
                M_ObjFace f0 = faces[0];
                M_ObjFace f1 = faces[triangle_i + 1];
                M_ObjFace f2 = faces[triangle_i + 2];

                M_AssertAlways(ind_count + 3 <= ArrayCount(M.inds));
                M_AssertAlways(f0.ind >= 1 && (f0.ind & 0xffff) == f0.ind);
                M_AssertAlways(f1.ind >= 1 && (f1.ind & 0xffff) == f1.ind);
                M_AssertAlways(f2.ind >= 1 && (f2.ind & 0xffff) == f2.ind);
                M.inds[ind_count + 0] = (U16)(f0.ind - 1);
                M.inds[ind_count + 1] = (U16)(f1.ind - 1);
                M.inds[ind_count + 2] = (U16)(f2.ind - 1);
                ind_count += 3;

                // advance current's material max to contain these indices
                M.materials[material_count].index_range.max += 3;
            }
        }
    }

    //
    // Processing - generating additional data
    //

    // material_count was used as index; advance it by one
    material_count += 1;

    // automatically calculate normals if they are missing from .obj file
    if (!normal_count)
    {
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
    }

    //
    // Output - generating C header
    //
    Pr_AddCstr(out, "// Model: "); Pr_Add(out, model_name); Pr_AddCstr(out, "\n");
    Pr_AddCstr(out, "static Rdr_ModelVertex Model_"); Pr_Add(out, model_name); Pr_AddCstr(out, "_vrt[] =\n{\n");
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

    // Log errors, cleanup
    if (out->err)
    {
        M_LOG(M_LogErr, "[OBJ PARSE] Printer out err: %u", out->err);
        exit(1);
    }
}
