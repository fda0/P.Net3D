static I64 Parse_I64(S8 number)
{
  char buf[128];
  number.size = Min(number.size, sizeof(buf)-1);
  memcpy(buf, number.str, number.size);
  buf[number.size] = 0;
  I64 result = SDL_strtoll(buf, 0, 0);
  return result;
}
static I32 Parse_I32(S8 number)
{
  return (I32)Parse_I64(number);
}
static I16 Parse_I16(S8 number)
{
  return (I16)Parse_I64(number);
}
static I8 Parse_I8(S8 number)
{
  return (I8)Parse_I64(number);
}

static U64 Parse_U64(S8 number)
{
  char buf[128];
  number.size = Min(number.size, sizeof(buf)-1);
  memcpy(buf, number.str, number.size);
  buf[number.size] = 0;
  U64 result = SDL_strtoull(buf, 0, 0);
  return result;
}
static U32 Parse_U32(S8 number)
{
  return (U32)Parse_U64(number);
}
static U16 Parse_U16(S8 number)
{
  return (U16)Parse_U64(number);
}
static U8 Parse_U8(S8 number)
{
  return (U8)Parse_U64(number);
}

static double Parse_Double(S8 number)
{
  char buf[128];
  number.size = Min(number.size, sizeof(buf)-1);
  memcpy(buf, number.str, number.size);
  buf[number.size] = 0;
  double result = SDL_atof(buf);
  return result;
}
static float Parse_Float(S8 number)
{
  return (float)Parse_Double(number);
}
