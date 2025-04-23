static Clay_Color UI_red1 = {0xcc, 0x24, 0x1d, 255};
static Clay_Color UI_red2 = {0xfb, 0x49, 0x34, 255};
static Clay_Color UI_green1 = {0x98, 0x97, 0x1a, 255};
static Clay_Color UI_green2 = {0xb8, 0xbb, 0x26, 255};
static Clay_Color UI_yellow1 = {0xd7, 0x99, 0x21, 255};
static Clay_Color UI_yellow2 = {0xfa, 0xbd, 0x2f, 255};
static Clay_Color UI_blue1 = {0x45, 0x85, 0x88, 255};
static Clay_Color UI_blue2 = {0x83, 0xa5, 0x98, 255};
static Clay_Color UI_orange1 = {0xd6, 0x5d, 0x0e, 255};
static Clay_Color UI_orange2 = {0xfe, 0x80, 0x19, 255};

static FONT_Type UI_font = FONT_Regular;

static Clay_Color UI_bg = {40, 40, 40, 255};
static Clay_Color UI_fg = {235, 219, 178, 255};
static Clay_Color UI_border_bg = {29, 32, 33, 255};

static Clay_Color UI_shadow = {29, 32, 33, 96};
static Clay_Color UI_darker = {0, 0, 0, 64};

static Clay_Color UI_btn_bg = {80, 73, 69, 255};
static Clay_Color UI_btn_hover_bg = {102, 92, 84, 255};
static Clay_BorderWidth UI_checkbox_border_width = CLAY_BORDER_OUTSIDE(1);
static Clay_CornerRadius UI_radius = {3,3,3,3};
static Clay_BorderWidth UI_border_width = CLAY_BORDER_OUTSIDE(1);
static float UI_checkbox_dim = 8;
#define UI_window_gap 4*APP.font.scale
#define UI_window_pad {UI_window_gap, UI_window_gap, UI_window_gap, UI_window_gap}
#define UI_bar_pad {0.5f*UI_window_gap, 0.5f*UI_window_gap, 0.5f*UI_window_gap, 0.5f*UI_window_gap}
#define UI_header_pad {0, 0, 0, UI_window_gap}
#define UI_btn_pad {UI_window_gap, UI_window_gap, 0.5f*UI_window_gap, 0.5f*UI_window_gap}
#define UI_tex_pad {UI_window_gap, UI_window_gap, 0.5f*UI_window_gap, 2*UI_window_gap}

#define CLAY_SIZING_SCALED(ScaledPx) CLAY_SIZING_FIXED(APP.font.scale*(ScaledPx))

typedef struct
{
  float min, max;
  float *ptr;
  U32 index; // temp workaround because of how clay.h works
} UI_SliderConfig;

static void UI_RenderCheckbox(Clay_String label, bool in_horizontal_bar, bool *checkbox_bool)
{
  Clay_Sizing root_sizing = {};
  if (in_horizontal_bar) root_sizing.height = CLAY_SIZING_GROW(0);
  else                   root_sizing.width  = CLAY_SIZING_GROW(0);

  CLAY({.layout = {.sizing = root_sizing,
                   .padding = UI_btn_pad,
                   .childAlignment = {.y = CLAY_ALIGN_Y_CENTER},
                   .childGap = UI_window_gap},
        .backgroundColor = Clay_Hovered() ? UI_btn_hover_bg : UI_btn_bg,
        .cornerRadius = UI_radius})
  {
    if (Clay_Hovered() && KEY_Pressed(KEY_MouseLeft) && !APP.debug.click_id)
    {
      *checkbox_bool = !(*checkbox_bool);
      APP.debug.click_id = 1;
    }

    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_SCALED(UI_checkbox_dim),
                                .height = CLAY_SIZING_SCALED(UI_checkbox_dim)},
                     .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = (*checkbox_bool ? UI_yellow1 : UI_btn_bg),
          .border = {.color = UI_fg,
                     .width = UI_checkbox_border_width},
          .cornerRadius = UI_radius});
    CLAY_TEXT(label, CLAY_TEXT_CONFIG({.fontId = UI_font, .textColor = UI_fg}));
  }
}

