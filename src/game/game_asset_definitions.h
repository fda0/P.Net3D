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
X(TestPBR001)

//
// Textures
//
typedef enum
{
#define TEX_DEF_ENUM(a) TEX_##a,
  TEX_LIST(TEX_DEF_ENUM)
  TEX_COUNT
} TEX_Kind;

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

typedef enum
{
#define MODEL_DEF_ENUM(a) MODEL_##a,
  MODEL_LIST(MODEL_DEF_ENUM)
  MODEL_COUNT
} MODEL_Type;

READ_ONLY static const char *MODEL_CstrNames[] =
{
#define MODEL_DEF_CSTR_NAMES(a) #a
  MODEL_LIST(MODEL_DEF_CSTR_NAMES)
};

static const char *MODEL_GetCstrName(MODEL_Type model)
{
  Assert(model < MODEL_COUNT);
  return MODEL_CstrNames[model];
}
