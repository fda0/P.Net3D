// Text layout

UI_TextLineParams :: struct
{
  draw: bool;
  draw_p: V2;
  draw_color: Color32;
  dimensions: V2; // out
};

// All in one text function - can do layout, drawing and/or text dimension measurement
UI_ProcessTextLine :: (text: string, font: FONT_Family, pixel_height: u16, using params: *UI_TextLineParams)
{
  metrics := FONT_GetFaceMetrics(font, pixel_height);

  // draw
  text_pos := draw_p;
  text_pos.y += metrics.ascent;

  // measure
  dimensions.y = metrics.linespace;

  for codepoint: text // this isn't correct
  {
    glyph := FONT_GetGlyph(xx codepoint, font, pixel_height);
    tex_p   := V2.{xx glyph.p.x, xx glyph.p.y};
    tex_dim := V2.{xx glyph.dim.x, xx glyph.dim.y};

    if draw
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
      text_pos.x += glyph.advance;
    }

    dimensions.x += glyph.advance;
  }
}

UI_DrawTextLine :: (text: string, font: FONT_Family, pixel_height: u16, pos: V2, color: Color32)
{
  params := UI_TextLineParams.{draw=true, draw_p=pos, draw_color=color};
  UI_ProcessTextLine(text, font, pixel_height, *params);
}

UI_MeasureText :: (params: Layout.MeasureTextParams) -> V2
{
  font := params.font.(FONT_Family);
  pixel_height := Round(params.font_size).(u16);

  process_params: UI_TextLineParams;
  UI_ProcessTextLine(params.text, font, pixel_height, *process_params);
  return process_params.dimensions;
}
