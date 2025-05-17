#define PARSE_COPY_TO_CSTR(buf, string) \
char buf[128]; \
string.size = Min(string.size, sizeof(buf)-1); \
memcpy(buf, string.str, string.size); \
buf[string.size] = 0;

static I64 Parse_I64(S8 string, bool *err, U64 *adv)
{
  PARSE_COPY_TO_CSTR(buf, string);

  char *end = buf;
  I64 result = SDL_strtoll(buf, &end, 0);
  if (err) *err |= (buf == end);
  if (adv) *adv = (U64)end - (U64)buf;
  return result;
}
static I32 Parse_I32(S8 string, bool *err, U64 *adv) { return (I32)Parse_I64(string, err, adv); }
static I16 Parse_I16(S8 string, bool *err, U64 *adv) { return (I16)Parse_I64(string, err, adv); }
static I8   Parse_I8(S8 string, bool *err, U64 *adv) { return  (I8)Parse_I64(string, err, adv); }

static U64 Parse_U64(S8 string, bool *err, U64 *adv)
{
  PARSE_COPY_TO_CSTR(buf, string);

  char *end = buf;
  U64 result = SDL_strtoull(buf, &end, 0);
  if (err) *err |= (buf == end);
  if (adv) *adv = (U64)end - (U64)buf;
  return result;
}
static U32 Parse_U32(S8 string, bool *err, U64 *adv) { return (U32)Parse_U64(string, err, adv); }
static U16 Parse_U16(S8 string, bool *err, U64 *adv) { return (U16)Parse_U64(string, err, adv); }
static U8   Parse_U8(S8 string, bool *err, U64 *adv) { return  (U8)Parse_U64(string, err, adv); }

static double Parse_Double(S8 string, bool *err, U64 *adv)
{
  PARSE_COPY_TO_CSTR(buf, string);

  char *end = buf;
  double result = SDL_strtod(buf, &end);
  if (err) *err |= (buf == end);
  if (adv) *adv = (U64)end - (U64)buf;
  return result;
}
static float Parse_Float(S8 string, bool *err, U64 *adv) { return (float)Parse_Double(string, err, adv); }

static void Parse_FloatArray(S8 string, float *arr, U32 arr_count, bool *err, U64 *adv)
{
  U8 *start_str = string.str;
  S8_ConsumeSkip(&string, S8Lit("{"), 0);

  ForU32(i, arr_count)
  {
    while (S8_ConsumeSkip(&string, S8Lit(" "), 0));
    U64 parse_adv = 0;
    arr[i] = Parse_Float(string, err, &parse_adv);
    string = S8_Skip(string, parse_adv);
    S8_ConsumeSkip(&string, S8Lit(","), 0);
  }

  if (adv) *adv = (U64)string.str - (U64)start_str;
}
static V2 Parse_V2(S8 string, bool *err, U64 *adv)
{
  V2 result = {};
  Parse_FloatArray(string, result.E, ArrayCount(result.E), err, adv);
  return result;
}
static V3 Parse_V3(S8 string, bool *err, U64 *adv)
{
  V3 result = {};
  Parse_FloatArray(string, result.E, ArrayCount(result.E), err, adv);
  return result;
}
static V4 Parse_V4(S8 string, bool *err, U64 *adv)
{
  V4 result = {};
  Parse_FloatArray(string, result.E, ArrayCount(result.E), err, adv);
  return result;
}
