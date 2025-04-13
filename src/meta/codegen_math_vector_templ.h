// --- --- --- --- --- --- ---
// --- Vector math for SCALAR ---
// --- --- --- --- --- --- ---
typedef union
{
  struct { SCALAR x, y; };
  SCALAR E[2];
} V2POST;

typedef union
{
  struct { SCALAR x, y, z; };
  SCALAR E[3];
} V3POST;

typedef union
{
  struct { SCALAR x, y, z, w; };
  SCALAR E[4];
} V4POST;

static V2POST V2POST_Scale(V2POST a, SCALAR scale) { return (V2POST){a.x*scale, a.y*scale}; }
static V3POST V3POST_Scale(V3POST a, SCALAR scale) { return (V3POST){a.x*scale, a.y*scale, a.z*scale}; }
static V4POST V4POST_Scale(V4POST a, SCALAR scale) { return (V4POST){a.x*scale, a.y*scale, a.z*scale, a.w*scale}; }

static V2POST V2POST_Add(V2POST a, V2POST b) { return (V2POST){a.x + b.x, a.y + b.y}; }
static V3POST V3POST_Add(V3POST a, V3POST b) { return (V3POST){a.x + b.x, a.y + b.y, a.z + b.z}; }
static V4POST V4POST_Add(V4POST a, V4POST b) { return (V4POST){a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; }

static V2POST V2POST_Sub(V2POST a, V2POST b) { return (V2POST){a.x - b.x, a.y - b.y}; }
static V3POST V3POST_Sub(V3POST a, V3POST b) { return (V3POST){a.x - b.x, a.y - b.y, a.z - b.z}; }
static V4POST V4POST_Sub(V4POST a, V4POST b) { return (V4POST){a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w}; }

static V2POST V2POST_Mul(V2POST a, V2POST b) { return (V2POST){a.x * b.x, a.y * b.y}; }
static V3POST V3POST_Mul(V3POST a, V3POST b) { return (V3POST){a.x * b.x, a.y * b.y, a.z * b.z}; }
static V4POST V4POST_Mul(V4POST a, V4POST b) { return (V4POST){a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w}; }

static V2POST V2POST_Clamp(V2POST min, V2POST max, V2POST val) {
  return (V2POST){Clamp(min.x, max.x, val.x), Clamp(min.y, max.y, val.y)};
}
static V3POST V3POST_Clamp(V3POST min, V3POST max, V3POST val) {
  return (V3POST){Clamp(min.x, max.x, val.x), Clamp(min.y, max.y, val.y), Clamp(min.z, max.z, val.z)};
}
static V4POST V4POST_Clamp(V4POST min, V4POST max, V4POST val) {
  return (V4POST){Clamp(min.x, max.x, val.x), Clamp(min.y, max.y, val.y), Clamp(min.z, max.z, val.z), Clamp(min.w, max.w, val.w)};
}

static V2POST V2POST_Reverse(V2POST a) { return (V2POST){-a.x, -a.y}; }
static V3POST V3POST_Reverse(V3POST a) { return (V3POST){-a.x, -a.y, -a.z}; }
static V4POST V4POST_Reverse(V4POST a) { return (V4POST){-a.x, -a.y, -a.z, -a.w}; }

static SCALAR V2POST_Dot(V2POST a, V2POST b) { return a.x*b.x + a.y*b.y; }
static SCALAR V3POST_Dot(V3POST a, V3POST b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static SCALAR V4POST_Dot(V4POST a, V4POST b) { return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w; }

static bool V2POST_HasLength(V2POST a) { return a.x || a.y; }
static bool V3POST_HasLength(V3POST a) { return a.x || a.y || a.z; }
static bool V4POST_HasLength(V4POST a) { return a.x || a.y || a.z || a.w; }
