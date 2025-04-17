static Clay_Color UI_bg = {40, 40, 40, 255};
static Clay_Color UI_fg = {235, 219, 178, 255};
static Clay_Color UI_bar_bg = {254, 128, 25, 255};
static Clay_Color UI_bar_fg = {40, 40, 40, 255};
static Clay_Color UI_border_bg = {29, 32, 33, 255};

static Clay_Padding UI_window_pad = {8,8,8,8};
static U16 UI_window_gap = 8;

static Clay_Color UI_btn_bg = {80, 73, 69, 255};
static Clay_Color UI_btn_hover_bg = {102, 92, 84, 255};
static Clay_Padding UI_btn_pad = {8, 8, 4, 4};

static Clay_BorderWidth UI_checkbox_border_width = CLAY_BORDER_OUTSIDE(1.5);

static Clay_CornerRadius UI_radius = {4,4,4,4};
static Clay_BorderWidth UI_border_width = CLAY_BORDER_OUTSIDE(1.5);

static void UI_HoverCallbackCheckbox(Clay_ElementId element_id, Clay_PointerData pointer_info, intptr_t user_data)
{
  (void)element_id;
  bool *checkbox_ptr = (bool *)user_data;
  if (pointer_info.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME)
  {
    *checkbox_ptr = !(*checkbox_ptr);
  }
}

static void UI_RenderCheckbox(FONT_Type font, Clay_String label, bool in_horizontal_bar, bool *checkbox_bool)
{
  Clay_Sizing root_sizing = {};
  if (in_horizontal_bar) root_sizing.height = CLAY_SIZING_GROW(0);
  else                   root_sizing.width  = CLAY_SIZING_GROW(0);

  CLAY({.layout = {.sizing = root_sizing,
                   .padding = UI_btn_pad,
                   .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
        .backgroundColor = Clay_Hovered() ? UI_btn_hover_bg : UI_btn_bg,
        .cornerRadius = UI_radius})
  {
    Clay_OnHover(UI_HoverCallbackCheckbox, (intptr_t)checkbox_bool);

    float checkbox_dim = 15 * APP.atlas.scale;
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_FIXED(checkbox_dim),
                                .height = CLAY_SIZING_FIXED(checkbox_dim)},
                     .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = Clay_Hovered() ? UI_btn_hover_bg : UI_btn_bg,
          .border = {.color = UI_fg,
                     .width = UI_checkbox_border_width},
          .cornerRadius = CLAY_CORNER_RADIUS(4)})
    {
      if (*checkbox_bool)
        CLAY_TEXT(CLAY_STRING("X"), CLAY_TEXT_CONFIG({.fontId = font, .textColor = UI_fg}));
    }
    CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(10)}}}) {}
    CLAY_TEXT(label, CLAY_TEXT_CONFIG({.fontId = font, .textColor = UI_fg}));
  }
}

static void UI_BuildUILayoutElements()
{
  V2 clamped_win_p = V2_Clamp((V2){}, V2_Scale(APP.window_dim, 0.9f), APP.debug.win_p);

  CLAY({.layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                   .sizing = {.width = CLAY_SIZING_FIT(),
                              .height = CLAY_SIZING_FIT()}},
        .backgroundColor = UI_bg,
        .floating = {.offset = {clamped_win_p.x, clamped_win_p.y},
                     .attachTo = CLAY_ATTACH_TO_ROOT},
        .border = {.color = UI_border_bg,
                   .width = UI_border_width},
        .cornerRadius = UI_radius})
  {
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW(0),
                                .height = CLAY_SIZING_FIT()},
                     .padding = CLAY_PADDING_ALL(2),
                     .childAlignment = {.x = CLAY_ALIGN_X_CENTER}},
          .backgroundColor = UI_bar_bg,
          .cornerRadius = UI_radius})
    {
      if (Clay_Hovered() && KEY_Pressed(KEY_MouseLeft))
        APP.debug.win_drag = true;
      if (APP.debug.win_drag)
        APP.debug.win_p = V2_Add(APP.debug.win_p, APP.mouse_delta);
      if (!APP.debug.win_drag)
        APP.debug.win_p = clamped_win_p;

      CLAY_TEXT(CLAY_STRING("Debug window"),
                CLAY_TEXT_CONFIG({.fontId = FONT_Regular, .textColor = UI_bar_fg}));
    }

    CLAY({.layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .sizing = {.width = CLAY_SIZING_GROW(0),
                                .height = CLAY_SIZING_FIT()},
                     .padding = UI_window_pad,
                     .childGap = UI_window_gap},
          .cornerRadius = UI_radius})
    {
      UI_RenderCheckbox(FONT_Regular, CLAY_STRING("📽️ Noclip camera"), false, &APP.debug.noclip_camera);
      UI_RenderCheckbox(FONT_Regular, CLAY_STRING("☀️ Sun camera"), false, &APP.debug.sun_camera);
      UI_RenderCheckbox(FONT_Regular, CLAY_STRING("📦 Draw collision box"), false, &APP.debug.draw_collision_box);
    }
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
  if (font_index < 0 || font_index >= FONT_COUNT) font_index = FONT_Regular;
  TTF_Font *ttf_font = APP.atlas.fonts[font_index][0];

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

static void UI_ProcessWindowResize()
{
  if (APP.window_resized)
  {
    Clay_SetLayoutDimensions((Clay_Dimensions){APP.window_dim.x, APP.window_dim.y});
    Clay_ResetMeasureTextCache();
  }
}

static void UI_StartFrame()
{
  if (!KEY_Held(KEY_MouseLeft) || KEY_Released(KEY_MouseLeft))
  {
    APP.debug.win_drag = false;
  }

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

    UI_GpuShape shape =
    {
      .p_min = (V2){box.x, box.y},
      .p_max = (V2){box.x + box.width, box.y + box.height},
      .tex_layer = -1.f,
    };

    switch (rcom->commandType)
    {
      case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
      {
        Clay_RectangleRenderData rect = rcom->renderData.rectangle;
        shape.color = Color32_ClayColor(rect.backgroundColor);
        shape.corner_radius = APP.atlas.scale * rect.cornerRadius.topLeft; // @todo support corner radius for each corner
      } break;

      case CLAY_RENDER_COMMAND_TYPE_BORDER:
      {
        Clay_BorderRenderData border = rcom->renderData.border;
        shape.color = Color32_ClayColor(border.color);
        shape.corner_radius = APP.atlas.scale * border.cornerRadius.topLeft; // @todo support corner radius for each corner
        shape.border_thickness = APP.atlas.scale * border.width.left; // @todo support border width per direction?
      } break;

      case CLAY_RENDER_COMMAND_TYPE_TEXT:
      {
        Clay_TextRenderData text = rcom->renderData.text;
        S8 string = S8_FromClaySlice(text.stringContents);

        FONT_Type font_index = text.fontId;
        if (font_index < 0 || font_index >= FONT_COUNT) font_index = FONT_Regular;

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
    }

    if (shape.corner_radius > 0.f)
      shape.edge_softness = APP.atlas.scale * 0.5f;

    UI_DrawRaw(shape);
  }
}
