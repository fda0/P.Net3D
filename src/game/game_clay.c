static void CLAY_LogError(Clay_ErrorData error)
{
  LOG(Log_Clay, "%s", error.errorText.chars);
}

static Clay_Dimensions CLAY_MeasureText(Clay_StringSlice clay_string, Clay_TextElementConfig *config, void *user_data)
{
  (void)user_data;
  S8 string = S8_Make((U8 *)clay_string.chars, clay_string.length);

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

static void CLAY_Init()
{
  U64 memory_size = Clay_MinMemorySize();
  Clay_Arena clay_arena = (Clay_Arena)
  {
    .memory = SDL_malloc(memory_size),
    .capacity = memory_size
  };

  Clay_Initialize(clay_arena, (Clay_Dimensions){APP.window_width, APP.window_height}, (Clay_ErrorHandler){CLAY_LogError});
  Clay_SetMeasureTextFunction(CLAY_MeasureText, 0);
}

static void CLAY_ProcessWindowResize()
{
  if (APP.window_resized)
  {
    Clay_SetLayoutDimensions((Clay_Dimensions){APP.window_width, APP.window_height});
  }
}

static void CLAY_StartFrame()
{
  Clay_SetPointerState((Clay_Vector2){APP.mouse.x, APP.mouse.y}, KEY_Pressed(KEY_MouseLeft));
  Clay_UpdateScrollContainers(true, (Clay_Vector2){APP.mouse_scroll.x, APP.mouse_scroll.y}, APP.dt);
  Clay_BeginLayout();
}

void CLAY_RenderDropdownMenuItem(Clay_String text)
{
  CLAY({.layout = { .padding = CLAY_PADDING_ALL(16)}}) {
    CLAY_TEXT(text, CLAY_TEXT_CONFIG({
                                      .fontId = FA_Regular,
                                      .fontSize = 16,
                                      .textColor = { 255, 255, 255, 255 }
                                      }));
  }
}

static void CLAY_AddLayoutItems()
{
  Clay_Sizing layoutExpand = {
    .width = CLAY_SIZING_GROW(0),
    .height = CLAY_SIZING_GROW(0)
  };

  Clay_Color contentBackgroundColor = { 90, 90, 90, 255 };

  // Build UI here
  CLAY({.id = CLAY_ID("OuterContainer"),
        .backgroundColor = {43, 41, 51, 0 },
        .layout =
        {
         .layoutDirection = CLAY_TOP_TO_BOTTOM,
         .sizing = layoutExpand,
         .padding = CLAY_PADDING_ALL(16),
         .childGap = 16
         }
        })
  {
    // Child elements go inside braces
    CLAY({ .id = CLAY_ID("HeaderBar"),
          .layout =
          {
           .sizing =
           {
            .height = CLAY_SIZING_FIXED(60),
            .width = CLAY_SIZING_GROW(0)
            },
           .padding = { 16, 16, 0, 0 },
           .childGap = 16,
           .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
           },
          .backgroundColor = contentBackgroundColor,
          .cornerRadius = CLAY_CORNER_RADIUS(8)
          })
    {
      CLAY({ .id = CLAY_ID("FileButton"),
            .layout = { .padding = { 16, 16, 8, 8 } },
            .backgroundColor = { 140, 140, 140, 255 },
            .cornerRadius = CLAY_CORNER_RADIUS(5)
            })
      {
        CLAY_TEXT(CLAY_STRING("File"),
                  CLAY_TEXT_CONFIG({
                                    .fontId = FA_Regular,
                                    .fontSize = 16,
                                    .textColor = { 255, 255, 255, 255 }
                                    }));

        bool fileMenuVisible =
          Clay_PointerOver(Clay_GetElementId(CLAY_STRING("FileButton"))) ||
          Clay_PointerOver(Clay_GetElementId(CLAY_STRING("FileMenu")));

        if (fileMenuVisible)
        { // Below has been changed slightly to fix the small bug where the menu would dismiss when mousing over the top gap
          CLAY({ .id = CLAY_ID("FileMenu"),
                .floating =
                {
                 .attachTo = CLAY_ATTACH_TO_PARENT,
                 .attachPoints = { .parent = CLAY_ATTACH_POINT_LEFT_BOTTOM },
                 },
                .layout = { .padding = {0, 0, 8, 8} }
                })
          {
            CLAY({
                  .layout =
                  {
                   .layoutDirection = CLAY_TOP_TO_BOTTOM,
                   .sizing = { .width = CLAY_SIZING_FIXED(200) },
                   },
                  .backgroundColor = { 40, 40, 40, 255 },
                  .cornerRadius = CLAY_CORNER_RADIUS(8)
                  })
            {
              // Render dropdown items here
              CLAY_RenderDropdownMenuItem(CLAY_STRING("New"));
              CLAY_RenderDropdownMenuItem(CLAY_STRING("Open"));
              CLAY_RenderDropdownMenuItem(CLAY_STRING("Close"));
            }
          }

        }
      }
    }
  }


}

static void CLAY_FinishFrame()
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
        S8 string = S8_Make((U8 *)text.stringContents.chars, text.stringContents.length);
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
