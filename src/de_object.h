typedef enum
{
    ObjectFlag_Draw          = (1 << 0),
    ObjectFlag_Move          = (1 << 1),
    ObjectFlag_Collide       = (1 << 2),
} Object_Flags;

typedef struct
{
    Uint32 flags;
    bool init;
    V2 p; // position of center
    V2 dp; // change of p
    V2 prev_p; // position from the last frame

    // visuals
    Uint32 sprite_id;
    ColorF sprite_color;

    float sprite_animation_t;
    Uint32 sprite_animation_index;
    Uint32 sprite_frame_index;

    Uint32 some_number;

    // temp
    bool has_collision;
} Object;

static Object Object_Lerp(Object prev, Object next, float t);
