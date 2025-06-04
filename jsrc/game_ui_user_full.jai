UI_RED1    :: Color32_RGBi(0xcc, 0x24, 0x1d);
UI_RED2    :: Color32_RGBi(0xfb, 0x49, 0x34);
UI_GREEN1  :: Color32_RGBi(0x98, 0x97, 0x1a);
UI_GREEN2  :: Color32_RGBi(0xb8, 0xbb, 0x26);
UI_YELLOW1 :: Color32_RGBi(0xd7, 0x99, 0x21);
UI_YELLOW2 :: Color32_RGBi(0xfa, 0xbd, 0x2f);
UI_BLUE1   :: Color32_RGBi(0x45, 0x85, 0x88);
UI_BLUE2   :: Color32_RGBi(0x83, 0xa5, 0x98);
UI_ORANGE1 :: Color32_RGBi(0xd6, 0x5d, 0x0e);
UI_ORANGE2 :: Color32_RGBi(0xfe, 0x80, 0x19);

static FONT_Type UI_FONT = FONT_Regular;

static Clay_Color UI_BG = {40, 40, 40, 255};
static Clay_Color UI_FG = {235, 219, 178, 255};
static Clay_Color UI_BORDER_BG = {29, 32, 33, 255};

static Clay_Color UI_SHADOW = {29, 32, 33, 96};
static Clay_Color UI_DARKER = {0, 0, 0, 64};

static Clay_Color UI_BTN_BG = {80, 73, 69, 255};
static Clay_Color UI_BTN_HOVER_BG = {102, 92, 84, 255};
static Clay_BorderWidth UI_CHECKBOX_BORDER_WIDTH = CLAY_BORDER_OUTSIDE(1);
static Clay_CornerRadius UI_RADIUS = {3,3,3,3};
static Clay_BorderWidth UI_BORDER_WIDTH = CLAY_BORDER_OUTSIDE(1);
static float UI_CHECKBOX_DIM = 8;
#define UI_WINDOW_GAP 4*G.font.scale
#define UI_WINDOW_PAD {UI_WINDOW_GAP, UI_WINDOW_GAP, UI_WINDOW_GAP, UI_WINDOW_GAP}
#define UI_BAR_PAD {0.5f*UI_WINDOW_GAP, 0.5f*UI_WINDOW_GAP, 0.5f*UI_WINDOW_GAP, 0.5f*UI_WINDOW_GAP}
#define UI_HEADER_PAD {0, 0, 0, UI_WINDOW_GAP}
#define UI_BTN_PAD {UI_WINDOW_GAP, UI_WINDOW_GAP, 0.5f*UI_WINDOW_GAP, 0.5f*UI_WINDOW_GAP}
#define UI_TEX_PAD {UI_WINDOW_GAP, UI_WINDOW_GAP, 0.5f*UI_WINDOW_GAP, 2*UI_WINDOW_GAP}

#define CLAY_SIZING_SCALED(ScaledPx) CLAY_SIZING_FIXED(G.font.scale*(ScaledPx))

UI_State :: struct
{
  dev: struct
  {
    show: bool;
    win_p: V2;
    win_dim: V2;
  };
};

UI_SliderConfig :: struct
{
  float *ptr;
  float min, max;
  U32 index; // temp workaround because of how clay.h works
};

UI_Checkbox :: (Clay_String label, bool in_horizontal_bar, bool *checkbox_bool)
{
  Clay_Sizing root_sizing = {};
  if (in_horizontal_bar) root_sizing.height = CLAY_SIZING_GROW(0);
  else                   root_sizing.width  = CLAY_SIZING_GROW(0);

  CLAY({.layout = {.sizing = root_sizing,
                   .padding = UI_BTN_PAD,
                   .childAlignment = {.y = CLAY_ALIGN_Y_CENTER},
                   .childGap = UI_WINDOW_GAP},
        .backgroundColor = Clay_Hovered() ? UI_BTN_HOVER_BG : UI_BTN_BG,
        .cornerRadius = UI_RADIUS})
  {
    if (Clay_Hovered() && KEY_Pressed(KEY_MouseLeft) && !G.debug.click_id)
    {
      *checkbox_bool = !(*checkbox_bool);
      G.debug.click_id = 1;
    }

    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_SCALED(UI_CHECKBOX_DIM),
                                .height = CLAY_SIZING_SCALED(UI_CHECKBOX_DIM)},
                     .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = (*checkbox_bool ? UI_YELLOW1 : UI_BTN_BG),
          .border = {.color = UI_FG,
                     .width = UI_CHECKBOX_BORDER_WIDTH},
          .cornerRadius = UI_RADIUS});
    CLAY_TEXT(label, CLAY_TEXT_CONFIG({.fontId = UI_FONT, .textColor = UI_FG}));
  }
}

