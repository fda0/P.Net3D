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
  S8 name;
  // name format:
  //   tex.Bricks071
  //   Tree.Bark_diffuse
} MATERIAL_Key;

static MATERIAL_Key MATERIAL_CreateKey(S8 name /* EXTERNALLY OWNED STRING */)
{
  MATERIAL_Key res = {};
  res.hash = MATERIAL_Hash(name);
  res.name = name;
  return res;
}

static bool MATERIAL_KeyMatch(MATERIAL_Key a, MATERIAL_Key b)
{
  return a.hash == b.hash;
}

static bool MATERIAL_KeyIsZero(MATERIAL_Key a)
{
  return !a.hash;
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
