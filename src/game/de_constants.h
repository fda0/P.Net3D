// App constants
#define TICK_RATE 128
#define TIME_STEP (1.f / (float)TICK_RATE)

// App forward declares
struct AppState;
typedef struct AppState AppState;

#if 1
#define MODEL_VERTEX_ARR Model_teapot_vrt
#define MODEL_INDEX_ARR Model_teapot_ind
#else
#define MODEL_VERTEX_ARR Model_flag_vrt
#define MODEL_INDEX_ARR Model_flag_ind
#endif
