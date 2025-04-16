#define PARSE_COPY_TO_CSTR(buf, string) \
char buf[128]; \
string.size = Min(string.size, sizeof(buf)-1); \
memcpy(buf, string.str, string.size); \
buf[string.size] = 0;

static I64 Parse_I64(S8 string, bool *err)
{
  PARSE_COPY_TO_CSTR(buf, string);

  char *end = buf;
  I64 result = SDL_strtoll(buf, &end, 0);
  if (err) *err |= (buf == end);
  return result;
}
static I32 Parse_I32(S8 string, bool *err) { return (I32)Parse_I64(string, err); }
static I16 Parse_I16(S8 string, bool *err) { return (I16)Parse_I64(string, err); }
static I8   Parse_I8(S8 string, bool *err) { return  (I8)Parse_I64(string, err); }

static U64 Parse_U64(S8 string, bool *err)
{
  PARSE_COPY_TO_CSTR(buf, string);

  char *end = buf;
  U64 result = SDL_strtoull(buf, &end, 0);
  if (err) *err |= (buf == end);
  return result;
}
static U32 Parse_U32(S8 string, bool *err) { return (U32)Parse_U64(string, err); }
static U16 Parse_U16(S8 string, bool *err) { return (U16)Parse_U64(string, err); }
static U8   Parse_U8(S8 string, bool *err) { return (U8)Parse_U64(string, err); }

static double Parse_Double(S8 string, bool *err)
{
  PARSE_COPY_TO_CSTR(buf, string);

  char *end = buf;
  double result = SDL_strtod(buf, &end);
  if (err) *err |= (buf == end);
  return result;
}
static float Parse_Float(S8 string, bool *err) { return (float)Parse_Double(string, err); }

static void Parse_FloatArray(S8 string, float *arr, U32 arr_count, bool *err)
{
  S8_Consume(&string, S8Lit("{"), 0);

  ForU32(i, arr_count)
  {
    while (S8_Consume(&string, S8Lit(" "), 0));
    arr[i] = Parse_Float(string, err);
    S8_Consume(&string, S8Lit(","), 0);
  }
}
static V2 Parse_V2(S8 string, bool *err)
{
  V2 result = {};
  Parse_FloatArray(string, result.E, ArrayCount(result.E), err);
  return result;
}
static V3 Parse_V3(S8 string, bool *err)
{
  V3 result = {};
  Parse_FloatArray(string, result.E, ArrayCount(result.E), err);
  return result;
}
static V4 Parse_V4(S8 string, bool *err)
{
  V4 result = {};
  Parse_FloatArray(string, result.E, ArrayCount(result.E), err);
  return result;
}
