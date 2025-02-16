typedef struct
{
  void *vals;
  U64 used; // in elements
  U64 cap; // in elements
  U32 elem_size; // in bytes
} M_Buffer;

static M_Buffer M_BufferAlloc(Arena *a, U64 count, U32 elem_size)
{
  M_Buffer buf = {};
  buf.cap = count;
  buf.elem_size = elem_size;
  buf.vals = Arena_AllocateBytes(a, count*elem_size, elem_size, false);
  return buf;
}
static U32 *M_BufferPushU32(M_Buffer *buf, U64 count)
{
  M_AssertAlways(buf->elem_size == sizeof(U32));
  M_AssertAlways(buf->cap >= buf->used);
  M_AssertAlways(count <= (buf->cap - buf->used));
  U32 *res = (U32 *)buf->vals + buf->used;
  buf->used += count;
  return res;
}
static U16 *M_BufferPushU16(M_Buffer *buf, U64 count)
{
  M_AssertAlways(buf->elem_size == sizeof(U16));
  M_AssertAlways(buf->cap >= buf->used);
  M_AssertAlways(count <= (buf->cap - buf->used));
  U16 *res = (U16 *)buf->vals + buf->used;
  buf->used += count;
  return res;
}
static U8 *M_BufferPushU8(M_Buffer *buf, U64 count)
{
  M_AssertAlways(buf->elem_size == sizeof(U8));
  M_AssertAlways(buf->cap >= buf->used);
  M_AssertAlways(count <= (buf->cap - buf->used));
  U8 *res = (U8 *)buf->vals + buf->used;
  buf->used += count;
  return res;
}
static float *M_BufferPushFloat(M_Buffer *buf, U64 count)
{
  return (float *)M_BufferPushU32(buf, count);
}

static U32 *M_BufferAtU32(M_Buffer *buf, U64 index)
{
  M_AssertAlways(buf->elem_size == sizeof(U32));
  M_AssertAlways(buf->used > index);
  return (U32 *)buf->vals + index;
}
static U16 *M_BufferAtU16(M_Buffer *buf, U64 index)
{
  M_AssertAlways(buf->elem_size == sizeof(U16));
  M_AssertAlways(buf->used > index);
  return (U16 *)buf->vals + index;
}
static U8 *M_BufferAtU8(M_Buffer *buf, U64 index)
{
  M_AssertAlways(buf->elem_size == sizeof(U8));
  M_AssertAlways(buf->used > index);
  return (U8 *)buf->vals + index;
}
static float *M_BufferAtFloat(M_Buffer *buf, U64 index)
{
  return (float *)M_BufferAtU32(buf, index);
}
