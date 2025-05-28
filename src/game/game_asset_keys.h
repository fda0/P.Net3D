// MATERIAL
static U64 MATERIAL_Hash(S8 name)
{
  HashState hs = HashBegin();
  S8_HashAbsorb(&hs, S8Lit("MATERIAL_KEY:"));
  S8_HashAbsorb(&hs, name);
  U64 hash = HashEnd(&hs);
  return hash;
}
static U64 MATERIAL_HashCstr(const char *name)
{
  return MATERIAL_Hash(S8_FromCstr(name));
}

typedef struct
{
  U64 hash;
  S8 name;
  // @info name is used for debugging/logging only -
  // - and might be excluded from shipping builds.
  // The constraint is this string's buffer has to be stored externally and
  // it has to be valid through the whole lifetime of the program.
  // This is fine for now. In the future if we add .pie file reloading we might
  // decide to store these names in some lookup table outside of MATERIAL_Key itself.

  // name format:
  //   tex.Bricks071
  //   Tree.Bark_diffuse
} MATERIAL_Key;
static MATERIAL_Key MATERIAL_CreateKey(S8 name /* EXTERNALLY OWNED STRING */)
{
  MATERIAL_Key res = {};
  res.hash = MATERIAL_Hash(name);
  res.name = name;
  return res;
}
static bool MATERIAL_KeyMatch(MATERIAL_Key a, MATERIAL_Key b)
{
  return a.hash == b.hash;
}
static bool MATERIAL_KeyIsZero(MATERIAL_Key a)
{
  return !a.hash;
}

// MODEL
static U64 MODEL_Hash(S8 name)
{
  HashState hs = HashBegin();
  S8_HashAbsorb(&hs, S8Lit("MODEL_KEY:"));
  S8_HashAbsorb(&hs, name);
  U64 hash = HashEnd(&hs);
  return hash;
}
static U64 MODEL_HashCstr(const char *name)
{
  return MODEL_Hash(S8_FromCstr(name));
}

typedef struct
{
  U64 hash;
  S8 name;
} MODEL_Key;
static MODEL_Key MODEL_CreateKey(S8 name /* EXTERNALLY OWNED STRING */)
{
  MODEL_Key res = {};
  res.hash = MODEL_Hash(name);
  res.name = name;
  return res;
}
static bool MODEL_KeyMatch(MODEL_Key a, MODEL_Key b)
{
  return a.hash == b.hash;
}
static bool MODEL_KeyIsZero(MODEL_Key a)
{
  return !a.hash;
}
