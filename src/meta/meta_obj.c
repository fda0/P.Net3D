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
        s = S8_Skip(s, 1);
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
            if (ByteIsWhite(s.str[0]) || s.str[0] == '/')
                break;

            if (s.str[0] == '.')
                has_dot = true;

            s = S8_Skip(s, 1);
        }
        res.kind = (has_dot ? M_ObjToken_Float : M_ObjToken_Int);
    }

    res.text = S8_Range(start_src.str, s.str); // get final text range
    p->at += res.text.size; // adv parser

    M_LogObjToken(M_LogObjDebug, res);
    return res;
}

static bool M_IsNumberObjTokenKind(M_ObjTokenKind kind, bool allow_float)
{
    if (kind == M_ObjToken_Int) return true;
    if (allow_float && kind == M_ObjToken_Float) return true;
    return false;
}

static M_ObjFacePart M_ParseObjFaceTokens(M_ObjParser *p)
{
    M_ObjFacePart res = {};
    M_ObjParser p_copy = *p;

    M_ObjToken token = M_ParseObjGetToken(p);
    // abort if token is not a number
    if (!M_IsNumberObjTokenKind(token.kind, false))
    {
        *p = p_copy;
        return res;
    }

    res.pos = M_ParseInt(token.text);

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

static U16 M_FindOrInsertRdrModelVertex(Rdr_ModelVertex rdr_vertex)
{
    //U64 hash = HashU64(0, &rdr_vertex, sizeof(rdr_vertex));
    // @todo this is n^2, use hash table instead

    ForArray(i, M.vertex_table)
    {
        if (!M.vertex_table[i].filled)
        {
            M.vertex_table[i].filled = true;
            M.vertex_table[i].rdr = rdr_vertex;
            return (U16)i;
        }

        if (Memeq(&rdr_vertex, &M.vertex_table[i].rdr, sizeof(rdr_vertex)))
        {
            return (U16)i;
        }
    }

    M_AssertAlways(false);
    return 0;
}

static void M_ParseObj(const char *path, Printer *out, M_ModelSpec spec)
{
    ArenaScope tmp_scope = Arena_PushScope(M.tmp);

    // Clear global memory
    SDL_zeroa(M.vertex_table);

    // Prepare tmp memory
    U32 max_elems = 1024*1024;
    S8 active_material = {};

    float *obj_verts = AllocZeroed(M.tmp, float, max_elems);
    float *obj_normals = AllocZeroed(M.tmp, float, max_elems);
    M_ObjFacePart *obj_parts = AllocZeroed(M.tmp, M_ObjFacePart, max_elems);
    U64 obj_vert_count = 0;
    U64 obj_normal_count = 0;
    U64 obj_part_count = 0;

    U16 *out_inds = AllocZeroed(M.tmp, U16, max_elems);
    U64 out_ind_count = 0;

    // Prepare transformation matrices
    Mat4 rotation_mat = Mat4_Rotation_RH((V3){1,0,0}, spec.rot_x);
    rotation_mat = Mat4_Mul(Mat4_Rotation_RH((V3){0,1,0}, spec.rot_y), rotation_mat);
    rotation_mat = Mat4_Mul(Mat4_Rotation_RH((V3){0,0,1}, spec.rot_z), rotation_mat);

    Mat4 vert_mat = Mat4_Diagonal(1.f);
    if (spec.scale)
    {
        vert_mat = Mat4_Mul(Mat4_ScaleF(spec.scale), vert_mat);
    }
    vert_mat = Mat4_Mul(rotation_mat, vert_mat);

    // Load .obj file
    S8 model_name = S8_ScanCstr(path);
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
            M_ObjToken mat_name = M_ParseObjGetToken(&parser);
            if (mat_name.kind != M_ObjToken_String)
            {
                M_LOG(M_LogErr, "[OBJ PARSE] expected String token when parsin material name, got:");
                M_LogObjToken(M_LogErr, mat_name);
                M_LogObjParser(M_LogErr, &parser);
                exit(1);
            }
            active_material = mat_name.text;
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
                M_AssertAlways(obj_vert_count + 3 <= max_elems);
                M_AssertAlways(obj_normal_count + 3 <= max_elems);
                V4 vec =
                {
                    (float)M_ParseDouble(num0.text),
                    (float)M_ParseDouble(num1.text),
                    (float)M_ParseDouble(num2.text),
                    1.f
                };

                if (is_vert)
                {
                    vec = V4_MulM4(vert_mat, vec);

                    obj_verts[obj_vert_count + 0] = vec.x;
                    obj_verts[obj_vert_count + 1] = vec.y;
                    obj_verts[obj_vert_count + 2] = vec.z;
                    obj_vert_count += 3;
                }

                if (is_normal)
                {
                    vec = V4_MulM4(rotation_mat, vec);

                    obj_normals[obj_normal_count + 0] = vec.x;
                    obj_normals[obj_normal_count + 1] = vec.y;
                    obj_normals[obj_normal_count + 2] = vec.z;
                    obj_normal_count += 3;
                }
            }
        }

        if (is_ind)
        {
            M_ObjFacePart face_parts[8] = {};
            U32 face_part_count = 0;

            for (;;)
            {
                M_ObjFacePart part = M_ParseObjFaceTokens(&parser);
                if (!part.pos)
                    break;

                part.material = active_material;

                if (face_part_count >= ArrayCount(face_parts))
                {
                    M_LOG(M_LogErr, "[OBJ PARSE] face_count overflow! face_count == %u.", face_part_count);
                    M_LogObjParser(M_LogErr, &parser);
                    exit(1);
                }

                face_parts[face_part_count] = part;
                face_part_count += 1;
            }

            if (face_part_count < 3)
            {
                M_LOG(M_LogErr, "[OBJ PARSE] face_count underflow! "
                      "3 faces are needed at minimum. face_count == %u.", face_part_count);
                M_LogObjParser(M_LogErr, &parser);
                exit(1);
            }

            U32 triangle_count = face_part_count - 2;
            ForU32(triangle_i, triangle_count)
            {
                M_AssertAlways(obj_part_count + 3 <= max_elems);
                obj_parts[obj_part_count + 0] = face_parts[0];
                obj_parts[obj_part_count + 1] = face_parts[triangle_i + 1];
                obj_parts[obj_part_count + 2] = face_parts[triangle_i + 2];
                obj_part_count += 3;
            }
        }
    }

    //
    // Processing - generating additional data
    //

    // automatically calculate normals if they are missing from .obj file
    if (!obj_normal_count)
    {
        for (U64 part_index = 0;
             part_index + 3 <= obj_part_count;
             part_index += 3)
        {
            U32 pos0_base = (obj_parts[part_index + 0].pos - 1) * 3;
            U32 pos1_base = (obj_parts[part_index + 1].pos - 1) * 3;
            U32 pos2_base = (obj_parts[part_index + 2].pos - 1) * 3;
            M_AssertAlways(pos0_base + 3 <= obj_vert_count);
            M_AssertAlways(pos1_base + 3 <= obj_vert_count);
            M_AssertAlways(pos2_base + 3 <= obj_vert_count);

            V3 vert0 = {obj_verts[pos0_base], obj_verts[pos0_base + 1], obj_verts[pos0_base + 2]};
            V3 vert1 = {obj_verts[pos1_base], obj_verts[pos1_base + 1], obj_verts[pos1_base + 2]};
            V3 vert2 = {obj_verts[pos2_base], obj_verts[pos2_base + 1], obj_verts[pos2_base + 2]};

            V3 u = V3_Sub(vert1, vert0);
            V3 v = V3_Sub(vert2, vert0);

            V3 normal =
            {
                // formulas from: https://www.khronos.org/opengl/wiki/Calculating_a_Surface_Normal#Pseudo-code
                u.y*v.z - u.z*v.y,
                u.z*v.x - u.x*v.z,
                u.x*v.y - u.y*v.x,
            };

            obj_normals[pos0_base] += normal.x;
            obj_normals[pos0_base + 1] += normal.y;
            obj_normals[pos0_base + 2] += normal.z;

            obj_normals[pos1_base] += normal.x;
            obj_normals[pos1_base + 1] += normal.y;
            obj_normals[pos1_base + 2] += normal.z;

            obj_normals[pos2_base] += normal.x;
            obj_normals[pos2_base + 1] += normal.y;
            obj_normals[pos2_base + 2] += normal.z;

            obj_parts[part_index + 0].nrm = obj_parts[part_index + 0].pos;
            obj_parts[part_index + 1].nrm = obj_parts[part_index + 1].pos;
            obj_parts[part_index + 2].nrm = obj_parts[part_index + 2].pos;

            obj_normal_count = max_elems;
        }
    }

    // Generate final Rdr_ModelVertex values
    for (U64 obj_part_index = 0;
         obj_part_index + 3 <= obj_part_count;
         obj_part_index += 3)
    {
        M_ObjFacePart parts[3] =
        {
            obj_parts[obj_part_index + 0],
            obj_parts[obj_part_index + 1],
            obj_parts[obj_part_index + 2],
        };

        ForArray(i, parts)
        {
            bool invalid = false;
            invalid = invalid || parts[i].pos < 1;
            invalid = invalid || (parts[i].pos & 0xffff) != parts[i].pos;
            invalid = invalid || parts[i].pos >= obj_vert_count;
            if (parts[i].nrm)
            {
                invalid = invalid || (parts[i].nrm & 0xffff) != parts[i].nrm;
                invalid = invalid || parts[i].nrm >= obj_normal_count;
            }

            if (invalid)
            {
                M_LOG(M_LogErr, "[OBJ PARSE] invalid parts[%u].pos: %u, .nrm: %u; for obj_part_index %u",
                      (U32)i, (U32)parts[i].pos, (U32)parts[i].nrm, (U32)obj_part_index);
                M_LogObjParser(M_LogErr, &parser);
                exit(1);
            }
        }

        ForArray(part_i, parts)
        {
            Rdr_ModelVertex rdr_vertex = {};

            U32 pos_offset = (parts[part_i].pos - 1) * 3;
            rdr_vertex.p = (V3)
            {
                obj_verts[pos_offset + 0],
                obj_verts[pos_offset + 1],
                obj_verts[pos_offset + 2],
            };

            if (parts[part_i].nrm)
            {
                U32 nrm_offset = (parts[part_i].nrm - 1) * 3;
                rdr_vertex.normal = (V3)
                {
                    obj_normals[nrm_offset + 0],
                    obj_normals[nrm_offset + 1],
                    obj_normals[nrm_offset + 2],
                };
            }

            S8 material = parts[part_i].material;
            rdr_vertex.color = (V3){1,1,1};
            if (S8_Match(material, S8Lit(""), 0))
            {
            }
            else if (S8_Match(material, S8Lit("Brown"), 0))
            {
                rdr_vertex.color = (V3){0.5f, 0.32f, 0.22f};
                rdr_vertex.color = (V3){1,1,1};
            }
            else if (S8_Match(material, S8Lit("LightRed"), 0))
            {
                rdr_vertex.color = (V3){1.f, 0.15f, 0.08f};
            }
            else
            {
                M_LOG(M_LogErr, "[OBJ PARSE] Warning: material with name \"%.*s\" is unsupported",
                      S8Print(material));
            }

            U16 ind = M_FindOrInsertRdrModelVertex(rdr_vertex);
            M_AssertAlways(out_ind_count + 3 <= max_elems);
            out_inds[out_ind_count] = ind;
            out_ind_count += 1;
        }
    }

    //
    // Output - generating C header
    //
    Pr_AddCstr(out, "// Model: "); Pr_Add(out, model_name); Pr_AddCstr(out, "\n");
    Pr_AddCstr(out, "static Rdr_ModelVertex Model_"); Pr_Add(out, model_name); Pr_AddCstr(out, "_vrt[] =\n{\n");
    ForArray(i, M.vertex_table)
    {
        if (!M.vertex_table[i].filled)
            break;

        Rdr_ModelVertex rdr = M.vertex_table[i].rdr;

        Pr_AddCstr(out, "  ");
        // vert
        Pr_AddFloat(out, rdr.p.x); Pr_AddCstr(out, "f, ");
        Pr_AddFloat(out, rdr.p.y); Pr_AddCstr(out, "f, ");
        Pr_AddFloat(out, rdr.p.z); Pr_AddCstr(out, "f, ");

        // color
        Pr_Add(out, S8Lit("/*col*/"));
        Pr_AddFloat(out, rdr.color.x); Pr_AddCstr(out, "f, ");
        Pr_AddFloat(out, rdr.color.y); Pr_AddCstr(out, "f, ");
        Pr_AddFloat(out, rdr.color.z); Pr_AddCstr(out, "f, ");

        // normals
        Pr_Add(out, S8Lit("/*nrm*/"));
        rdr.normal = V3_Normalize(rdr.normal);
        Pr_AddFloat(out, rdr.normal.x); Pr_AddCstr(out, "f, ");
        Pr_AddFloat(out, rdr.normal.y); Pr_AddCstr(out, "f, ");
        Pr_AddFloat(out, rdr.normal.z); Pr_AddCstr(out, "f,\n");
    }
    Pr_AddCstr(out, "};\n\n");

    Pr_AddCstr(out, "static U16 Model_"); Pr_Add(out, model_name); Pr_AddCstr(out, "_ind[] =\n{\n");
    for (U64 i = 0;
         i + 3 <= out_ind_count;
         i += 3)
    {
        Pr_AddCstr(out, "  ");
        // data
        Pr_AddU16(out, out_inds[i    ]); Pr_AddCstr(out, ", ");
        Pr_AddU16(out, out_inds[i + 1]); Pr_AddCstr(out, ", ");
        Pr_AddU16(out, out_inds[i + 2]); Pr_AddCstr(out, ",\n");
    }
    Pr_AddCstr(out, "};\n\n");

    // Log errors, cleanup
    if (out->err)
    {
        M_LOG(M_LogErr, "[OBJ PARSE] Printer out err: %u", out->err);
        exit(1);
    }

    Arena_PopScope(tmp_scope);
}
