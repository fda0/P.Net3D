UI_State :: struct
{
  dev: struct
  {
    show := true;
    win_p: V2;
    win_dim: V2;
  };
};

UI_RED1      :: #run Color32_RGBi(0xcc, 0x24, 0x1d);
UI_RED2      :: #run Color32_RGBi(0xfb, 0x49, 0x34);
UI_GREEN1    :: #run Color32_RGBi(0x98, 0x97, 0x1a);
UI_GREEN2    :: #run Color32_RGBi(0xb8, 0xbb, 0x26);
UI_YELLOW1   :: #run Color32_RGBi(0xd7, 0x99, 0x21);
UI_YELLOW2   :: #run Color32_RGBi(0xfa, 0xbd, 0x2f);
UI_BLUE1     :: #run Color32_RGBi(0x45, 0x85, 0x88);
UI_BLUE2     :: #run Color32_RGBi(0x83, 0xa5, 0x98);
UI_ORANGE1   :: #run Color32_RGBi(0xd6, 0x5d, 0x0e);
UI_ORANGE2   :: #run Color32_RGBi(0xfe, 0x80, 0x19);
UI_BG        :: #run Color32_RGBi(40, 40, 40);
UI_FG        :: #run Color32_RGBi(235, 219, 178);
UI_BORDER_BG :: #run Color32_RGBi(29, 32, 33);
UI_SHADOW    :: #run Color32_RGBAi(29, 32, 33, 96);
UI_DARKER    :: #run Color32_RGBAi(0, 0, 0, 64);

UI_RADIUS :: #run CornerRadius_All(Px(3));
UI_BORDER_WIDTH :: #run BorderWidths_Sides(Px(1));
UI_BAR_PAD :: #run Padding_All(Rem(0.5 + 2.5));
UI_WINDOW_GAP :: #run Rem(4 + 1);
UI_WINDOW_PAD :: #run Padding_All(UI_WINDOW_GAP);

UI_BuildDeveloperWindow :: ()
{
  using G.ui.dev;
  if (!show) return;

  header_text := BoxSpec.{text = .{font = xx FONT_Family.PlayfairDisplay, font_size = 24}};
  regular_text := BoxSpec.{text = .{font = xx FONT_Family.PlayfairDisplay, font_size = 16}};
  funky_text := BoxSpec.{text = .{font = xx FONT_Family.Jacquard24, font_size = 24}};

  Layer(.{text = .{color = Color32_Grayi(255),
                   font = xx FONT_Family.PlayfairDisplay}});

  clamped_win_p := clamp(win_p, .{}, G.window_dim * 0.9);

  // Floating developer window
  Parent(CreateBox(.{layout = .{child_layout = .TOP_TO_BOTTOM,
                                sizing = .{width = Fit(SizeMin(Rem(20))), height = Size(Rem(20))}},
                     bg_color = UI_BG,
                     floating = .{attach_to = .ROOT, offset = Px(clamped_win_p)},
                     border = .{color = UI_SHADOW, widths = UI_BORDER_WIDTH},
                     corner_radius = UI_RADIUS}));

  // Window bar
  {
    window_bar := Parent(CreateBox(.{layout = .{sizing = .{width = Grow(), height = Fit()},
                                                padding = UI_BAR_PAD,
                                                child_align.x = .CENTER},
                                     bg_color = UI_ORANGE2,
                                     corner_radius = UI_RADIUS}));
    if window_bar.flags & .DRAG  win_p += G.mouse_delta;
    else                         win_p = clamped_win_p;

    CreateText("Developer window", .{text.color = UI_BG}/*, header_text*/);
  }

  // Content body
  {
    Parent(CreateBox(.{layout = .{sizing = .{Grow(), Grow()}},
                       bg_color = Color32_RGBAf(0.2, 0.2, 0.7 + 0.3*Sin(G.at), 0.75)}));

    // Category vertical bar
    {
      Parent(CreateBox(.{layout = .{child_layout = .TOP_TO_BOTTOM,
                                    sizing = .{Fit(), Grow()},
                                    padding = UI_WINDOW_PAD,
                                    child_gap = UI_WINDOW_GAP,
                                    child_align.x = .CENTER},
                         bg_color = Color32_RGBi(255,255,0),
                         border = .{color = UI_BORDER_BG,
                                    widths = UI_BORDER_WIDTH}}));

      CreateText("switches", .{text.color = UI_RED1}, regular_text);
      CreateText("?",  .{text.color = UI_RED1}, regular_text);
    }
  }
}

Layout :: #import,file "brick_ui_layout.jai"(Color=Color32);
#scope_file;
using Layout;
