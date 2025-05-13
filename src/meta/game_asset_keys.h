static U64 MATERIAL_Hash(S8 name)
{
  U64 hash = S8_Hash(0, S8Lit("TEX_Key:"));
  hash = S8_Hash(hash, name);
  return hash;
}
static U64 MATERIAL_HashCstr(const char *name)
{
  return MATERIAL_Hash(S8_FromCstr(name));
}

typedef struct
{
  U64 hash;
  const char *name;
  // name format:
  // tex.Bricks071
  // Tree.Bark_diffuse
} MATERIAL_Key;

static MATERIAL_Key MATERIAL_CreateKey(const char *name_cstr_literal)
{
  MATERIAL_Key res = {};
  res.hash = MATERIAL_HashCstr(name_cstr_literal);
  res.name = name_cstr_literal;
  return res;
}


#define TEX_LIST(X) \
X(Bricks071) \
X(Bricks097) \
X(Grass004) \
X(Ground037) \
X(Ground068) \
X(Ground078) \
X(Leather011) \
X(PavingStones067) \
X(Tiles101) \
X(TestPBR001) \
X(Tree0Bark) \
X(Tree0eaves) \
X(Tree0Material)

//
// Textures
//
typedef U32 TEX_Kind;
enum
{
#define TEX_DEF_ENUM(a) TEX_##a,
  TEX_LIST(TEX_DEF_ENUM)
  TEX_COUNT
};

READ_ONLY static S8 TEX_Names[] =
{
#define TEX_DEF_NAMES(a) {(U8 *)#a, sizeof(#a)-1},
  TEX_LIST(TEX_DEF_NAMES)
};

static S8 TEX_GetName(TEX_Kind tex_kind)
{
  Assert(tex_kind < TEX_COUNT);
  return TEX_Names[tex_kind];
}



//
// Models
//
#define MODEL_LIST(X) \
X(Flag) \
X(Tree) \
X(Worker) \
X(Formal) \
X(Casual)

typedef U32 MODEL_Type;
enum
{
#define MODEL_DEF_ENUM(a) MODEL_##a,
  MODEL_LIST(MODEL_DEF_ENUM)
  MODEL_COUNT
};

READ_ONLY static const char *MODEL_CstrNames[] =
{
#define MODEL_DEF_CSTR_NAMES(a) #a,
  MODEL_LIST(MODEL_DEF_CSTR_NAMES)
};
READ_ONLY static S8 MODEL_Names[] =
{
#define MODEL_DEF_NAMES(a) {(U8 *)#a, sizeof(#a)-1},
  MODEL_LIST(MODEL_DEF_NAMES)
};

static const char *MODEL_GetCstrName(MODEL_Type model)
{
  if (model < ArrayCount(MODEL_CstrNames))
    return MODEL_CstrNames[model];
  return "MODEL.MISSING.cstr";
}

static S8 MODEL_GetName(MODEL_Type model)
{
  if (model < ArrayCount(MODEL_Names))
    return MODEL_Names[model];
  return S8Lit("MODEL.MISSING.S8");
}