UI_Button :: (Clay_String label, bool in_horizontal_bar, U32 *target, U32 value)
{
  Clay_Sizing root_sizing = {.width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT()};
  //Clay_Sizing root_sizing = {.height = CLAY_SIZING_SCALED(35)};
  if (in_horizontal_bar) root_sizing.height = CLAY_SIZING_GROW();
  else                   root_sizing.width  = CLAY_SIZING_GROW();

  CLAY({.layout = {.sizing = root_sizing,
                   .padding = UI_BTN_PAD,
                   .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}
                   //.childAlignment = {.x = CLAY_ALIGN_X_RIGHT, .y = CLAY_ALIGN_Y_BOTTOM}
                   },
        .backgroundColor = Clay_Hovered() ? UI_BTN_HOVER_BG : UI_BTN_BG,
        .cornerRadius = UI_RADIUS})
  {
    if (Clay_Hovered() && KEY_Pressed(KEY_MouseLeft) && !G.debug.click_id)
    {
      *target = value;
      G.debug.click_id = 1;
    }

    //CLAY({.layout = {.sizing = {CLAY_SIZING_FIT(), CLAY_SIZING_FIT()}},
          //.backgroundColor = UI_RED2})
    {
      CLAY_TEXT(label, CLAY_TEXT_CONFIG({.fontId = UI_FONT, .textColor = UI_FG}));
    }
  }
}

UI_Slider :: (Clay_String label, UI_SliderConfig config)
{
  CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW()},
                   .childGap = UI_WINDOW_GAP}})
  {
    if (config.min > config.max)
    {
      float tmp = config.min;
      config.min = config.max;
      config.max = tmp;
    }
    float range = config.max - config.min;
    if (!range)
    {
      range += 1;
      config.max += 1;
    }

    float width = 100;
    float value = (config.ptr ? *config.ptr : 0.f);
    float share = value / range;
    float filled_width = width * share;
    filled_width = Clamp(0, width, filled_width);

    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_SCALED(width),
                                .height = CLAY_SIZING_GROW()}},
          .backgroundColor = UI_YELLOW1,
          .cornerRadius = UI_RADIUS})
    {
      CLAY({.layout = {.sizing = {.width = CLAY_SIZING_SCALED(filled_width),
                                  .height = CLAY_SIZING_GROW()}},
            .backgroundColor = UI_YELLOW2,
            .cornerRadius = UI_RADIUS,
            .floating = {.attachTo = CLAY_ATTACH_TO_PARENT}
            });


      Clay_ElementId overlay_id = Clay__HashString(label, config.index, (U32)S8_Hash(S8Lit("RenderSliderOverlay")));
      CLAY({.id = overlay_id,
            .layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                  .height = CLAY_SIZING_GROW()},
                       .childAlignment = {.x = CLAY_ALIGN_X_CENTER,
                                          .y = CLAY_ALIGN_Y_CENTER}},
            .floating = {.attachTo = CLAY_ATTACH_TO_PARENT}})
      {
        if (Clay_Hovered() && KEY_Pressed(KEY_MouseLeft) && !G.debug.click_id)
          G.debug.click_id = overlay_id.id;
        if (G.debug.click_id == overlay_id.id)
        {
          Clay_ElementData overlay_data = Clay_GetElementData(overlay_id);
          V2 overlay_p = (V2){overlay_data.boundingBox.x, overlay_data.boundingBox.y};
          V2 overlay_dim = (V2){overlay_data.boundingBox.width, overlay_data.boundingBox.height};

          V2 rel_mouse = V2_Sub(G.mouse, overlay_p);
          V2 clamped_mouse = V2_Clamp((V2){}, overlay_dim, rel_mouse);

          if (overlay_dim.x)
          {
            float value_x = (clamped_mouse.x / overlay_dim.x) * range;
            if (config.ptr) *config.ptr = value_x;
          }
        }

        if (G.debug.click_id == overlay_id.id ||
            (Clay_Hovered() && !G.debug.click_id))
        {
          if (KEY_Pressed(SDL_SCANCODE_R)) // Round
            if (config.ptr) *config.ptr = FRound(*config.ptr);
          if (KEY_Pressed(SDL_SCANCODE_C)) // Clamp
            if (config.ptr) *config.ptr = Clamp(config.min, config.max, *config.ptr);
          if (KEY_Pressed(SDL_SCANCODE_H)) // Half
            if (config.ptr) *config.ptr = (config.min + config.max) * 0.5f;
          if (KEY_Pressed(SDL_SCANCODE_RIGHT))
            if (config.ptr) *config.ptr = *config.ptr + 1;
          if (KEY_Pressed(SDL_SCANCODE_LEFT))
            if (config.ptr) *config.ptr = *config.ptr - 1;
          if (KEY_Pressed(SDL_SCANCODE_UP))
            if (config.ptr) *config.ptr = *config.ptr + 10;
          if (KEY_Pressed(SDL_SCANCODE_DOWN))
            if (config.ptr) *config.ptr = *config.ptr - 10;
        }

        Printer p = Pr_Alloc(G.a_frame, 32);
#if 1
        Pr_Float(&p, value);
#else
        I32 before_period = FRound(share * 100);
        I32 after_period = ((I32)FAbs(FRound(share * 10000))) % 100;
        Pr_I32(&p, before_period);
        Pr_S8(&p, S8Lit("."));
        Pr_I32(&p, after_period);
        Pr_S8(&p, S8Lit("%"));
#endif
        CLAY_TEXT(ClayString_FromS8(Pr_AsS8(&p)),
                  CLAY_TEXT_CONFIG({.fontId = UI_FONT, .textColor = UI_BG}));
      }
    }

    CLAY_TEXT(label,
              CLAY_TEXT_CONFIG({.fontId = UI_FONT, .textColor = UI_FG}));
  }
}

