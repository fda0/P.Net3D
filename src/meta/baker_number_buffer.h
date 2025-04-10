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

#define M_BufferPushImpl(TYPE) \
M_Check(buf->elem_size == sizeof(TYPE)); \
M_Check(buf->cap >= buf->used); \
M_Check(count <= (buf->cap - buf->used)); \
TYPE *res = (TYPE *)buf->vals + buf->used; \
buf->used += count; \
return res;

#define M_BufferAtImpl(TYPE) \
M_Check(buf->elem_size == sizeof(TYPE)); \
M_Check(buf->used > index); \
return (TYPE *)buf->vals + index;

static U32   *M_BufferPushU32(M_Buffer *buf, U64 count) { M_BufferPushImpl(U32); }
static U16   *M_BufferPushU16(M_Buffer *buf, U64 count) { M_BufferPushImpl(U16); }
static U8    *M_BufferPushU8(M_Buffer *buf, U64 count) { M_BufferPushImpl(U8); }
static float *M_BufferPushFloat(M_Buffer *buf, U64 count) { M_BufferPushImpl(float); }
static S8    *M_BufferPushS8(M_Buffer *buf, U64 count) { M_BufferPushImpl(S8); }
static V3    *M_BufferPushV3(M_Buffer *buf, U64 count) { M_BufferPushImpl(V3); }
static V4    *M_BufferPushV4(M_Buffer *buf, U64 count) { M_BufferPushImpl(V4); }

static U32   *M_BufferAtU32(M_Buffer *buf, U64 index) { M_BufferAtImpl(U32); }
static U16   *M_BufferAtU16(M_Buffer *buf, U64 index) { M_BufferAtImpl(U16); }
static U8    *M_BufferAtU8(M_Buffer *buf, U64 index) { M_BufferAtImpl(U8); }
static float *M_BufferAtFloat(M_Buffer *buf, U64 index) { M_BufferAtImpl(float); }
static S8    *M_BufferAtS8(M_Buffer *buf, U64 index) { M_BufferAtImpl(S8); }
static V3    *M_BufferAtV3(M_Buffer *buf, U64 index) { M_BufferAtImpl(V3); }
static V4    *M_BufferAtV4(M_Buffer *buf, U64 index) { M_BufferAtImpl(V4); }
