// Text layout
// #scope_file;
// using,only(MeasureTextParams) Layout;
// #scope_export;

UI_TextLineParams :: struct
{
  draw: bool;
  measure: bool;
  dimensions: V2; // out
};

UI_ProcessTextLine :: (text: string, pos: V2, font: FONT_Family, pixel_height: u16, using params: *UI_TextLineParams)
{
  FONT_GetFaceMetrics(font, pixel_height);

  metrics := FONT_GetFaceMetrics(font, pixel_height);

  dimensions.y = metrics.linespace;

  text_pos := pos;
  text_pos.y += metrics.ascent;

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
        color = Color32_Grayi(255),
      };
      UI_DrawRaw(shape);
    }

    text_pos.x += glyph.advance;
    dimensions.x += glyph.advance;
  }
}

UI_DrawTextLine :: (text: string, pos: V2, font: FONT_Family, pixel_height: u16)
{
  params := UI_TextLineParams.{draw=true};
  UI_ProcessTextLine(text, pos, font, pixel_height, *params);
}

UI_MeasureText :: (params: Layout.MeasureTextParams) -> V2
{
  font := params.font.(FONT_Family);
  pixel_height := params.font_size.(u16);

  process_params := UI_TextLineParams.{measure=true};
  UI_ProcessTextLine(params.text, .{}, font, pixel_height, *process_params);
  return process_params.dimensions;
}
