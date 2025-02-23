static I32 M_ParseInt(S8 number)
{
  char buf[128];
  if (number.size > ArrayCount(buf) - 1)
    number.size = ArrayCount(buf) - 1;

  memcpy(buf, number.str, number.size);
  buf[number.size] = 0;

  I32 result = SDL_atoi(buf);
  return result;
}

static double M_ParseDouble(S8 number)
{
  char buf[128];
  if (number.size > ArrayCount(buf) - 1)
    number.size = ArrayCount(buf) - 1;

  memcpy(buf, number.str, number.size);
  buf[number.size] = 0;

  double result = SDL_atof(buf);
  return result;
}

static V3 M_ParseV3(S8 x, S8 y, S8 z)
{
  V3 res =
  {
    (float)M_ParseDouble(x),
    (float)M_ParseDouble(y),
    (float)M_ParseDouble(z)
  };
  return res;
}

static void Pr_Float(Printer *p, float value)
{
  // @todo remove clib dependency in the future
  char buf[128];
  snprintf(buf, sizeof(buf), "%f", value);
  S8 string = S8_ScanCstr(buf);
  Pr_S8(p, string);
}

static void Pr_U16(Printer *p, U16 value)
{
  // @todo remove clib dependency in the future
  char buf[128];
  snprintf(buf, sizeof(buf), "%u", (U32)value);
  S8 string = S8_ScanCstr(buf);
  Pr_S8(p, string);
}

static void Pr_U32(Printer *p, U32 value)
{
  // @todo remove clib dependency in the future
  char buf[128];
  snprintf(buf, sizeof(buf), "%u", value);
  S8 string = S8_ScanCstr(buf);
  Pr_S8(p, string);
}

static void Pr_I32(Printer *p, I32 value)
{
  // @todo remove clib dependency in the future
  char buf[128];
  snprintf(buf, sizeof(buf), "%d", value);
  S8 string = S8_ScanCstr(buf);
  Pr_S8(p, string);
}

static void Pr_U32Hex(Printer *p, U32 value)
{
  // @todo remove clib dependency in the future
  char buf[128];
  snprintf(buf, sizeof(buf), "%x", value);
  S8 string = S8_ScanCstr(buf);
  Pr_S8(p, string);
}

static void Pr_U64(Printer *p, U64 value)
{
  // @todo remove clib dependency in the future
  char buf[128];
  snprintf(buf, sizeof(buf), "%llu", value);
  S8 string = S8_ScanCstr(buf);
  Pr_S8(p, string);
}

static void Pr_FloatArray(Printer *p, float *numbers, U64 number_count)
{
  U64 per_row = 8;
  ForU64(i, number_count)
  {
    Pr_Float(p, numbers[i]);
    Pr_S8(p, S8Lit("f"));

    if (i + 1 < number_count)
    {
      Pr_S8(p, S8Lit(","));
      S8 whitespace = (i % per_row == per_row-1 ? S8Lit("\n") : S8Lit(" "));
      Pr_S8(p, whitespace);
    }
  }
}
