// float
typedef struct
{
  float *vals;
  U64 used;
  U64 cap;
} M_FloatBuffer;
static M_FloatBuffer M_FloatBuffer_Alloc(Arena *a, U64 count)
{
  M_FloatBuffer buf = {};
  buf.vals = Alloc(a, float, count);
  buf.cap = count;
  return buf;
}
static float *M_FloatBuffer_Push(M_FloatBuffer *buf, U64 count)
{
  M_AssertAlways(buf->cap >= buf->used);
  M_AssertAlways(count <= (buf->cap - buf->used));
  float *res = buf->vals + buf->used;
  buf->used += count;
  return res;
}

// I16
typedef struct
{
  I16 *vals;
  U64 used;
  U64 cap;
} M_I16Buffer;
static M_I16Buffer M_I16Buffer_Alloc(Arena *a, U64 count)
{
  M_I16Buffer buf = {};
  buf.vals = Alloc(a, I16, count);
  buf.cap = count;
  return buf;
}
static I16 *M_I16Buffer_Push(M_I16Buffer *buf, U64 count)
{
  M_AssertAlways(buf->cap >= buf->used);
  M_AssertAlways(count <= (buf->cap - buf->used));
  I16 *res = buf->vals + buf->used;
  buf->used += count;
  return res;
}
