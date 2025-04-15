static Clay_Color CL_content_bg = {0, 0, 0, 128};
static Clay_Color CL_btn_color = {40,40,40,255};
static Clay_Color CL_btn_hover_color = {60,60,60,255};
static Clay_Padding CL_button_pad = {8, 8, 4, 4};
static Clay_CornerRadius CL_radius = {8, 8, 8, 8};
static bool CL_checkbox_test;

static void CL_RenderHeaderButton(Clay_String text)
{
  CLAY({.layout = {.sizing = {.height = CLAY_SIZING_GROW(0)},
                   .padding = CL_button_pad,
                   .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
        .backgroundColor = Clay_Hovered() ? CL_btn_hover_color : CL_btn_color,
        .cornerRadius = CLAY_CORNER_RADIUS(5)})
  {
    CLAY_TEXT(text, CLAY_TEXT_CONFIG({.fontId = FONT_Header,
                                      .textColor = {255, 255, 255, 255}}));
  }
}

static void CL_RenderDropdownMenuItem(Clay_String text)
{
  CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW(0)},
                   .padding = CLAY_PADDING_ALL(16)},
        .backgroundColor = Clay_Hovered() ? CL_btn_hover_color : CL_btn_color,
        .cornerRadius = CL_radius})
  {
    CLAY_TEXT(text, CLAY_TEXT_CONFIG({.fontId = FONT_Regular,
                                      .textColor = {255, 255, 255, 255}}));
  }
}

static void CL_HoverCallbackCheckbox(Clay_ElementId element_id, Clay_PointerData pointer_info, intptr_t user_data)
{
  (void)element_id;
  bool *checkbox_ptr = (bool *)user_data;
  if (pointer_info.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME)
  {
    *checkbox_ptr = !(*checkbox_ptr);
  }
}

static void CL_RenderCheckbox(FONT_Type font, Clay_String label, bool in_horizontal_bar, bool *checkbox_bool)
{
  Clay_Sizing root_sizing = {};
  if (in_horizontal_bar) root_sizing.height = CLAY_SIZING_GROW(0);
  else                   root_sizing.width  = CLAY_SIZING_GROW(0);

  CLAY({.layout = {.sizing = root_sizing,
                   .padding = CL_button_pad,
                   .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
        .backgroundColor = Clay_Hovered() ? CL_btn_hover_color : CL_btn_color,
        .cornerRadius = CL_radius})
  {
    Clay_OnHover(CL_HoverCallbackCheckbox, (intptr_t)checkbox_bool);

    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_FIXED(25),
                                .height = CLAY_SIZING_FIXED(25)},
                     .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = Clay_Hovered() ? CL_btn_hover_color : CL_btn_color,
          .border = {.color = {240, 240, 240, 255},
                     .width = CLAY_BORDER_ALL(2)},
          .cornerRadius = CLAY_CORNER_RADIUS(4)})
    {
      if (*checkbox_bool)
        CLAY_TEXT(CLAY_STRING("X"), CLAY_TEXT_CONFIG({.fontId = font, .textColor = {255, 255, 255, 255}}));
    }
    CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(10)}}}) {}
    CLAY_TEXT(label, CLAY_TEXT_CONFIG({.fontId = font, .textColor = {255, 255, 255, 255}}));
  }
}

static void CL_BuildUILayoutElements()
{
  V2 clamped_win_p = V2_Clamp((V2){}, V2_Scale(APP.window_dim, 0.9f), APP.debug.win_p);

  CLAY({.layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                   .sizing = {.width = CLAY_SIZING_FIT(),
                              .height = CLAY_SIZING_FIT()}},
        .backgroundColor = {10, 250, 10, 128},
        .floating = {.offset = {clamped_win_p.x, clamped_win_p.y},
                     .attachTo = CLAY_ATTACH_TO_ROOT},
        .border = {.color = {0,0,0,255},
                   .width = CLAY_BORDER_OUTSIDE(2)}})
  {
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW(0),
                                .height = CLAY_SIZING_FIT()},
                     .padding = CLAY_PADDING_ALL(2),
                     .childAlignment = {.x = CLAY_ALIGN_X_CENTER}},
          .backgroundColor = {10, 10, 250, 128}})
    {
      if (Clay_Hovered() && KEY_Pressed(KEY_MouseLeft))
        APP.debug.win_drag = true;
      if (APP.debug.win_drag)
        APP.debug.win_p = V2_Add(APP.debug.win_p, APP.mouse_delta);
      if (!APP.debug.win_drag)
        APP.debug.win_p = clamped_win_p;

      CLAY_TEXT(CLAY_STRING("Debug window"),
                CLAY_TEXT_CONFIG({.fontId = FONT_Regular, .textColor = {255,255,255,255}}));
    }

    CLAY({.layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .sizing = {.width = CLAY_SIZING_GROW(0),
                                .height = CLAY_SIZING_FIT()},
                     .padding = CLAY_PADDING_ALL(16),
                     .childGap = 8},
          .backgroundColor = {250, 10, 10, 128}})
    {
      CL_RenderCheckbox(FONT_Regular, CLAY_STRING("📽️ Noclip camera"), false, &APP.debug.noclip_camera);
      CL_RenderCheckbox(FONT_Regular, CLAY_STRING("☀️ Sun camera"), false, &APP.debug.sun_camera);
      CL_RenderCheckbox(FONT_Regular, CLAY_STRING("📦 Draw collision box"), false, &APP.debug.draw_collision_box);
    }
  }
}

//
//
//
static void CL_LogError(Clay_ErrorData error)
{
  LOG(Log_Clay, "%s", error.errorText.chars);
}

static Clay_Dimensions CL_MeasureText(Clay_StringSlice clay_slice, Clay_TextElementConfig *config, void *user_data)
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

static void CL_Init()
{
  U64 memory_size = Clay_MinMemorySize();
  Clay_Arena clay_arena = (Clay_Arena)
  {
    .memory = SDL_malloc(memory_size),
    .capacity = memory_size
  };

  Clay_Initialize(clay_arena, (Clay_Dimensions){APP.window_dim.x, APP.window_dim.y}, (Clay_ErrorHandler){CL_LogError});
  Clay_SetMeasureTextFunction(CL_MeasureText, 0);
}

static void CL_ProcessWindowResize()
{
  if (APP.window_resized)
  {
    Clay_SetLayoutDimensions((Clay_Dimensions){APP.window_dim.x, APP.window_dim.y});
    Clay_ResetMeasureTextCache();
  }
}

static void CL_StartFrame()
{
  if (!KEY_Held(KEY_MouseLeft) || KEY_Released(KEY_MouseLeft))
  {
    APP.debug.win_drag = false;
  }

  Clay_SetPointerState((Clay_Vector2){APP.mouse.x, APP.mouse.y}, KEY_Held(KEY_MouseLeft));
  Clay_UpdateScrollContainers(true, (Clay_Vector2){APP.mouse_scroll.x, APP.mouse_scroll.y}, APP.dt);
  Clay_BeginLayout();
}

static void CL_FinishFrame()
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
        shape.corner_radius = rect.cornerRadius.topLeft; // @todo support corner radius for each corner
      } break;

      case CLAY_RENDER_COMMAND_TYPE_BORDER:
      {
        Clay_BorderRenderData border = rcom->renderData.border;
        shape.color = Color32_ClayColor(border.color);
        shape.corner_radius = border.cornerRadius.topLeft; // @todo support corner radius for each corner
        shape.border_thickness = border.width.left; // @todo support border width per direction?
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
      shape.edge_softness = 1.f;

    UI_DrawRaw(shape);
  }
}
