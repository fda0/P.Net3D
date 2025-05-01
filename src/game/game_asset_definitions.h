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
#define MDL_LIST(X) \
X(Flag, Rigid) \
X(Tree, Rigid) \
X(Worker, Skinned) \
X(Formal, Skinned) \
X(Casual, Skinned)

typedef enum
{
#define MDL_DEF_ENUM(a, b) MDL_##a,
  MDL_LIST(MDL_DEF_ENUM)
  MDL_COUNT
} MDL_Kind;

// MDL Categories
typedef enum
{
  MDL_Rigid,
  MDL_Skinned,
} MDL_Category;

READ_ONLY static MDL_Category MDL_Categories[] =
{
#define MDL_DEF_CATEGORIES(a, b) MDL_##b,
  MDL_LIST(MDL_DEF_CATEGORIES)
};
READ_ONLY static const char *MDL_CstrNames[] =
{
#define MDL_DEF_CSTR_NAMES(a, b) #a
  MDL_LIST(MDL_DEF_CSTR_NAMES)
};

static MDL_Category MDL_GetCategory(MDL_Kind model_kind)
{
  Assert(model_kind < MDL_COUNT);
  return MDL_Categories[model_kind];
}
static MDL_Category MDL_IsSkinned(MDL_Kind model_kind) // @todo remove this, this is a property of asset loaded from file
{
  return MDL_GetCategory(model_kind) == MDL_Skinned;
}
static const char *MDL_GetCstrName(MDL_Kind model_kind)
{
  Assert(model_kind < MDL_COUNT);
  return MDL_CstrNames[model_kind];
}
