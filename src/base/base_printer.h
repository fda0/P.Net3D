enum
{
  PrErr_Truncated = (1 << 0),
};

typedef struct
{
  U8 *buf;
  U64 used;
  U64 cap;
  U32 err; // uses AsText_Error enum flags
} Printer;

static void Pr_Reset(Printer *p)
{
  p->used = 0;
  p->err = 0;
}

static U64 Pr_FreeCount(Printer *p)
{
  if (p->cap >= p->used)
    return p->cap - p->used;
  return 0;
}

static char *Pr_AsCstr(Printer *p)
{
  if (!p->cap)
    return "";

  if (Pr_FreeCount(p))
  {
    p->buf[p->used] = 0;
    p->used += 1;
  }
  else
  {
    p->err |= PrErr_Truncated;
    p->buf[p->used-1] = 0;
  }
  return (char *)p->buf;
}

static S8 Pr_AsS8(Printer *p)
{
  return S8_Make(p->buf, p->used);
}

//
// Printer Makers
//
static Printer Pr_Make(U8 *buffer, U64 capacity)
{
  Printer res = {.buf = buffer, .cap = capacity};
  return res;
}

static Printer Pr_Alloc(Arena *a, U64 capacity)
{
  Printer res = Pr_Make(Alloc(a, U8, capacity), capacity);
  return res;
}

#define Pr_MakeOnStack(PrinterName, Size) \
U8 PrinterName##_buffer[Size]; \
Printer PrinterName = Pr_Make(PrinterName##_buffer, Size)

//
// Adding things to Printer
//
static void Pr_Data(Printer *p, void *data, U64 data_size)
{
  U64 free_count = Pr_FreeCount(p);
  if (free_count < data_size)
  {
    p->err |= PrErr_Truncated;
    if (!free_count)
      return;

    data_size = free_count;
  }

  memcpy(p->buf + p->used, data, data_size);
  p->used += data_size;
}

static void *Pr_ReserveBytes(Printer *p, U64 data_size) // returns 0 on lack of space
{
  U64 free_count = Pr_FreeCount(p);
  if (free_count < data_size)
    return 0;

  void *result = p->buf + p->used;
  p->used += data_size;
  return result;
}

static void Pr_S8(Printer *p, S8 str)
{
  Pr_Data(p, str.str, str.size);
}

static void Pr_Cstr(Printer *p, const char *cstr)
{
  Pr_S8(p, S8_FromCstr(cstr));
}

static void Pr_Printer(Printer *p, Printer *copy)
{
  Pr_S8(p, Pr_AsS8(copy));
}

// Number printers - @todo remove clib dependency in the future
static void Pr_U16(Printer *p, U16 value)
{
  char buf[128];
  snprintf(buf, sizeof(buf), "%u", (U32)value);
  Pr_Cstr(p, buf);
}

static void Pr_U32(Printer *p, U32 value)
{
  char buf[128];
  snprintf(buf, sizeof(buf), "%u", value);
  Pr_Cstr(p, buf);
}

static void Pr_U64(Printer *p, U64 value)
{
  char buf[128];
  snprintf(buf, sizeof(buf), "%llu", value);
  Pr_Cstr(p, buf);
}

static void Pr_I32(Printer *p, I32 value)
{
  char buf[128];
  snprintf(buf, sizeof(buf), "%d", value);
  Pr_Cstr(p, buf);
}

static void Pr_I64(Printer *p, I64 value)
{
  char buf[128];
  snprintf(buf, sizeof(buf), "%lld", value);
  Pr_Cstr(p, buf);
}

static void Pr_U32Hex(Printer *p, U32 value)
{
  char buf[128];
  snprintf(buf, sizeof(buf), "%x", value);
  Pr_Cstr(p, buf);
}

static void Pr_Float(Printer *p, float value)
{
  char buf[128];
  snprintf(buf, sizeof(buf), "%f", value);
  Pr_Cstr(p, buf);
}

static void Pr_V2(Printer *p, V2 value)
{
  char buf[128];
  snprintf(buf, sizeof(buf), "{%f, %f}", value.x, value.y);
  Pr_Cstr(p, buf);
}

static void Pr_V3(Printer *p, V3 value)
{
  char buf[128];
  snprintf(buf, sizeof(buf), "{%f, %f, %f}", value.x, value.y, value.z);
  Pr_Cstr(p, buf);
}

static void Pr_V4(Printer *p, V4 value)
{
  char buf[128];
  snprintf(buf, sizeof(buf), "{%f, %f, %f, %f}", value.x, value.y, value.z, value.w);
  Pr_Cstr(p, buf);
}
