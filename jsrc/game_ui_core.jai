UI_StartFrame :: ()
{
  LayoutBegin(G.window_dim);
}

UI_FinishFrame :: ()
{
  LayoutEnd(G.frame_number == 1);

  red: u32 = 0;

  for cmd: layout_state.render_commands
  {
    if G.frame_number == 1
      print("[%] render_command: % (%)\n", G.frame_number, cmd, cmd.box.loc);

    red = (red + 43) & 255;
    green := 255 - red;
    blue := (red + 128) & 255;
    debug_color := Color32_RGBAi(red, green, blue, 200);

    shape := UI_Shape.{
      p_min = cmd.p,
      p_max = cmd.p + cmd.dim,
      // color = cmd.color,
      color = debug_color,
      corner_radius = cmd.corner_radius,
      tex_layer = -1.0,
    };
    skip_shape := false;

    if cmd.type == {
      case .BORDER;
        shape.border_thickness = cmd.border_width;

      case .TEXT;
      skip_shape = true;

      sh := shape;
      for cmd.box.text.str
      {
        sh.p_min.x = cmd.p.x + it_index*16.0;
        sh.p_max.x = sh.p_min.x + 16.0;
        sh.p_min.y = shape.p_min.y + (it_index % 2) * 6.0;
        UI_DrawRaw(sh);
      }
    }

    if !skip_shape
      UI_DrawRaw(shape);
  }

  // Clay_RenderCommandArray render_commands = Clay_EndLayout();
  // ForI32(i, render_commands.length)
  // {
  //   Clay_RenderCommand *rcom = Clay_RenderCommandArray_Get(&render_commands, i);
  //   Clay_BoundingBox box = rcom.boundingBox;

  //   {
  //     box.x = FRound(box.x);
  //     box.y = FRound(box.y);
  //   }

  //   UI_Shape shape =
  //   {
  //     .p_min = (V2){box.x, box.y},
  //     .p_max = (V2){box.x + box.width, box.y + box.height},
  //     .tex_layer = -1.f,
  //   };
  //   bool skip_shape = false;

  //   switch (rcom.commandType)
  //   {
  //     case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
  //     {
  //       Clay_RectangleRenderData rect = rcom.renderData.rectangle;
  //       shape.color = Color32_ClayColor(rect.backgroundColor);
  //       shape.corner_radius = APP.font.scale * rect.cornerRadius.topLeft; // @todo support corner radius for each corner
  //     } break;

  //     case CLAY_RENDER_COMMAND_TYPE_BORDER:
  //     {
  //       Clay_BorderRenderData border = rcom.renderData.border;
  //       shape.color = Color32_ClayColor(border.color);
  //       shape.corner_radius = APP.font.scale * border.cornerRadius.topLeft; // @todo support corner radius for each corner
  //       shape.border_thickness = APP.font.scale * border.width.left; // @todo support border width per direction?

  //       if (rcom.userData == (void *)1)
  //       {
  //         // would be best to draw it in the background;
  //         // @todo add sorting in the future
  //         shape.corner_radius = 0;
  //         float expand = APP.font.scale * 10.f;
  //         V2 expand2 = (V2){expand, expand};
  //         shape.p_min = V2_Sub(shape.p_min, expand2);
  //         shape.p_max = V2_Add(shape.p_max, expand2);
  //         shape.edge_softness += expand;
  //       }
  //     } break;

  //     case CLAY_RENDER_COMMAND_TYPE_TEXT:
  //     {
  //       Clay_TextRenderData text = rcom.renderData.text;
  //       S8 string = S8_FromClaySlice(text.stringContents);

  //       FONT_Type font_index = text.fontId;
  //       if (font_index < 0 || font_index >= FONT_COUNT) font_index = UI_FONT;

  //       FONT_GlyphRun glyphs = FONT_GetGlyphRun(font_index, string);
  //       if (glyphs.hash)
  //       {
  //         shape.tex_min = V2_FromV2I16(glyphs.p);
  //         shape.tex_max = V2_FromV2I16(V2I16_Add(glyphs.p, glyphs.dim));
  //         shape.tex_layer = glyphs.layer;

  //         V2 dim = V2_Sub(shape.tex_max, shape.tex_min);
  //         shape.p_max = V2_Add(shape.p_min, dim);
  //       }

  //       //float real_width = shape.p_max.x - shape.p_min.x;
  //       //float tex_width = shape.tex_max.x - shape.tex_min.x;
  //       //LOG(LOG_Clay, "real width: %f, tex width: %f", real_width, tex_width);

  //       shape.color = Color32_ClayColor(text.textColor);
  //       // @todo font id, letterSpacing, lineHeight etc
  //     } break;

  //     case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
  //     {
  //       skip_shape = true;
  //       UI_Clip clip =
  //       {
  //         shape.p_min,
  //         shape.p_max
  //       };
  //       UI_PushClip(clip);
  //     } break;

  //     case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
  //     {
  //       skip_shape = true;
  //       UI_PopClip();
  //     } break;

  //     default: skip_shape = true; break;
  //   }

  //   if (shape.corner_radius > 0.f)
  //     shape.edge_softness += APP.font.scale * 0.25f;

  //   if (!skip_shape)
  //     UI_DrawRaw(shape);
  // }
}

#scope_file
using Layout;
