KEY_Code :: enum u32
{
  using,except(COUNT) SDL_Scancode;
  START :: SDL_SCANCODE_COUNT + 1;
  MouseLeft;
  MouseRight;
  COUNT;

  // @todo write metaprogram that imports SDL_Scancodes but removes SDL_SCANCODE_...
  // W :: SDL_SCANCODE_W;
  // S :: SDL_SCANCODE_S;
  // A :: SDL_SCANCODE_A;
  // D :: SDL_SCANCODE_D;
};

KEY_Flags :: enum_flags u8
{
  HELD;
  PRESSED;
  RELEASED;
  // @todo DOUBLE_PRESSED, TRIPLE_PRESSED
}

Key :: struct
{
  f: KEY_Flags;
};

KEY_Update :: (key_code: KEY_Code, is_down: bool)
{
  using KEY_Flags;

  key := *G.keys[key_code];
  is_held     := !!(key.f & HELD);
  is_pressed  := !!(key.f & PRESSED);
  is_released := !!(key.f & RELEASED);

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
  if is_held      key.f |= HELD;
  if is_pressed   key.f |= PRESSED;
  if is_released  key.f |= RELEASED;
}

KEY_Check :: (flags: KEY_Flags, key_code: KEY_Code) -> bool
{
  return !!(G.keys[key_code].f & flags);
}
KEY_CheckAny :: (flags: KEY_Flags, key_codes: ..KEY_Code) -> bool
{
  res: bool;
  for key_codes
    res |= KEY_Check(flags, it);
  return res;
}
KEY_Held     :: (key_codes: ..KEY_Code) -> bool { return KEY_CheckAny(.HELD, ..key_codes); }
KEY_Pressed  :: (key_codes: ..KEY_Code) -> bool { return KEY_CheckAny(.PRESSED, ..key_codes); }
KEY_Released :: (key_codes: ..KEY_Code) -> bool { return KEY_CheckAny(.RELEASED, ..key_codes); }
