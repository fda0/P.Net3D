#scope_file
using Layout;
#scope_export;

UI_StartFrame :: ()
{
  spec := LayoutSpec.{
    // debug_logging = (G.frame_number == 1),

    root_dim_px = G.window_dim,
    rem_to_px = 12.0 + 2.0 * floor(G.window_dim.y / 256.0),
    measure_text_callback = UI_MeasureText,
    mouse = G.mouse,
  };

  using ButtonType;
  spec.are_buttons_down[MOUSE_LEFT]   = KEY_Held(.MOUSE_LEFT);
  spec.are_buttons_down[MOUSE_MIDDLE] = false;
  spec.are_buttons_down[MOUSE_RIGHT]  = KEY_Held(.MOUSE_RIGHT);

  LayoutBegin(spec);
}

UI_FinishFrame :: ()
{
  LayoutEnd();

  for cmd: layout_state.render_commands
  {
    // if G.frame_number == 1
    //   print("[%] render_command: % (%)\n", G.frame_number, cmd, cmd.box.loc);

    c4 := ColorV4_32(cmd.color);
    c32 := Color32_RGBAf(c4);

    shape := UI_Shape.{
      rect = MakeRangeMinDim(cmd.p, cmd.dim),
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
      UI_DrawTextLine(cmd.text, xx cmd.font, xx cmd.font_size_px, cmd.color, cmd.p);
    }

    if !skip_shape
      UI_Draw2D(shape);
  }
}