static void UI_RenderButton(Clay_String label, bool in_horizontal_bar, U32 *target, U32 value)
{
  Clay_Sizing root_sizing = {.width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT()};
  //Clay_Sizing root_sizing = {.height = CLAY_SIZING_SCALED(35)};
  if (in_horizontal_bar) root_sizing.height = CLAY_SIZING_GROW();
  else                   root_sizing.width  = CLAY_SIZING_GROW();

  CLAY({.layout = {.sizing = root_sizing,
                   .padding = UI_btn_pad,
                   .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}
                   //.childAlignment = {.x = CLAY_ALIGN_X_RIGHT, .y = CLAY_ALIGN_Y_BOTTOM}
                   },
        .backgroundColor = Clay_Hovered() ? UI_btn_hover_bg : UI_btn_bg,
        .cornerRadius = UI_radius})
  {
    if (Clay_Hovered() && KEY_Pressed(KEY_MouseLeft) && !APP.debug.click_id)
    {
      *target = value;
      APP.debug.click_id = 1;
    }

    //CLAY({.layout = {.sizing = {CLAY_SIZING_FIT(), CLAY_SIZING_FIT()}},
          //.backgroundColor = UI_red2})
    {
      CLAY_TEXT(label, CLAY_TEXT_CONFIG({.fontId = UI_font, .textColor = UI_fg}));
    }
  }
}



static void UI_RenderSlider(Clay_String label, UI_SliderConfig config)
{
  CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW()},
                   .childGap = UI_window_gap}})
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
          .backgroundColor = UI_yellow1,
          .cornerRadius = UI_radius})
    {
      CLAY({.layout = {.sizing = {.width = CLAY_SIZING_SCALED(filled_width),
                                  .height = CLAY_SIZING_GROW()}},
            .backgroundColor = UI_yellow2,
            .cornerRadius = UI_radius,
            .floating = {.attachTo = CLAY_ATTACH_TO_PARENT}
            });


      Clay_ElementId overlay_id = Clay__HashString(label, config.index, (U32)S8_Hash(0, S8Lit("RenderSliderOverlay")));
      CLAY({.id = overlay_id,
            .layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                  .height = CLAY_SIZING_GROW()},
                       .childAlignment = {.x = CLAY_ALIGN_X_CENTER,
                                          .y = CLAY_ALIGN_Y_CENTER}},
            .floating = {.attachTo = CLAY_ATTACH_TO_PARENT}})
      {
        if (Clay_Hovered() && KEY_Pressed(KEY_MouseLeft) && !APP.debug.click_id)
          APP.debug.click_id = overlay_id.id;
        if (APP.debug.click_id == overlay_id.id)
        {
          Clay_ElementData overlay_data = Clay_GetElementData(overlay_id);
          V2 overlay_p = (V2){overlay_data.boundingBox.x, overlay_data.boundingBox.y};
          V2 overlay_dim = (V2){overlay_data.boundingBox.width, overlay_data.boundingBox.height};

          V2 rel_mouse = V2_Sub(APP.mouse, overlay_p);
          V2 clamped_mouse = V2_Clamp((V2){}, overlay_dim, rel_mouse);

          if (overlay_dim.x)
          {
            float value_x = (clamped_mouse.x / overlay_dim.x) * range;
            if (config.ptr) *config.ptr = value_x;
          }
        }

        if (APP.debug.click_id == overlay_id.id ||
            (Clay_Hovered() && !APP.debug.click_id))
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

        Printer p = Pr_Alloc(APP.a_frame, 32);
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
                  CLAY_TEXT_CONFIG({.fontId = UI_font, .textColor = UI_bg}));
      }
    }

    CLAY_TEXT(label,
              CLAY_TEXT_CONFIG({.fontId = UI_font, .textColor = UI_fg}));
  }
}

static void UI_RenderHeader(Clay_String label)
{
  CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                              .height = CLAY_SIZING_FIT()},
                   .padding = UI_header_pad}})
  {
    CLAY_TEXT(label, CLAY_TEXT_CONFIG({.fontId = FONT_Header, .textColor = UI_fg}));
  }
}

