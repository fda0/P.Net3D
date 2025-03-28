//
// Textures
//
typedef enum
{
#define TEX_INC(a) TEX_##a
#include "assets_textures.inc"
#undef TEX_INC
  TEX_COUNT
} TEX_Kind;

// TEX Paths
static S8 TEX_Names[] =
{
#define TEX_INC(a) {(U8 *)#a, sizeof(#a)-1}
#include "assets_textures.inc"
#undef TEX_INC
};

static S8 TEX_GetName(TEX_Kind tex_kind)
{
  Assert(tex_kind < TEX_COUNT);
  return TEX_Names[tex_kind];
}

//
// Models
//
typedef enum
{
#define MDL_INC(a, b) MDL_##a
#include "assets_models.inc"
#undef MDL_INC
  MDL_COUNT
} MDL_Kind;

// MDL Categories
typedef enum
{
  MDL_Rigid,
  MDL_Skinned,
} MDL_Category;

static MDL_Category MDL_Categories[] =
{
#define MDL_INC(a, b) MDL_##b
#include "assets_models.inc"
#undef MDL_INC
};

static MDL_Category MDL_GetCategory(MDL_Kind model_kind)
{
  Assert(model_kind < MDL_COUNT);
  return MDL_Categories[model_kind];
}
static MDL_Category MDL_IsSkinned(MDL_Kind model_kind)
{
  return MDL_GetCategory(model_kind) == MDL_Skinned;
}

// MDL Cstr Names
static const char *MDL_CstrNames[] =
{
#define MDL_INC(a, b) #a
#include "assets_models.inc"
#undef MDL_INC
};

static const char *MDL_GetCstrName(MDL_Kind model_kind)
{
  Assert(model_kind < MDL_COUNT);
  return MDL_CstrNames[model_kind];
}
