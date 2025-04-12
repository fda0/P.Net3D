static Clay_Color CL_content_bg = {0, 0, 0, 128};
static Clay_Color CL_btn_color = {40,40,40,255};
static Clay_Color CL_btn_hover_color = {60,60,60,255};
static Clay_Padding CL_button_pad = {8, 8, 4, 4};
static bool CL_checkbox_test;

static void CL_RenderHeaderButton(Clay_String text)
{
  CLAY({.layout = {.sizing = {.height = CLAY_SIZING_GROW(0)},
                   .padding = CL_button_pad,
                   .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
        .backgroundColor = Clay_Hovered() ? CL_btn_hover_color : CL_btn_color,
        .cornerRadius = CLAY_CORNER_RADIUS(5)})
  {
    CLAY_TEXT(text, CLAY_TEXT_CONFIG({.fontId = FA_Header,
                                      .textColor = {255, 255, 255, 255}}));
  }
}


static void CL_RenderDropdownMenuItem(Clay_String text)
{
  CLAY({.layout = {.padding = CLAY_PADDING_ALL(16)}})
  {
    CLAY_TEXT(text, CLAY_TEXT_CONFIG({.fontId = FA_Regular,
                                      .textColor = {255, 255, 255, 255}}));
  }
}

static void CL_CreateUI()
{
  Clay_Sizing layout_expand =
  {
    .width = CLAY_SIZING_GROW(0),
    .height = CLAY_SIZING_GROW(0)
  };

  // Build UI here
  CLAY({.id = CLAY_ID("OuterContainer"),
        .backgroundColor = {43, 41, 51, 0},
        .layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                   .sizing = layout_expand,
                   .padding = CLAY_PADDING_ALL(16),
                   .childGap = 16}})
  {
    // Child elements go inside braces
    CLAY({.id = CLAY_ID("HeaderBar"),
          .layout = {.sizing = {.height = CLAY_SIZING_FIT(0),
                                .width = CLAY_SIZING_GROW(0)},
                     .padding = {16, 16, 0, 0},
                     .childGap = 16,
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = CL_content_bg,
          .cornerRadius = CLAY_CORNER_RADIUS(8)})
    {
      CLAY({.id = CLAY_ID("FileButton"),
            .layout = {.sizing = {.height = CLAY_SIZING_GROW(0)},
                       .padding = CL_button_pad,
                       .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
            .backgroundColor = Clay_Hovered() ? CL_btn_hover_color : CL_btn_color,
            .cornerRadius = CLAY_CORNER_RADIUS(5)})
      {
        CLAY_TEXT(CLAY_STRING("File"),
                  CLAY_TEXT_CONFIG({.fontId = FA_Header,
                                    .textColor = {255, 255, 255, 255}}));

        bool fileMenuVisible =
          Clay_PointerOver(Clay_GetElementId(CLAY_STRING("FileButton"))) ||
          Clay_PointerOver(Clay_GetElementId(CLAY_STRING("FileMenu")));

        if (fileMenuVisible)
        {
          CLAY({.id = CLAY_ID("FileMenu"),
                .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                             .attachPoints = {.parent = CLAY_ATTACH_POINT_LEFT_BOTTOM}},
                .layout = {.padding = {0, 0, 8, 8}}})
          {
            CLAY({.layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                             .sizing = {.width = CLAY_SIZING_FIXED(200)}},
                  .backgroundColor = {40, 40, 40, 255},
                  .cornerRadius = CLAY_CORNER_RADIUS(8)})
            {
              // Render dropdown items here
              CL_RenderDropdownMenuItem(CLAY_STRING("New"));
              CL_RenderDropdownMenuItem(CLAY_STRING("Open"));
              CL_RenderDropdownMenuItem(CLAY_STRING("Close"));
            }
          }
        }
      }

      CL_RenderHeaderButton(CLAY_STRING("Edit"));

      {

        CLAY({.id = CLAY_ID("CheckboxContainer"),
              .layout = {.sizing = {.height = CLAY_SIZING_GROW(0)},
                         .padding = CL_button_pad,
                         .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = Clay_Hovered() ? CL_btn_hover_color : CL_btn_color,
              .cornerRadius = CLAY_CORNER_RADIUS(5)})
        {
          if (Clay_PointerOver(Clay_GetElementId(CLAY_STRING("CheckboxContainer"))) && KEY_Pressed(KEY_MouseLeft))
            CL_checkbox_test = !CL_checkbox_test;

          CLAY({.id = CLAY_ID("TestCheckbox"),
                .layout = {.sizing = {.width = CLAY_SIZING_FIXED(25),
                                      .height = CLAY_SIZING_FIXED(25)},
                           .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}},
                .backgroundColor = Clay_Hovered() ? CL_btn_hover_color : CL_btn_color,
                .border = {.color = {240, 240, 240, 255},
                           .width = CLAY_BORDER_ALL(2)},
                .cornerRadius = CLAY_CORNER_RADIUS(4)})
          {
            if (CL_checkbox_test)
            {
              CLAY_TEXT(CLAY_STRING("X"),
                        CLAY_TEXT_CONFIG({.fontId = FA_Header,
                                          .textColor = {255, 255, 255, 255}}));
            }
          }
          CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(10)}}}) {}

          CLAY_TEXT(CLAY_STRING("Nice checkbox"),
                    CLAY_TEXT_CONFIG({.fontId = FA_Header,
                                      .textColor = {255, 255, 255, 255}}));
        }
      }


      CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0)}}}) {}
      CL_RenderHeaderButton(CLAY_STRING("Upload"));
      CL_RenderHeaderButton(CLAY_STRING("Media"));
      CL_RenderHeaderButton(CLAY_STRING("Support"));
    }

    CLAY({.id = CLAY_ID("LowerContent"),
          .layout = {.sizing = layout_expand, .childGap = 16}})
    {
      CLAY({.id = CLAY_ID("Sidebar"),
            .backgroundColor = CL_content_bg,
            .layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                       .padding = CLAY_PADDING_ALL(16),
                       .childGap = 8,
                       .sizing = {.width = CLAY_SIZING_FIXED(250),
                                  .height = CLAY_SIZING_GROW(0)}}})
      {

      }

      CLAY({.id = CLAY_ID("MainContent"),
            .backgroundColor = CL_content_bg,
            .scroll = {.vertical = true },
            .layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                       .childGap = 16,
                       .padding = CLAY_PADDING_ALL(16),
                       .sizing = layout_expand}})
      {

      }
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

  FA_Font font_index = config->fontId;
  if (font_index < 0 || font_index >= FA_Font_COUNT) font_index = FA_Regular;
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

  Clay_Initialize(clay_arena, (Clay_Dimensions){APP.window_width, APP.window_height}, (Clay_ErrorHandler){CL_LogError});
  Clay_SetMeasureTextFunction(CL_MeasureText, 0);
}

static void CL_ProcessWindowResize()
{
  if (APP.window_resized)
  {
    Clay_SetLayoutDimensions((Clay_Dimensions){APP.window_width, APP.window_height});
    Clay_ResetMeasureTextCache();
  }
}

static void CL_StartFrame()
{
  Clay_SetPointerState((Clay_Vector2){APP.mouse.x, APP.mouse.y}, KEY_Pressed(KEY_MouseLeft));
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

        FA_Font font_index = text.fontId;
        if (font_index < 0 || font_index >= FA_Font_COUNT) font_index = FA_Regular;

        FA_GlyphRun glyphs = FA_GetGlyphRun(font_index, string);
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
