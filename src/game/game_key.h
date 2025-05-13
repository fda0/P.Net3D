typedef enum
{
  KEY_START = SDL_SCANCODE_COUNT + 1,

  KEY_MouseLeft,
  KEY_MouseRight,

  KEY_COUNT
} KEY_Code;

typedef U8 KEY_Flags;
enum
{
  KEY_IsHeld     = (1 << 0),
  KEY_IsPressed  = (1 << 1), // was pressed on this frame
  KEY_IsReleased = (1 << 2), // released on this frame
};

typedef struct
{
  KEY_Flags f;
} Key;
