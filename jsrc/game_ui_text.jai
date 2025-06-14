// Text layout
UI_TextAlignY :: enum {TOP; CENTER; BOTTOM;};

UI_TextLineParams :: struct
{
  draw: bool;
  draw_p: V2;
  draw_dim: V2 = .{-1,-1};
  draw_color: Color32;
  // draw_align_x: enum {LEFT; CENTER; RIGHT;};
  draw_align_y: UI_TextAlignY;

  dimensions: V2; // out
};

// All in one text function - can do layout, drawing and/or text dimension measurement
UI_ProcessTextLine :: (text: string, font: FONT_Family, pixel_height: u16, using params: *UI_TextLineParams)
{
  metrics := FONT_GetFaceMetrics(font, pixel_height);

  // draw
  text_pos := draw_p;

  if draw_align_y == {
    case .TOP;
    text_pos.y += metrics.ascent;
    text_pos.y += metrics.descent;

    case .CENTER;
    text_pos.y += metrics.ascent;
    text_pos.y += metrics.descent;
    if draw_dim.y >= 0.0 then text_pos.y += 0.5 * draw_dim.y;

    case .BOTTOM;
    text_pos.y += metrics.ascent;
    text_pos.y += metrics.descent;
    if draw_dim.y >= 0.0 then text_pos.y += 1.0 * draw_dim.y;
  }

  // measure
  dimensions.y = metrics.linespace;

  t := text;
  while t.count
  {
    codepoint, byte_length := STR_DecodeUTF8(t);
    t = STR_Skip(t, byte_length);

    glyph := FONT_GetGlyph(codepoint, font, pixel_height);

    if draw
    {
      tex_p   := V2.{xx glyph.p.x, xx glyph.p.y};
      tex_dim := V2.{xx glyph.dim.x, xx glyph.dim.y};

      if tex_dim.x > 0 && tex_dim.y > 0 // skip glyphs without bitmap
      {
        p := text_pos + glyph.bearing;
        p.x = Round(p.x);
        p.y = Round(p.y);

        shape := UI_Shape.{
          p_min = p,
          p_max = p + tex_dim,
          tex_min = tex_p,
          tex_max = tex_p + tex_dim,
          tex_layer = xx glyph.layer,
          color = draw_color,
        };
        UI_DrawRaw(shape);
      }

      text_pos.x += glyph.advance;
    }

    dimensions.x += glyph.advance;
  }
}

UI_DrawTextLine :: (text: string, font: FONT_Family, pixel_height: u16, color: Color32,
                    pos: V2, dim := V2.{-1,-1}, align_y := UI_TextAlignY.TOP)
{
  params := UI_TextLineParams.{
    draw = true,
    draw_p = pos,
    draw_dim = dim,
    draw_align_y = align_y,
    draw_color = color,
  };
  UI_ProcessTextLine(text, font, pixel_height, *params);
}


UI_MeasureText :: (params: Layout.MeasureTextParams) -> V2
{
  font := params.font.(FONT_Family);
  pixel_height := Round(params.font_size_px).(u16);

  process_params: UI_TextLineParams;
  UI_ProcessTextLine(params.text, font, pixel_height, *process_params);
  return process_params.dimensions;
}
