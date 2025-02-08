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

static void Pr_AddFloat(Printer *p, float value)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "%f", value);
    S8 string = S8_MakeScanCstr(buf);
    Pr_Add(p, string);
}

static void Pr_AddU16(Printer *p, U16 value)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "%u", (U32)value);
    S8 string = S8_MakeScanCstr(buf);
    Pr_Add(p, string);
}
