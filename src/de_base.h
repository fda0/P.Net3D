#define ArrayCount(Array) (sizeof(Array)/sizeof(Array[0]))
#define ForU32(I, Size) for (Uint32 I = 0; I < (Size); I += 1)
#define ForU64(I, Size) for (Uint32 I = 0; I < (Size); I += 1)
#define ForArray(I, Array) ForU64(I, ArrayCount(Array))
#define Swap(a, b) do { typeof(a) tmp = a; a = b; b = tmp; } while (0)

#define Assert(Expr) SDL_assert(Expr) // @todo(mg) this might be compiled out for release builds

struct AppState;
typedef struct AppState AppState;

#define TICK_RATE 128
#define TIME_STEP (1.f / (float)TICK_RATE)
