static void KEY_Update(U32 key_code, bool is_down)
{
  if (key_code >= ArrayCount(APP.keys))
  {
    Assert(false);
    return;
  }

  Key *key = APP.keys + key_code;
  bool is_held     = key->f & KEY_IsHeld;
  bool is_pressed  = key->f & KEY_IsPressed;
  bool is_released = key->f & KEY_IsReleased;

  if (is_down)
  {
    is_pressed = (!is_held && !is_pressed);
    is_released = false;
  }
  else
  {
    is_released = (!is_held && !is_released);
    is_pressed = false;
  }
  is_held = is_down;

  key->f = 0;
  key->f |= (is_held     ? KEY_IsHeld : 0);
  key->f |= (is_pressed  ? KEY_IsPressed : 0);
  key->f |= (is_released ? KEY_IsReleased : 0);
}

static bool KEY_Check(U32 key_code, KEY_Flags check_flags)
{
  if (key_code >= ArrayCount(APP.keys))
  {
    Assert(false);
    return false;
  }
  return !!(APP.keys[key_code].f & check_flags);
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
