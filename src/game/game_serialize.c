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

static bool SERIAL_IsIdentifierStart(U8 c)
{
  return ByteIsAlpha(c) || c == '[';
}

static bool SERIAL_IsIdentifier(U8 c)
{
  return ByteIsIdentifierPart(c) || c == '[' || c == ']' || c == '.';
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

  if (SERIAL_IsIdentifierStart(SERIAL_At(lex)))
  {
    lex.token_kind = SERIAL_TokenIdentifier;
    U64 start_at = lex.at;
    lex.at += 1;

    while (SERIAL_IsIdentifier(SERIAL_At(lex)))
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

      if (!ByteIsDigit(c) && c != '.')
        break;

      if (c == '.')
        lex.suspected_value = TYPE_float;

      lex.at += 1;
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
    //LOG(Log_Serial, "[%.*s]", S8Print(lex.token));

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
  Arena_Scope scope = Arena_PushScope(a);
  S8 file_content = OS_LoadFile(a, file_path);
  SERIAL_LoadFromS8(items, items_count, file_content);
  Arena_PopScope(scope);
}

static void SERIAL_SaveToFile(SERIAL_Item *items, U32 items_count, const char *file_path)
{
  U64 max_save_size = Kilobyte(32);

  Arena *a = APP.tmp;
  Arena_Scope scope = Arena_PushScope(a);
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
    SERIAL_DEF(APP.debug., show_debug_window, bool),
    SERIAL_DEF(APP.debug., win_p, V2),
    SERIAL_DEF(APP.debug., menu_category, U32),
    SERIAL_DEF(APP.debug., noclip_camera, bool),
    SERIAL_DEF(APP.debug., sun_camera, bool),
    SERIAL_DEF(APP.debug., draw_model_collision, bool),
  };

  const char *file_path = "debug.c_config";
  if (is_load)
  {
    SERIAL_LoadFromFile(items, ArrayCount(items), file_path);
    U64 hash = SERIAL_CalculateHash(items, ArrayCount(items));
    APP.debug.serialize_hash = hash;
  }
  else
  {
    U64 hash = SERIAL_CalculateHash(items, ArrayCount(items));
    if (APP.debug.serialize_hash == hash)
      return;
    APP.debug.serialize_hash = hash;

    SERIAL_SaveToFile(items, ArrayCount(items), file_path);
  }
}

static void SERIAL_AssetSettings(bool is_load)
{
#if 1
  (void)is_load;
#else
  if (!APP.ast.materials_count)
    return;

  if (!is_load)
  {
    U64 period_ms = 100;
    if (APP.ast.serialize_last_check_timestamp + period_ms > APP.timestamp)
      return;
    APP.ast.serialize_last_check_timestamp = APP.timestamp;
  }

  {
    ArenaScope scope = Arena_PushScope(APP.tmp);

    U32 items_count = APP.ast.materials_count;
    SERIAL_Item *items = Alloc(scope.a, SERIAL_Item, items_count);
    ForU32(i, items_count)
    {
      ASSET_Material *material = APP.ast.materials + i;
      items[i].ptr = &material->shininess;
      items[i].name = material->key.name;
      items[i].type = TYPE_float;
    }

    const char *file_path = "../src/data/assets.c_config";
    if (is_load)
    {
      SERIAL_LoadFromFile(items, items_count, file_path);
      U64 hash = SERIAL_CalculateHash(items, items_count);
      APP.ast.serialize_hash = hash;
    }
    else
    {
      U64 hash = SERIAL_CalculateHash(items, items_count);
      if (APP.ast.serialize_hash != hash)
      {
        APP.ast.serialize_hash = hash;
        SERIAL_SaveToFile(items, items_count, file_path);
      }
    }

    Arena_PopScope(scope);
  }
#endif
}