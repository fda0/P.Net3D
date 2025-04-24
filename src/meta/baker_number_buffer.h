typedef struct
{
  void *vals;
  U64 used; // in elements
  U64 cap; // in elements
  U32 elem_size; // in bytes
} BK_Buffer;

static BK_Buffer BK_BufferAlloc(Arena *a, U64 count, U32 elem_size)
{
  BK_Buffer buf = {};
  buf.cap = count;
  buf.elem_size = elem_size;
  buf.vals = Arena_AllocateBytes(a, count*elem_size, elem_size, false);
  return buf;
}

static void *BK_BufferPush(BK_Buffer *buf, U64 count)
{
  M_Check(buf->cap >= buf->used);
  M_Check(count <= (buf->cap - buf->used));
  U8 *res = (U8 *)buf->vals + (buf->used * buf->elem_size);
  buf->used += count;
  return res;
}

static void *BK_BufferAt(BK_Buffer *buf, U64 index)
{
  M_Check(buf->used > index);
  return (U8 *)buf->vals + (index * buf->elem_size);
}

#define BK_BufferPushImpl(TYPE) \
M_Check(buf->elem_size == sizeof(TYPE)); \
return (TYPE *)BK_BufferPush(buf, count)

#define BK_BufferAtImpl(TYPE) \
M_Check(buf->elem_size == sizeof(TYPE)); \
return (TYPE *)BK_BufferAt(buf, index)

static U32   *BK_BufferPushU32(BK_Buffer *buf, U64 count) { BK_BufferPushImpl(U32); }
static U16   *BK_BufferPushU16(BK_Buffer *buf, U64 count) { BK_BufferPushImpl(U16); }
static U8    *BK_BufferPushU8(BK_Buffer *buf, U64 count) { BK_BufferPushImpl(U8); }
static float *BK_BufferPushFloat(BK_Buffer *buf, U64 count) { BK_BufferPushImpl(float); }
static S8    *BK_BufferPushS8(BK_Buffer *buf, U64 count) { BK_BufferPushImpl(S8); }
static V3    *BK_BufferPushV3(BK_Buffer *buf, U64 count) { BK_BufferPushImpl(V3); }
static V4    *BK_BufferPushV4(BK_Buffer *buf, U64 count) { BK_BufferPushImpl(V4); }

static U32   *BK_BufferAtU32(BK_Buffer *buf, U64 index) { BK_BufferAtImpl(U32); }
static U16   *BK_BufferAtU16(BK_Buffer *buf, U64 index) { BK_BufferAtImpl(U16); }
static U8    *BK_BufferAtU8(BK_Buffer *buf, U64 index) { BK_BufferAtImpl(U8); }
static float *BK_BufferAtFloat(BK_Buffer *buf, U64 index) { BK_BufferAtImpl(float); }
static S8    *BK_BufferAtS8(BK_Buffer *buf, U64 index) { BK_BufferAtImpl(S8); }
static V3    *BK_BufferAtV3(BK_Buffer *buf, U64 index) { BK_BufferAtImpl(V3); }
static V4    *BK_BufferAtV4(BK_Buffer *buf, U64 index) { BK_BufferAtImpl(V4); }
