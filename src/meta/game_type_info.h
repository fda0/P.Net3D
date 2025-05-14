//
// Runtime type info
//
typedef struct { U64 a; } UNKNOWN;

#define TYPE_LIST(X) \
/* Type */ \
X(UNKNOWN) \
X(bool) \
X(U8) \
X(U16) \
X(U32) \
X(U64) \
X(I8) \
X(I16) \
X(I32) \
X(I64) \
X(float) \
X(double) \
X(V2) \
X(V3) \
X(V4) \
X(Quat) \
/* no Printer/Parser impl below */ \
X(RngU32) \
X(Mat4) \
X(PIE_List) \
X(PIE_Animation) \
X(PIE_AnimationChannel) \
X(PIE_MaterialTexSection) \
X(PIE_Material) \
X(PIE_Geometry) \
X(PIE_Model) \
X(PIE_Skeleton) \
X(PIE_Links) \
X(WORLD_Vertex)

// @todo implement slices
//#define TYPE_DEF_SLICE(T) typedef struct { T *arr; U64 count; } Slice_##T;
//TYPE_LIST(TYPE_DEF_SLICE)

typedef U32 TYPE_ENUM;
enum
{
#define TYPE_DEF_ENUM(a) TYPE_##a,
  TYPE_LIST(TYPE_DEF_ENUM)
    TYPE_COUNT
};

READ_ONLY static U32 TYPE_Sizes[] =
{
#define TYPE_DEF_SIZES(a) sizeof(a),
  TYPE_LIST(TYPE_DEF_SIZES)
};
READ_ONLY static U32 TYPE_Aligns[] =
{
#define TYPE_DEF_ALIGNS(a) _Alignof(a),
  TYPE_LIST(TYPE_DEF_ALIGNS)
};
READ_ONLY static S8 TYPE_Names[] =
{
#define TYPE_DEF_NAMES(a) {(U8 *)#a, sizeof(#a)-1},
  TYPE_LIST(TYPE_DEF_NAMES)
};

static U32 TYPE_GetSize(TYPE_ENUM type)
{
  Assert(type < TYPE_COUNT);
  return TYPE_Sizes[type];
}
static U32 TYPE_GetAlign(TYPE_ENUM type)
{
  Assert(type < TYPE_COUNT);
  return TYPE_Aligns[type];
}
static S8 TYPE_GetName(TYPE_ENUM type)
{
  Assert(type < TYPE_COUNT);
  return TYPE_Names[type];
}

#define Pr_None(...)
#define Parse_None(...) (void)

static void TYPE_Print(TYPE_ENUM type, Printer *p, void *src_ptr)
{
  switch (type)
  {
    default: Assert(false); break;
    case TYPE_bool:   Pr_U32(p, *(bool *)src_ptr); break;
    case TYPE_U8:     Pr_U32(p, *(U8 *)src_ptr); break;
    case TYPE_U16:    Pr_U32(p, *(U16 *)src_ptr); break;
    case TYPE_U32:    Pr_U32(p, *(U32 *)src_ptr); break;
    case TYPE_U64:    Pr_U64(p, *(U64 *)src_ptr); break;
    case TYPE_I8:     Pr_I32(p, *(I8 *)src_ptr); break;
    case TYPE_I16:    Pr_I32(p, *(I16 *)src_ptr); break;
    case TYPE_I32:    Pr_I32(p, *(I32 *)src_ptr); break;
    case TYPE_I64:    Pr_I64(p, *(I64 *)src_ptr); break;
    case TYPE_float:  Pr_Float(p, *(float *)src_ptr); break;
    case TYPE_double: Pr_Float(p, *(double *)src_ptr); break;
    case TYPE_V2:     Pr_V2(p, *(V2 *)src_ptr); break;
    case TYPE_V3:     Pr_V3(p, *(V3 *)src_ptr); break;
    case TYPE_V4:     Pr_V4(p, *(V4 *)src_ptr); break;
    case TYPE_Quat:   Pr_V4(p, *(Quat *)src_ptr); break;
  }
}

static void TYPE_Parse(TYPE_ENUM type, S8 string, void *dst_ptr, bool *err, U64 *adv)
{
  switch (type)
  {
    default: Assert(false); break;
    case TYPE_bool:   *(bool *)dst_ptr   = Parse_U32(string, err, adv); break;
    case TYPE_U8:     *(U8 *)dst_ptr     = Parse_U32(string, err, adv); break;
    case TYPE_U16:    *(U16 *)dst_ptr    = Parse_U32(string, err, adv); break;
    case TYPE_U32:    *(U32 *)dst_ptr    = Parse_U32(string, err, adv); break;
    case TYPE_U64:    *(U64 *)dst_ptr    = Parse_U64(string, err, adv); break;
    case TYPE_I8:     *(I8 *)dst_ptr     = Parse_I32(string, err, adv); break;
    case TYPE_I16:    *(I16 *)dst_ptr    = Parse_I32(string, err, adv); break;
    case TYPE_I32:    *(I32 *)dst_ptr    = Parse_I32(string, err, adv); break;
    case TYPE_I64:    *(I64 *)dst_ptr    = Parse_I64(string, err, adv); break;
    case TYPE_float:  *(float *)dst_ptr  = Parse_Float(string, err, adv); break;
    case TYPE_double: *(double *)dst_ptr = Parse_Float(string, err, adv); break;
    case TYPE_V2:     *(V2 *)dst_ptr     = Parse_V2(string, err, adv); break;
    case TYPE_V3:     *(V3 *)dst_ptr     = Parse_V3(string, err, adv); break;
    case TYPE_V4:     *(V4 *)dst_ptr     = Parse_V4(string, err, adv); break;
    case TYPE_Quat:   *(Quat *)dst_ptr   = Parse_V4(string, err, adv); break;
  }
}

#undef Pr_None
#undef Parse_None
