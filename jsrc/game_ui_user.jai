BR :: #import,file "brick_layout.jai"(ColorType=Color32);

UI_State :: struct
{
  dev: struct
  {
    show: bool;
    win_p: V2;
    win_dim: V2;
  };
};

UI_BuildDeveloperWindow :: ()
{
  using BR;
  using G.ui.dev;
  if (!show) return;

  clamped_win_p := clamp(V2.{}, G.window_dim * 0.9, win_p);

  // Parent(Box(.{layout = .{child_layout = .TOP_TO_BOTTOM,
  //                         sizing = .{width = UIC_Rem(200, fit_children = true),
  //                                    height = UIC_Rem(200)}},
  //              bg_color = UI_BG,
  //              floating = .{attach_to = .ROOT, offset = clamped_win_p},
  //              border = .{color = UI_SHADOW, width = UI_BORDER_WIDTH},
  //              corner_radius = UI_RADIUS}));
}
