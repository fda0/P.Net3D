//
// Runtime type info
//
#define TYPE_LIST(X) \
X(bool, U32) \
X(U32, U32) \
X(U64, U64) \
X(I32, I32) \
X(I64, I64) \
X(float, Float) \
X(V2, V2) \
X(V3, V3) \
X(V4, V4)

typedef enum
{
#define TYPE_DEF_ENUM(a, b) TYPE_##a,
  TYPE_LIST(TYPE_DEF_ENUM)
  TYPE_COUNT
} TYPE_ENUM;

READ_ONLY static U32 TYPE_Sizes[] =
{
#define TYPE_DEF_SIZES(a, b) sizeof(a),
  TYPE_LIST(TYPE_DEF_SIZES)
};
READ_ONLY static U32 TYPE_Aligns[] =
{
#define TYPE_DEF_ALIGNS(a, b) _Alignof(a),
  TYPE_LIST(TYPE_DEF_ALIGNS)
};
READ_ONLY static S8 TYPE_Names[] =
{
#define TYPE_DEF_NAMES(a, b) {(U8 *)#a, sizeof(#a)-1},
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

static void TYPE_Print(TYPE_ENUM type, Printer *p, void *src_ptr)
{
  switch (type)
  {
    default: Assert(false); break;
#define TYPE_DEF_PRINT(A, B) case TYPE_##A: Pr_##B(p, *(A *)src_ptr); break;
    TYPE_LIST(TYPE_DEF_PRINT)
  }
}

static void TYPE_Parse(TYPE_ENUM type, S8 string, void *dst_ptr, bool *err, U64 *adv)
{
  switch (type)
  {
    default: Assert(false); break;
#define TYPE_DEF_PARSE(A, B) case TYPE_##A: *(A *)dst_ptr = Parse_##B(string, err, adv); break;
    TYPE_LIST(TYPE_DEF_PARSE)
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

typedef enum
{
  SERIAL_TokenUnknown,
  SERIAL_TokenIdentifier,
  SERIAL_TokenAssignment,
  SERIAL_TokenValue,
  SERIAL_TokenEndOfSource,
} SERIAL_TokenKind;

typedef struct
{
  // Keeping the whole source and offset around
  // for better error reporting in the future.
  S8 txt;
  U64 at;
  S8 token;
  SERIAL_TokenKind token_kind;
  TYPE_ENUM suspected_value;
} SERIAL_Lexer;

static U8 SERIAL_At(SERIAL_Lexer lex)
{
  if (lex.at < lex.txt.size)
    return lex.txt.str[lex.at];
  return 0;
}

static SERIAL_Lexer SERIAL_NextToken(SERIAL_Lexer lex)
{
  lex.token = (S8){};
  lex.token_kind = 0;
  lex.suspected_value = 0;

  // eat white characters
  while (ByteIsWhite(SERIAL_At(lex)))
    lex.at += 1;

  if (lex.at >= lex.txt.size)
  {
    lex.token_kind = SERIAL_TokenEndOfSource;
    return lex;
  }

  if (SERIAL_At(lex) == '=')
  {
    lex.token_kind = SERIAL_TokenAssignment;
    lex.token = S8_Substring(lex.txt, lex.at, lex.at + 1);
    lex.at += 1;
    return lex;
  }

  if (ByteIsAlpha(SERIAL_At(lex)))
  {
    lex.token_kind = SERIAL_TokenIdentifier;
    U64 start_at = lex.at;
    lex.at += 1;

    while (ByteIsIdentifierPart(SERIAL_At(lex)))
      lex.at += 1;

    lex.token = S8_Substring(lex.txt, start_at, lex.at);
    return lex;
  }

  if (SERIAL_At(lex) == '{')
  {
    lex.token_kind = SERIAL_TokenValue;
    U64 start_at = lex.at;

    U32 comma_count = 0;
    for (;;)
    {
      U8 c = SERIAL_At(lex);
      if (!c) break;

      if (c == ',')
        comma_count += 1;

      lex.at += 1;

      if (c == '}')
        break;
    }

    lex.token = S8_Substring(lex.txt, start_at, lex.at);

    switch (comma_count)
    {
      case 0:
      {
        lex.suspected_value = TYPE_float;
        lex.token = S8_Skip(lex.token, 1);
      } break;
      case 1: lex.suspected_value = TYPE_V2; break;
      case 2: lex.suspected_value = TYPE_V3; break;
      default:
      case 3: lex.suspected_value = TYPE_V4; break;
    }
    return lex;
  }

  if (ByteIsDigit(SERIAL_At(lex)) || SERIAL_At(lex) == '-')
  {
    lex.token_kind = SERIAL_TokenValue;
    lex.suspected_value = TYPE_I64;

    U64 start_at = lex.at;
    lex.at += 1;

    for (;;)
    {
      U8 c = SERIAL_At(lex);
      if (!c) break;

      if (c == '.')
        lex.suspected_value = TYPE_float;

      if (!ByteIsDigit(c) && c != '.')
        break;
    }

    lex.token = S8_Substring(lex.txt, start_at, lex.at);
    return lex;
  }

  // we don't know what's going on!
  lex.at += 1;
  return lex;
}

static void SERIAL_LoadFromS8(SERIAL_Item *items, U32 items_count, S8 source)
{
  S8 last_identifier = {};
  SERIAL_Lexer lex = {source};
  for (;;)
  {
    lex = SERIAL_NextToken(lex);
    LOG(Log_Serial, "[%.*s]", S8Print(lex.token));

    if (lex.token_kind == SERIAL_TokenEndOfSource)
      break;

    if (lex.token_kind == SERIAL_TokenIdentifier)
    {
      last_identifier = lex.token;
    }

    if (lex.token_kind == SERIAL_TokenValue && last_identifier.size)
    {
      ForU32(i, items_count)
      {
        SERIAL_Item item = items[i];
        if (S8_Match(item.name, last_identifier, 0))
        {
          TYPE_Parse(item.type, lex.token, item.ptr, 0, 0); // @todo report parse error
          break;
        }
      }
      last_identifier = (S8){};
    }
  }
}

static void SERIAL_LoadFromFile(SERIAL_Item *items, U32 items_count, const char *file_path)
{
  Arena *a = APP.tmp;
  ArenaScope scope = Arena_PushScope(a);
  S8 file_content = OS_LoadFile(a, file_path);
  SERIAL_LoadFromS8(items, items_count, file_content);
  Arena_PopScope(scope);
}

static void SERIAL_SaveToFile(SERIAL_Item *items, U32 items_count, const char *file_path)
{
  U64 max_save_size = Kilobyte(32);

  Arena *a = APP.tmp;
  ArenaScope scope = Arena_PushScope(a);
  Printer p = Pr_Alloc(a, max_save_size);
  SERIAL_SaveToPrinter(items, items_count, &p);
  OS_SaveFile(file_path, Pr_AsS8(&p));
  Arena_PopScope(scope);
}

static void SERIAL_DebugSettings(bool is_load)
{
  if (!is_load)
  {
    U64 period_ms = 100;
    if (APP.debug.serialize_last_check_timestamp + period_ms > APP.timestamp)
      return;
    APP.debug.serialize_last_check_timestamp = APP.timestamp;
  }

  SERIAL_Item items[] =
  {
    SERIAL_DEF(APP.debug., win_p, V2),
    SERIAL_DEF(APP.debug., noclip_camera, bool),
    SERIAL_DEF(APP.debug., sun_camera, bool),
    SERIAL_DEF(APP.debug., draw_collision_box, bool),
  };

  const char *file_path = "debug.p3";
  if (is_load)
  {
    SERIAL_LoadFromFile(items, ArrayCount(items), file_path);
    U64 hash = SERIAL_CalculateHash(items, ArrayCount(items));
    APP.debug.serialize_hash = hash;

    LOG(Log_Serial, "(LOAD) win_p: %f, %f, hash: %llu", APP.debug.win_p.x, APP.debug.win_p.y, hash);
  }
  else
  {
    U64 hash = SERIAL_CalculateHash(items, ArrayCount(items));
    if (APP.debug.serialize_hash == hash)
      return;
    APP.debug.serialize_hash = hash;
    LOG(Log_Serial, "(SAVE) win_p: %f, %f, hash: %llu", APP.debug.win_p.x, APP.debug.win_p.y, hash);

    SERIAL_SaveToFile(items, ArrayCount(items), file_path);
  }
}
