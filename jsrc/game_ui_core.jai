#scope_file
using Layout;
#scope_export;

UI_StartFrame :: ()
{
  mouse_left: LayoutSpec.ButtonFlags;
  if KEY_Held(.MouseLeft) then mouse_left |= .HELD;
  if KEY_Pressed(.MouseLeft)  then mouse_left |= .PRESSED;
  if KEY_Released(.MouseLeft) then mouse_left |= .RELEASED;

  layout_spec := LayoutSpec.{
    root_dim_px = G.window_dim,
    rem_to_px = 12.0 + 2.0 * floor(G.window_dim.y / 256.0),
    measure_text_callback = UI_MeasureText,
    mouse = G.mouse,
    buttons[0] = mouse_left,
    debug_logging = (G.frame_number == 1),
  };
  LayoutBegin(layout_spec);
}

UI_FinishFrame :: ()
{
  LayoutEnd();

  for cmd: layout_state.render_commands
  {
    // if G.frame_number == 1
    //   print("[%] render_command: % (%)\n", G.frame_number, cmd, cmd.box.loc);

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
