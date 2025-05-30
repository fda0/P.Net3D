#define M_LOG_HEADER "[CODEGEN_MATH] "
#define BASE_SKIP_MATH
#include "meta_util.h"

// @todo implement Rng
// @todo implement Slice

int main()
{
  Arena *tmp = 0;
  {
    U64 memory_size = Megabyte(8);
    void *memory = malloc(memory_size);
    tmp = Arena_MakeInside(memory, memory_size);
  }

  S8 all_vectors = {};
  {
    S8 templ_vectors = M_LoadFile("../src/meta/codegen_math_vector_templ.h", true);

    struct
    {
      S8 scalar;
      bool no_postfix;
      bool is_unsigned;
      S8 result;
    } vectors[] =
    {
      {S8Lit("float"), .no_postfix = true},
      {S8Lit("I32")},
      {S8Lit("I16")},
      {S8Lit("U64"), .is_unsigned = true},
      {S8Lit("U32"), .is_unsigned = true},
    };

    U64 vectors_total_size = 0;
    ForArray(i, vectors)
    {
      S8 scalar = vectors[i].scalar;
      S8 postfix = (vectors[i].no_postfix ? (S8){} : scalar);

      vectors[i].result = templ_vectors;
      if (vectors[i].is_unsigned)
        S8_ConsumeChopUntil(&vectors[i].result, S8Lit("// [SIGNED_ONLY]"), 0);

      vectors[i].result = S8_ReplaceAll(tmp, vectors[i].result, S8Lit("SCALAR"), scalar, 0);
      vectors[i].result = S8_ReplaceAll(tmp, vectors[i].result, S8Lit("POST"), postfix, 0);

      vectors_total_size += vectors[i].result.size;
    }

    all_vectors = S8_Allocate(tmp, vectors_total_size);
    {
      U8 *dst = all_vectors.str;
      ForArray(i, vectors)
      {
        memcpy(dst, vectors[i].result.str, vectors[i].result.size);
        dst += vectors[i].result.size;
      }
    }
  }

  S8 header = S8Lit("// ===\n"
                    "// THIS FILE IS AUTOGENERATED BY codegen_math.c\n"
                    "// DO NOT EDIT MANUALLY\n"
                    "// ===\n\n");
  S8 final = S8_Concat2(tmp, header, all_vectors);
  M_SaveFile("../src/base/gen_vector_math.h", final);

  // exit
  M_LOG(M_Idk, "%s", (M_LogState.error_count > 0 ? "Fail" : "Success"));
  return M_LogState.error_count;
}