UI_Header :: (Clay_String label)
{
  CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                              .height = CLAY_SIZING_FIT()},
                   .padding = UI_HEADER_PAD}})
  {
    CLAY_TEXT(label, CLAY_TEXT_CONFIG({.fontId = FONT_Header, .textColor = UI_FG}));
  }
}

UI_BuildDeveloperWindow :: ()
{
  using G.ui.dev;
  if (!show) return;

  clamped_win_p := clamp(V2.{}, G.window_dim * 0.9, win_p);


  {
    UIC_Parent(UIC_Box(.{layout = .{child_layout = .TOP_TO_BOTTOM,
                                    sizing = .{width = UIC_Rem(200, fit_children = true),
                                               height = UIC_Rem(200)}},
                         bg_color = UI_BG,
                         floating = .{attach_to = .ROOT, offset = clamped_win_p},
                         border = .{color = UI_SHADOW, width = UI_BORDER_WIDTH},
                         corner_radius = UI_RADIUS}))
    {
      window_bar := UIC_Box(.{layout = {sizing = .{width = UIC_Grow(), height = UIC_Fit()},
                                        padding = UI_BAR_PAD,
                                        child_align = .{x = .CENTER}},
                              bg_color = UI_ORANGE2,
                              corner_radius = UI_RADIUS})
      UIC_Parent(window_bar);

      if window_bar.f & .DRAG  G.debug.win_p += G.mouse_delta;
      else                     G.debug.win_p = clamped_win_p;

      UIC_CreateText("Developer window", .{text = .{color = UI_BG}});
    }

    // Content of the window
    {
      UIC_Parent(UIC_Box(.{layout = .{sizing = .{width = UIC_Grow(), height = UIC_Grow()}}}))

      // Category selection bar
      CLAY(.{layout = {child_layout = .TOP_TO_BOTTOM,
                       sizing = .{width = UIC_Fit(),
                                  height = UIC_GROW()},
                       padding = UI_WINDOW_PAD,
                       childGap = UI_WINDOW_GAP},
             border = {color = UI_BORDER_BG,
                       width = UI_BORDER_WIDTH}})
      {
        UI_Button(CLAY_STRING("🕹️"), false, &G.debug.menu_category, 0);
        UI_Button(CLAY_STRING("🤔"), false, &G.debug.menu_category, 1);
      }

      // Menu content
      CLAY({.layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                       .sizing = {.width = CLAY_SIZING_GROW(),
                                  .height = CLAY_SIZING_FIT()},
                       .padding = UI_WINDOW_PAD},
            .cornerRadius = UI_RADIUS})
      {
        if (G.debug.menu_category == 0)
        {
          UI_Header(CLAY_STRING("Debug switches"));

          {
            Object *player = OBJ_Get(G.client.player_key, OBJ_Network);
            V3 rel_p = V3_Sub(G.camera_p, player.s.p);

            Pr_MakeOnStack(p, Kilobyte(1));
            Pr_Cstr(&p, "World camera p: ");
            Pr_V3(&p, G.camera_p);

            Pr_Cstr(&p, "\nPlayer camera p: ");
            Pr_V3(&p, rel_p);

            Pr_Cstr(&p, "\nCamera angles: ");
            Pr_V3(&p, G.camera_angles);

            UI_Label(Pr_AsS8(&p));
          }

          CLAY({.layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                           .sizing = {.width = CLAY_SIZING_GROW(),
                                      .height = CLAY_SIZING_FIT()},
                           .childGap = UI_WINDOW_GAP}})
          {
            UI_Checkbox(CLAY_STRING("📽️ Noclip camera"), false, &G.debug.noclip_camera);
            UI_Checkbox(CLAY_STRING("☀️ Sun camera"), false, &G.debug.sun_camera);
            UI_Checkbox(CLAY_STRING("📦 Draw model collision"), false, &G.debug.draw_model_collision);
            bool todo = false;
            UI_Checkbox(CLAY_STRING("🌌 [todo] Render normals"), false, &todo);
          }
        }
        else if (G.debug.menu_category == 1)
        {
          UI_Header(CLAY_STRING("Nothing here yet"));
        }
      }
    }

    // Capture uncaptured mouse click
    if (Clay_Hovered() && KEY_Pressed(KEY_MouseLeft) && !G.debug.click_id)
      G.debug.click_id = 1;
  }
}