static void UI_BuildUILayoutElements()
{
  V2 clamped_win_p = V2_Clamp((V2){}, V2_Scale(APP.window_dim, 0.9f), APP.debug.win_p);

  CLAY({.layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                   .sizing = {.width = CLAY_SIZING_FIT(APP.font.scale * 200),
                              .height = CLAY_SIZING_SCALED(200)}},
        .backgroundColor = UI_bg,
        .floating = {.offset = {clamped_win_p.x, clamped_win_p.y},
                     .attachTo = CLAY_ATTACH_TO_ROOT},
        .border = {.color = UI_shadow,
                   .width = UI_border_width},
        .cornerRadius = UI_radius,
        .userData = (void *)1 /* temporary hack: apply big edge smoothing */
        })
  {
    // Window bar
    Clay_ElementId window_bar_id = CLAY_ID("WindowBar");
    CLAY({.id = window_bar_id,
          .layout = {.sizing = {.width = CLAY_SIZING_GROW(0),
                                .height = CLAY_SIZING_FIT()},
                     .padding = UI_bar_pad,
                     .childAlignment = {.x = CLAY_ALIGN_X_CENTER}},
          .backgroundColor = UI_orange2,
          .cornerRadius = UI_radius})
    {
      if (Clay_Hovered() && KEY_Pressed(KEY_MouseLeft) && !APP.debug.click_id)
        APP.debug.click_id = window_bar_id.id;
      if (APP.debug.click_id == window_bar_id.id)
        APP.debug.win_p = V2_Add(APP.debug.win_p, APP.mouse_delta);
      else
        APP.debug.win_p = clamped_win_p;

      CLAY_TEXT(CLAY_STRING("Developer window"),
                CLAY_TEXT_CONFIG({.fontId = UI_font, .textColor = UI_bg}));
    }

    // Content of the window
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                .height = CLAY_SIZING_GROW()}}})
    {
      // Category selection bar
      CLAY({.layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                       .sizing = {.width = CLAY_SIZING_FIT(),
                                  .height = CLAY_SIZING_GROW()},
                       .padding = UI_window_pad,
                       .childGap = UI_window_gap},
            .border = {.color = UI_border_bg,
                       .width = UI_border_width}})
      {
        UI_RenderButton(CLAY_STRING("🕹️"), false, &APP.debug.menu_category, 0);
        UI_RenderButton(CLAY_STRING("🧱"), false, &APP.debug.menu_category, 1);
        UI_RenderButton(CLAY_STRING("🤔"), false, &APP.debug.menu_category, 2);
      }

      // Menu content
      CLAY({.layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                       .sizing = {.width = CLAY_SIZING_GROW(),
                                  .height = CLAY_SIZING_FIT()},
                       .padding = UI_window_pad},
            .cornerRadius = UI_radius})
      {
        if (APP.debug.menu_category == 0)
        {
          UI_RenderHeader(CLAY_STRING("Debug switches"));

          CLAY({.layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                           .sizing = {.width = CLAY_SIZING_GROW(),
                                      .height = CLAY_SIZING_FIT()},
                           .childGap = UI_window_gap}})
          {
            UI_RenderCheckbox(CLAY_STRING("📽️ Noclip camera"), false, &APP.debug.noclip_camera);
            UI_RenderCheckbox(CLAY_STRING("☀️ Sun camera"), false, &APP.debug.sun_camera);
            UI_RenderCheckbox(CLAY_STRING("📦 Draw collision box"), false, &APP.debug.draw_collision_box);
          }
        }
        else if (APP.debug.menu_category == 1)
        {
          UI_RenderHeader(CLAY_STRING("Material editor"));

          CLAY({.layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                           .sizing = {.width = CLAY_SIZING_GROW(),
                                      .height = CLAY_SIZING_FIT()}},
                .scroll = {.vertical = true}})
          {
            ForU32(tex_index, TEX_COUNT)
            {
              CLAY_TEXT(ClayString_FromS8(TEX_GetName(tex_index)),
                        CLAY_TEXT_CONFIG({.fontId = UI_font, .textColor = UI_fg}));

              CLAY({.layout = {.padding = UI_tex_pad}})
              {
                UI_RenderSlider(CLAY_STRING("Shininess"), (UI_SliderConfig)
                                {0, 64,
                                 &TEX_GetAsset(tex_index)->shininess,
                                 tex_index});
                //UI_RenderSlider(CLAY_STRING("Displacement"), (UI_SliderConfig){0, 16, &APP.debug.tex_displacement});
              }
            }
          }
        }
        else if (APP.debug.menu_category == 2)
        {
          UI_RenderHeader(CLAY_STRING("Nothing here yet"));
        }
      }
    }

    // Capture uncaptured mouse click
    if (Clay_Hovered() && KEY_Pressed(KEY_MouseLeft) && !APP.debug.click_id)
      APP.debug.click_id = 1;
  }
}

//
//
//
static void UI_LogError(Clay_ErrorData error)
{
  LOG(Log_Clay, "%s", error.errorText.chars);
}

static Clay_Dimensions UI_MeasureText(Clay_StringSlice clay_slice, Clay_TextElementConfig *config, void *user_data)
{
  (void)user_data;
  S8 string = S8_FromClaySlice(clay_slice);

  FONT_Type font_index = config->fontId;
  if (font_index < 0 || font_index >= FONT_COUNT) font_index = UI_font;
  TTF_Font *ttf_font = APP.font.ttfs[font_index][0];

  I32 width = 0, height = 0;
  if (!TTF_GetStringSize(ttf_font, (char *)string.str, string.size, &width, &height))
    LOG(Log_Clay, "Failed to measure text: %s", SDL_GetError());

  Clay_Dimensions result = {width, height};
  return result;
}

