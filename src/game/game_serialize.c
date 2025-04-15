//
// Runtime type info
//
typedef enum
{
#define TYPE_INC(a, b) TYPE_##a,
#include "game_serialize_types.inc"
#undef TYPE_INC
  TYPE_COUNT
} TYPE_ENUM;

READ_ONLY static U32 TYPE_Sizes[] =
{
#define TYPE_INC(a, b) sizeof(a),
#include "game_serialize_types.inc"
#undef TYPE_INC
};
READ_ONLY static U32 TYPE_Aligns[] =
{
#define TYPE_INC(a, b) _Alignof(a),
#include "game_serialize_types.inc"
#undef TYPE_INC
};
READ_ONLY static S8 TYPE_Names[] =
{
#define TYPE_INC(a, b) {(U8 *)#a, sizeof(#a)-1},
#include "game_serialize_types.inc"
#undef TYPE_INC
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

static void TYPE_Print(TYPE_ENUM type, Printer *p, void *src_ptr)
{
  switch (type)
  {
    default: Assert(false); break;
#define TYPE_INC(A, B) case TYPE_##A: Pr_##B(p, *(A *)src_ptr); break;
#include "game_serialize_types.inc"
#undef TYPE_INC
  }
}

static void TYPE_Parse(TYPE_ENUM type, S8 string, void *dst_ptr)
{
  switch (type)
  {
    default: Assert(false); break;
#define TYPE_INC(A, B) case TYPE_##A: *(A *)dst_ptr = Parse_##B(string); break;
#include "game_serialize_types.inc"
#undef TYPE_INC
  }
}

//
// Serialization
//
typedef struct
{
  void *ptr;
  S8 name;
  TYPE_ENUM type;
} SERIAL_Item;
#define SERIAL_DEF(Path, Name, Type) (SERIAL_Item){&(Path Name), S8Lit(#Name), TYPE_##Type}

static U64 SERIAL_CalculateHash(SERIAL_Item *items, U32 items_count)
{
  U64 hash = 0;
  ForU32(i, items_count)
  {
    SERIAL_Item item = items[i];
    hash = U64_Hash(hash, item.ptr, TYPE_GetSize(item.type));
  }
  return hash;
}

static void SERIAL_SaveToPrinter(SERIAL_Item *items, U32 items_count, Printer *p)
{
  ForU32(i, items_count)
  {
    SERIAL_Item item = items[i];
    Pr_S8(p, TYPE_GetName(item.type));
    Pr_S8(p, S8Lit(" "));
    Pr_S8(p, item.name);
    Pr_S8(p, S8Lit(" = "));
    TYPE_Print(item.type, p, item.ptr);
    Pr_S8(p, S8Lit(";\n"));
  }
}

static void SERIAL_LoadFromS8(SERIAL_Item *items, U32 items_count, S8 source)
{
#if 1
  (void)items; (void)items_count; (void)source;
#else
  while (source.size)
  {
    // 1. eat white etc etc
    // 2. get token (also track previous token somehow... tokenizing whole file would require memory alloc, dont do that... use Queue thing from game_util.c?)
    // 3. minimum thing that needs to be parsable `some_setting = 4`,
    //    max: `U32 some_setting = 4;`,
    //    edge_case: `some_setting=        4`

    S8 token = source;
  }

  ForU32(i, items_count)
  {
    SERIAL_Item item = items[i];
    Pr_S8(p, TYPE_GetName(item.type));
    Pr_S8(p, S8Lit(" "));
    Pr_S8(p, item.name);
    Pr_S8(p, S8Lit(" = "));
    TYPE_Print(item.type, p, item.ptr);
    Pr_S8(p, S8Lit(";\n"));
  }
#endif
}

static void SERIAL_DebugSettings(bool is_load)
{
  U64 period_ms = 100;
  if (APP.debug.serialize_last_check_timestamp + period_ms > APP.timestamp)
    return;
  APP.debug.serialize_last_check_timestamp = APP.timestamp;

  SERIAL_Item items[] =
  {
    SERIAL_DEF(APP.debug., win_p, V2),
    SERIAL_DEF(APP.debug., noclip_camera, bool),
    SERIAL_DEF(APP.debug., sun_camera, bool),
    SERIAL_DEF(APP.debug., draw_collision_box, bool),
  };

  U64 max_save_size = Kilobyte(32);
  const char *file_path = "debug.p3";

  Arena *a = APP.tmp;
  ArenaScope scope = Arena_PushScope(a);
  if (is_load)
  {
    S8 file_content = OS_LoadFile(a, file_path);
    SERIAL_LoadFromS8(items, ArrayCount(items), file_content);

    U64 hash = SERIAL_CalculateHash(items, ArrayCount(items));
    APP.debug.serialize_hash = hash;
  }
  else // is_save
  {
    U64 hash = SERIAL_CalculateHash(items, ArrayCount(items));
    if (APP.debug.serialize_hash == hash)
      return;
    APP.debug.serialize_hash = hash;

    Printer p = Pr_Alloc(a, max_save_size);
    SERIAL_SaveToPrinter(items, ArrayCount(items), &p);
    OS_SaveFile(file_path, Pr_AsS8(&p));
  }
  Arena_PopScope(scope);
}
