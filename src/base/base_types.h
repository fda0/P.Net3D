#include <stdint.h>
#include <stdbool.h>

// ---
// Preprocessor switches
// ---

// detect compiler
#ifdef _MSC_VER
    #define COMPILER_MSVC 1
#endif
#ifdef __clang__
    #define COMPILER_CLANG 1
#endif

// detect OS
#ifdef _WIN32
     #define OS_WINDOWS 1
#endif
#ifdef __linux__
     #define OS_LINUX 1
#endif

// set undef compiler vars to 0
#ifndef COMPILER_MSVC
    #define COMPILER_MSVC 0
#endif
#ifndef COMPILER_CLANG
    #define COMPILER_CLANG 0
#endif
#if COMPILER_MSVC == COMPILER_CLANG
    #error "COMPILER_MSVC is equal to COMPILER_CLANG"
#endif

// set undef OS vars to 0
#ifndef OS_WINDOWS
     #define OS_WINDOWS 0
#endif
#ifndef OS_LINUX
    #define OS_LINUX 0
#endif
#if OS_WINDOWS == OS_LINUX
    #error "OS_WINDOWS is equal to OS_LINUX"
#endif

// Read only memory attribute - taken from EpicGamesExt/raddebugger
#if COMPILER_MSVC || (COMPILER_CLANG && OS_WIN)
#pragma section(".rdata$", read)
#define READ_ONLY __declspec(allocate(".rdata$"))
#elif (COMPILER_CLANG && OS_LIN)
#define READ_ONLY __attribute__((section(".rodata")))
#else
// NOTE(rjf): I don't know of a useful way to do this in GCC land.
// __attribute__((section(".rodata"))) looked promising, but it introduces a
// strange warning about malformed section attributes, and it doesn't look
// like writing to that section reliably produces access violations, strangely
// enough. (It does on Clang)
#define READ_ONLY
#endif

// ---
// Types
// ---
typedef uint8_t U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;
typedef int8_t I8;
typedef int16_t I16;
typedef int32_t I32;
typedef int64_t I64;

// ---
// Macros
// ---
// Sizes
#define Kilobyte(X) ((X)*1024)
#define Megabyte(X) (Kilobyte(X)*1024)
#define Gigabyte(X) (Megabyte(X)*1024)

// Loop macros
#define ArrayCount(Array) (sizeof(Array)/sizeof(Array[0]))
#define ForU64(I, Size) for (U32 I = 0; I < (Size); I += 1)
#define ForU32(I, Size) for (U32 I = 0; I < (Size); I += 1)
#define ForU16(I, Size) for (U16 I = 0; I < (Size); I += 1)
#define ForArray(I, Array) ForU64(I, ArrayCount(Array))

// Asserts
#define Assert(Expr) SDL_assert(Expr) // @todo(mg) this might be compiled out for release builds
#define AssertBounds(Index, Array) Assert((Index) < ArrayCount((Array)))

// Other helpers
#define Swap(a, b) do { typeof(a) tmp = a; a = b; b = tmp; } while (0)
#define ModuloPow2(X, Y) ((X) & ((Y)-1))