static void UI_Init()
{
  U64 memory_size = Clay_MinMemorySize();
  Clay_Arena clay_arena = (Clay_Arena)
  {
    .memory = SDL_malloc(memory_size),
    .capacity = memory_size
  };

  Clay_Initialize(clay_arena, (Clay_Dimensions){APP.window_dim.x, APP.window_dim.y}, (Clay_ErrorHandler){UI_LogError});
  Clay_SetMeasureTextFunction(UI_MeasureText, 0);
}

static void UI_ProcessWindowAndFontResize()
{
  if (APP.window_resized)
    Clay_SetLayoutDimensions((Clay_Dimensions){APP.window_dim.x, APP.window_dim.y});

  if (APP.font.scale_changed)
    Clay_ResetMeasureTextCache();
}

static void UI_StartFrame()
{
  if (!KEY_Held(KEY_MouseLeft) || KEY_Released(KEY_MouseLeft))
    APP.debug.click_id = 0;

  Clay_SetPointerState((Clay_Vector2){APP.mouse.x, APP.mouse.y}, KEY_Held(KEY_MouseLeft));
  Clay_UpdateScrollContainers(true, (Clay_Vector2){APP.mouse_scroll.x, APP.mouse_scroll.y}, APP.dt);
  Clay_BeginLayout();
}

static void UI_FinishFrame()
{
  Clay_RenderCommandArray render_commands = Clay_EndLayout();
  ForI32(i, render_commands.length)
  {
    Clay_RenderCommand *rcom = Clay_RenderCommandArray_Get(&render_commands, i);
    Clay_BoundingBox box = rcom->boundingBox;

    {
      box.x = FRound(box.x);
      box.y = FRound(box.y);
    }

    UI_GpuShape shape =
    {
      .p_min = (V2){box.x, box.y},
      .p_max = (V2){box.x + box.width, box.y + box.height},
      .tex_layer = -1.f,
    };
    bool skip_shape = false;

    switch (rcom->commandType)
    {
      case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
      {
        Clay_RectangleRenderData rect = rcom->renderData.rectangle;
        shape.color = Color32_ClayColor(rect.backgroundColor);
        shape.corner_radius = APP.font.scale * rect.cornerRadius.topLeft; // @todo support corner radius for each corner
      } break;

      case CLAY_RENDER_COMMAND_TYPE_BORDER:
      {
        Clay_BorderRenderData border = rcom->renderData.border;
        shape.color = Color32_ClayColor(border.color);
        shape.corner_radius = APP.font.scale * border.cornerRadius.topLeft; // @todo support corner radius for each corner
        shape.border_thickness = APP.font.scale * border.width.left; // @todo support border width per direction?

        if (rcom->userData == (void *)1)
        {
          // would be best to draw it in the background;
          // @todo add sorting in the future
          shape.corner_radius = 0;
          float expand = APP.font.scale * 10.f;
          V2 expand2 = (V2){expand, expand};
          shape.p_min = V2_Sub(shape.p_min, expand2);
          shape.p_max = V2_Add(shape.p_max, expand2);
          shape.edge_softness += expand;
        }
      } break;

      case CLAY_RENDER_COMMAND_TYPE_TEXT:
      {
        Clay_TextRenderData text = rcom->renderData.text;
        S8 string = S8_FromClaySlice(text.stringContents);

        FONT_Type font_index = text.fontId;
        if (font_index < 0 || font_index >= FONT_COUNT) font_index = UI_font;

        FONT_GlyphRun glyphs = FONT_GetGlyphRun(font_index, string);
        if (glyphs.hash)
        {
          shape.tex_min = V2_FromV2I16(glyphs.p);
          shape.tex_max = V2_FromV2I16(V2I16_Add(glyphs.p, glyphs.dim));
          shape.tex_layer = glyphs.layer;

          V2 dim = V2_Sub(shape.tex_max, shape.tex_min);
          shape.p_max = V2_Add(shape.p_min, dim);
        }

        //float real_width = shape.p_max.x - shape.p_min.x;
        //float tex_width = shape.tex_max.x - shape.tex_min.x;
        //LOG(Log_Clay, "real width: %f, tex width: %f", real_width, tex_width);

        shape.color = Color32_ClayColor(text.textColor);
        // @todo font id, letterSpacing, lineHeight etc
      } break;

      case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
      {
        skip_shape = true;
        UI_GpuClip clip =
        {
          shape.p_min,
          shape.p_max
        };
        UI_PushClip(clip);
      } break;

      case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
      {
        skip_shape = true;
        UI_PopClip();
      } break;

      default: skip_shape = true; break;
    }

    if (shape.corner_radius > 0.f)
      shape.edge_softness += APP.font.scale * 0.25f;

    UI_DrawRaw(shape);
  }
}
