Init :: ()
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

StartLayout :: ()
{
  if (!KEY_Held(KEY_MouseLeft) || KEY_Released(KEY_MouseLeft))
    APP.debug.click_id = 0;

  Clay_SetPointerState((Clay_Vector2){APP.mouse.x, APP.mouse.y}, KEY_Held(KEY_MouseLeft));
  Clay_UpdateScrollContainers(true, (Clay_Vector2){APP.mouse_scroll.x, APP.mouse_scroll.y}, APP.dt);
  Clay_BeginLayout();
}

FinishLayout :: ()
{
  Clay_RenderCommandArray render_commands = Clay_EndLayout();
  ForI32(i, render_commands.length)
  {
    Clay_RenderCommand *rcom = Clay_RenderCommandArray_Get(&render_commands, i);
    Clay_BoundingBox box = rcom.boundingBox;

    {
      box.x = FRound(box.x);
      box.y = FRound(box.y);
    }

    UI_Shape shape =
    {
      .p_min = (V2){box.x, box.y},
      .p_max = (V2){box.x + box.width, box.y + box.height},
      .tex_layer = -1.f,
    };
    bool skip_shape = false;

    switch (rcom.commandType)
    {
      case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
      {
        Clay_RectangleRenderData rect = rcom.renderData.rectangle;
        shape.color = Color32_ClayColor(rect.backgroundColor);
        shape.corner_radius = APP.font.scale * rect.cornerRadius.topLeft; // @todo support corner radius for each corner
      } break;

      case CLAY_RENDER_COMMAND_TYPE_BORDER:
      {
        Clay_BorderRenderData border = rcom.renderData.border;
        shape.color = Color32_ClayColor(border.color);
        shape.corner_radius = APP.font.scale * border.cornerRadius.topLeft; // @todo support corner radius for each corner
        shape.border_thickness = APP.font.scale * border.width.left; // @todo support border width per direction?

        if (rcom.userData == (void *)1)
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
        Clay_TextRenderData text = rcom.renderData.text;
        S8 string = S8_FromClaySlice(text.stringContents);

        FONT_Type font_index = text.fontId;
        if (font_index < 0 || font_index >= FONT_COUNT) font_index = UI_FONT;

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
        //LOG(LOG_Clay, "real width: %f, tex width: %f", real_width, tex_width);

        shape.color = Color32_ClayColor(text.textColor);
        // @todo font id, letterSpacing, lineHeight etc
      } break;

      case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
      {
        skip_shape = true;
        UI_Clip clip =
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

    if (!skip_shape)
      UI_DrawRaw(shape);
  }
}

MeasureText :: (Clay_StringSlice clay_slice, Clay_TextElementConfig *config, void *user_data) -> Clay_Dimensions
{
  // @speed profile to check if text measurement should be cached
  (void)user_data;
  S8 string = S8_FromClaySlice(clay_slice);

  FONT_Type font_index = config.fontId;
  if (font_index < 0 || font_index >= FONT_COUNT) font_index = UI_FONT;
  TTF_Font *ttf_font = APP.font.ttfs[font_index][0];

  I32 width = 0, height = 0;
  if (!TTF_GetStringSize(ttf_font, (char *)string.str, string.size, &width, &height))
    LOG(LOG_Clay, "Failed to measure text: %s", SDL_GetError());

  Clay_Dimensions result = {width, height};
  return result;
}

ProcessWindowAndFontResize :: ()
{
  if (APP.window_resized)
    Clay_SetLayoutDimensions((Clay_Dimensions){APP.window_dim.x, APP.window_dim.y});

  if (APP.font.scale_changed)
    Clay_ResetMeasureTextCache();
}
