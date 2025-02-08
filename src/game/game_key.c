static void Key_Update(U32 key_code, bool is_held_down)
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
    Key_IsHeld,
    Key_IsPressed,
    Key_IsReleased,
} KeyCheck;

static bool Key_Check(U32 key_code, KeyCheck check)
{
    if (key_code >= ArrayCount(APP.keys))
    {
        Assert(false);
        return false;
    }

    Key key = APP.keys[key_code];
    if (check == Key_IsHeld) return key.held;
    if (check == Key_IsPressed) return key.pressed;
    if (check == Key_IsReleased) return key.released;

    Assert(false);
    return false;
}

static bool Key_Held(U32 key_code)
{
    return Key_Check(key_code, Key_IsHeld);
}

static bool Key_Pressed(U32 key_code)
{
    return Key_Check(key_code, Key_IsPressed);
}

static bool Key_Released(U32 key_code)
{
    return Key_Check(key_code, Key_IsReleased);
}
