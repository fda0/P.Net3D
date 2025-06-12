UI_Init :: ()
{
  SetMeasureTextCallback(UI_MeasureText);
}

UI_StartFrame :: ()
{
  LayoutBegin(G.window_dim, 12.0 + 2.0 * floor(G.window_dim.y / 256.0));
}

UI_FinishFrame :: ()
{
  LayoutEnd(G.frame_number == 1);

  for cmd: layout_state.render_commands
  {
    if G.frame_number == 1
      print("[%] render_command: % (%)\n", G.frame_number, cmd, cmd.box.loc);

    c4 := ColorV4_32(cmd.color);
    c32 := Color32_V4(c4);

    shape := UI_Shape.{
      p_min = cmd.p,
      p_max = cmd.p + cmd.dim,
      color = c32,
      corner_radius = cmd.corner_radius,
      tex_layer = -1.0,
    };
    skip_shape := false;

    if cmd.type == {
      case .BORDER;
        shape.border_thickness = cmd.border_width;

      case .TEXT;
      skip_shape = true;
      t := cmd.box.text;
      UI_DrawTextLine(t.str, xx t.font, xx t.font_size, cmd.p, t.color);
    }

    if !skip_shape
      UI_DrawRaw(shape);
  }
}

#scope_file
using Layout;
