KEY_Code :: enum u32
{
  START :: SDL_Scancode.SDL_SCANCODE_COUNT + 1;
  MouseLeft;
  MouseRight;
  COUNT;
};

KEY_Flags :: enum_flags u32
{
  IsHeld;
  IsPressed;
  IsReleased;
}

Key :: struct
{
  f: KEY_Flags;
};

KEY_Update :: (key_code: KEY_Code, is_down: bool)
{
  using KEY_Flags;

  key := *G.keys[key_code];
  is_held     := !!(key.f & IsHeld);
  is_pressed  := !!(key.f & IsPressed);
  is_released := !!(key.f & IsReleased);

  if is_down
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

  key.f = 0;
  if is_held      key.f |= IsHeld;
  if is_pressed   key.f |= IsPressed;
  if is_released  key.f |= IsReleased;
}

KEY_Check :: (key_code: KEY_Code, check_flags: KEY_Flags) -> bool
{
  return !!(G.keys[key_code].f & check_flags);
}

KEY_Held :: (key_code: KEY_Code) -> bool
{
  return KEY_Check(key_code, .IsHeld);
}

KEY_Pressed :: (key_code: KEY_Code) -> bool
{
  return KEY_Check(key_code, .IsPressed);
}

KEY_Released :: (key_code: KEY_Code) -> bool
{
  return KEY_Check(key_code, .IsReleased);
}
