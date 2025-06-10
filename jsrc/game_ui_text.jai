// Text layout
// #scope_file;
// using,only(MeasureTextParams) Layout;
// #scope_export;

UI_DrawTextLine :: (text: string, pos: V2, font: FONT_Family, pixel_height: u16)
{
  p := pos;
  for codepoint: text // this isn't correct
  {
    glyph := FONT_GetGlyph(xx codepoint, font, pixel_height);
    tex_p   := V2.{xx glyph.p.x, xx glyph.p.y};
    tex_dim := V2.{xx glyph.dim.x, xx glyph.dim.y};

    shape := UI_Shape.{
      p_min = p,
      p_max = p + tex_dim,
      tex_min = tex_p,
      tex_max = tex_p + tex_dim,
      tex_layer = xx glyph.layer,
      color = Color32_Grayi(255),
    };
    UI_DrawRaw(shape);

    p.x += tex_dim.x;
  }
}

UI_MeasureText :: (params: Layout.MeasureTextParams) -> V2
{
  using params;
  result: V2;
  for codepoint: text // this isn't correct
  {
    glyph := FONT_GetGlyph(xx codepoint, font.(FONT_Family), font_size.(u16));
    tex_dim := V2.{xx glyph.dim.x, xx glyph.dim.y};
    result.x += tex_dim.x;
    result.y = max(tex_dim.y, result.y);
  }
  return result;
}
