static void KEY_Update(U32 key_code, bool is_held_down)
{
  if (key_code >= ArrayCount(APP.keys))
  {
    Assert(false);
    return;
  }

  Key *key = APP.keys + key_code;
  if (is_held_down)
  {
    key->pressed = (!key->held && !key->pressed);
    key->released = false;
  }
  else
  {
    key->released = (!key->held && !key->released);
    key->pressed = false;
  }
  key->held = is_held_down;
}

typedef enum
{
  KEY_IsHeld,
  KEY_IsPressed,
  KEY_IsReleased,
} KeyCheck;

static bool KEY_Check(U32 key_code, KeyCheck check)
{
  if (key_code >= ArrayCount(APP.keys))
  {
    Assert(false);
    return false;
  }

  Key key = APP.keys[key_code];
  if (check == KEY_IsHeld) return key.held;
  if (check == KEY_IsPressed) return key.pressed;
  if (check == KEY_IsReleased) return key.released;

  Assert(false);
  return false;
}

static bool KEY_Held(U32 key_code)
{
  return KEY_Check(key_code, KEY_IsHeld);
}

static bool KEY_Pressed(U32 key_code)
{
  return KEY_Check(key_code, KEY_IsPressed);
}

static bool KEY_Released(U32 key_code)
{
  return KEY_Check(key_code, KEY_IsReleased);
}
