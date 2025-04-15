static V3 M_ParseV3(S8 x, S8 y, S8 z)
{
  V3 res =
  {
    (float)Parse_Float(x),
    (float)Parse_Float(y),
    (float)Parse_Float(z)
  };
  return res;
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
