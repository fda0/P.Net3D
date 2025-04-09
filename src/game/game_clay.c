static void CL_RenderHeaderButton(Clay_String text)
{
  CLAY({.layout = {.padding = {16, 16, 8, 8}},
        .backgroundColor = {140, 140, 140, 255},
        .cornerRadius = CLAY_CORNER_RADIUS(5)})
  {
    CLAY_TEXT(text, CLAY_TEXT_CONFIG({.fontId = FA_Regular,
                                      .textColor = {255, 255, 255, 255}}));
  }
}


static void CL_RenderDropdownMenuItem(Clay_String text)
{
  CLAY({.layout = {.padding = CLAY_PADDING_ALL(16)}})
  {
    CLAY_TEXT(text, CLAY_TEXT_CONFIG({.fontId = FA_Regular,
                                      .fontSize = 16,
                                      .textColor = {255, 255, 255, 255}}));
  }
}

static void CL_CreateUI()
{
  Clay_Sizing layoutExpand =
  {
    .width = CLAY_SIZING_GROW(0),
    .height = CLAY_SIZING_GROW(0)
  };

  Clay_Color contentBackgroundColor = {90, 90, 90, 255};

  // Build UI here
  CLAY({.id = CLAY_ID("OuterContainer"),
        .backgroundColor = {43, 41, 51, 0},
        .layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                   .sizing = layoutExpand,
                   .padding = CLAY_PADDING_ALL(16),
                   .childGap = 16}})
  {
    // Child elements go inside braces
    CLAY({.id = CLAY_ID("HeaderBar"),
          .layout = {.sizing = {.height = CLAY_SIZING_FIXED(60),
                                .width = CLAY_SIZING_GROW(0)},
                     .padding = {16, 16, 0, 0},
                     .childGap = 16,
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = contentBackgroundColor,
          .cornerRadius = CLAY_CORNER_RADIUS(8)})
    {
      CLAY({.id = CLAY_ID("FileButton"),
            .layout = {.padding = {16, 16, 8, 8}},
            .backgroundColor = {140, 140, 140, 255},
            .cornerRadius = CLAY_CORNER_RADIUS(5)
            })
      {
        CLAY_TEXT(CLAY_STRING("File"),
                  CLAY_TEXT_CONFIG({.fontId = FA_Regular,
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
      CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0)}}}) {}
      CL_RenderHeaderButton(CLAY_STRING("Upload"));
      CL_RenderHeaderButton(CLAY_STRING("Media"));
      CL_RenderHeaderButton(CLAY_STRING("Support"));
    }

    CLAY({.id = CLAY_ID("LowerContent"),
          .layout = {.sizing = layoutExpand, .childGap = 16}})
    {
      CLAY({.id = CLAY_ID("Sidebar"),
            .backgroundColor = contentBackgroundColor,
            .layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                       .padding = CLAY_PADDING_ALL(16),
                       .childGap = 8,
                       .sizing = {.width = CLAY_SIZING_FIXED(250),
                                  .height = CLAY_SIZING_GROW(0)}}})
      {

      }

      CLAY({.id = CLAY_ID("MainContent"),
            .backgroundColor = contentBackgroundColor,
            .scroll = { .vertical = true },
            .layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                       .childGap = 16,
                       .padding = CLAY_PADDING_ALL(16),
                       .sizing = layoutExpand}})
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
  if (config->fontId < 0 || config->fontId > FA_Font_COUNT)
    font_index = FA_Regular;
  TTF_Font *ttf_font = APP.atlas.fonts[font_index][0];

  I32 width = 0, height = 0;
  if (!TTF_GetStringSize(ttf_font, (char *)string.str, string.size, &width, &height)) {
    LOG(Log_Clay, "Failed to measure text: %s", SDL_GetError());
  }

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
    Clay_RenderCommand *command = Clay_RenderCommandArray_Get(&render_commands, i);
    Clay_BoundingBox box = command->boundingBox;

    UI_GpuShape shape =
    {
      .p_min = (V2){box.x, box.y},
      .p_max = (V2){box.x + box.width, box.y + box.height},
      .tex_layer = -1.f,
    };

    switch (command->commandType)
    {
      case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
      {
        Clay_RectangleRenderData rect = command->renderData.rectangle;
        shape.color = Color32_ClayColor(rect.backgroundColor);
        shape.corner_radius = rect.cornerRadius.topLeft; // @todo support corner radius for each corner
      } break;

      case CLAY_RENDER_COMMAND_TYPE_TEXT:
      {
        Clay_TextRenderData text = command->renderData.text;
        S8 string = S8_FromClaySlice(text.stringContents);
        FA_GlyphRun glyphs = FA_GetGlyphRun(FA_Regular, string);
        if (glyphs.hash)
        {
          shape.tex_min = V2_FromV2I16(glyphs.p);
          shape.tex_max = V2_FromV2I16(V2I16_Add(glyphs.p, glyphs.dim));
          shape.tex_layer = glyphs.layer;
        }

        shape.color = Color32_ClayColor(text.textColor);
        // @todo font id, letterSpacing, lineHeight etc
      } break;
    }

    UI_DrawRaw(shape);
  }
}
