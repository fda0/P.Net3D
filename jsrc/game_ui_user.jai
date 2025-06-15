UI_State :: struct
{
  dev: struct
  {
    show := true;
    win_p: V2;
    win_dim: V2;

    show_stats: bool;
    noclip: bool;
    sun_camera: bool;
    show_colliders: bool;
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
UI_BUTTON_BG :: #run Color32_RGBi(80, 73, 69);
UI_BUTTON_HOVER_BG :: #run Color32_RGBi(102, 92, 84);
UI_SHADOW    :: #run Color32_RGBAi(29, 32, 33, 96);
UI_DARKER    :: #run Color32_RGBAi(0, 0, 0, 64);

UI_RADIUS :: #run RadiusAll(Px(3));
UI_BORDER_WIDTH :: #run BorderOutside(Px(1));
UI_WINDOW_GAP :: #run Rem(0.5);
UI_WINDOW_PAD :: #run PaddingAll(UI_WINDOW_GAP);
UI_BUTTON_PAD :: #run Padding.{UI_WINDOW_GAP, UI_WINDOW_GAP, UI_WINDOW_GAP*0.5, UI_WINDOW_GAP*0.5};

UI_HEADER_TEXT  :: #run BoxSpec.{text = .{font = xx FONT_Family.PlayfairDisplay, font_size = Rem(1.5),}};
UI_REGULAR_TEXT :: #run BoxSpec.{text = .{font = xx FONT_Family.PlayfairDisplay, font_size = Rem(1)}};
UI_SMALL_TEXT   :: #run BoxSpec.{text = .{font = xx FONT_Family.PlayfairDisplay, font_size = Rem(0.8)}};
UI_FUNKY_TEXT   :: #run BoxSpec.{text = .{font = xx FONT_Family.Jacquard24,      font_size = Rem(1.5)}};
UI_TEXT_PAD     :: #run BoxSpec.{layout.padding = PaddingV(Em(0.1), Em(0.4))};

UI_DeveloperKeyShortcuts :: ()
{
  if (KEY_Pressed(.GRAVE))
    G.debug.show_debug_window = !G.debug.show_debug_window;

  if (KEY_Pressed(.RETURN))
    G.ui.dev.noclip = !G.ui.dev.noclip;

  if (KEY_Pressed(.BACKSPACE))
    G.ui.dev.sun_camera = !G.ui.dev.sun_camera;

  // move camera; calculate camera scale
  if G.ui.dev.noclip
  {
    if (KEY_Held(.MouseLeft) && !G.debug.click_id)
    {
      rot_speed := 0.0004;
      G.camera_angles.z += rot_speed * G.mouse_delta.x;
      G.camera_angles.y += rot_speed * G.mouse_delta.y * (-2.0 / 3.0);
      G.camera_angles.z = WrapFloat(-0.5, 0.5, G.camera_angles.z);
      G.camera_angles.y = clamp(G.camera_angles.y, -0.2, 0.2);
    }

    move_dir: V3;
    if (KEY_Held(.W)) move_dir.x += 1;
    if (KEY_Held(.S)) move_dir.x -= 1;
    if (KEY_Held(.A)) move_dir.y += 1;
    if (KEY_Held(.D)) move_dir.y -= 1;
    if (KEY_Held(.SPACE)) move_dir.z += 1;
    if (KEY_Held(.LSHIFT) || KEY_Held(.RSHIFT)) move_dir.z -= 1;
    move_dir = normalize(move_dir);
    move_dir = Math.rotate(move_dir, RotationAroundAxis(-AxisV3(.Z)/* @todo why negation fixes things? */, G.camera_angles.z));
    G.camera_p += move_dir * 4.0 * G.dt;
  }
}

UI_BuildDeveloperWindow :: ()
{
  using G.ui.dev;
  if (!show) return;

  Layer(.{text.color = UI_FG});
  Layer(UI_REGULAR_TEXT);

  clamped_win_p := clamp(win_p, .{}, G.window_dim * 0.9);

  // Floating developer window
  Parent(CreateBox(.{layout = .{direction = .TOP_TO_BOTTOM,
                                sizing = .{width = Fit(SizeMin(Rem(30))), height = Size(Rem(30))}},
                     bg_color = UI_BG,
                     floating = .{attach_to = .ROOT, offset = Px(clamped_win_p)},
                     border = .{color = UI_SHADOW, widths = UI_BORDER_WIDTH},
                     corner_radius = UI_RADIUS}));

  // Window bar
  {
    window_bar := Signal(Parent(CreateBox(.{layout = .{sizing = .{width = Grow(), height = Fit()},
                                                       padding = PaddingAll(Rem(0.4)),
                                                       child_align = .{x = .CENTER, y = .CENTER}},
                                            bg_color = UI_ORANGE2,
                                            corner_radius = UI_RADIUS})));
    if window_bar.flags & .DRAG  win_p += G.mouse_delta;
    else                         win_p = clamped_win_p;

    CreateText("Developer window", .{text.color = UI_BG});
  }

  // Content body
  {
    Parent(CreateBox(.{layout = .{sizing = .{Grow(), Grow()}}}));

    // Category vertical bar
    {
      Parent(CreateBox(.{layout = .{direction = .TOP_TO_BOTTOM,
                                    sizing = .{Fit(), Grow()},
                                    padding = UI_WINDOW_PAD,
                                    child_gap = UI_WINDOW_GAP,
                                    child_align.x = .CENTER},
                         border = .{color = UI_BORDER_BG,
                                    widths = UI_BORDER_WIDTH}}));

      CreateText("switches");
      CreateText("?");
    }

    // Menu content
    {
      Parent(CreateBox(.{layout = .{direction = .TOP_TO_BOTTOM,
                                    sizing = .{Grow(), Fit()},
                                    padding = UI_WINDOW_PAD},
                         corner_radius = UI_RADIUS}));

      CreateText("Debug switches", UI_HEADER_TEXT, UI_TEXT_PAD);

      submenu := Merge(UI_SMALL_TEXT, .{layout.padding = .{UI_WINDOW_GAP, UI_WINDOW_GAP, Em(0.2), Em(0.2)}});

      UI_Checkbox("Show stats", *show_stats);
      if show_stats
      {
        Layers(submenu);
        CreateText(tprint("World Camera: %", G.camera_p));
        CreateText(tprint("Camera - Player: %", G.camera_p - OBJ_Get(G.client.player_key, .NETWORK).s.p));
        CreateText(tprint("Camera angles: %", G.camera_angles));
      }

      UI_Checkbox("Noclip camera", *noclip);
      UI_Checkbox("Sun camera", *sun_camera);
      UI_Checkbox("Show colliders", *show_colliders);
      UI_Checkbox("Render normals", null);
    }
  }
}

// UI element library
UI_Checkbox :: (label: string, checkbox_value: *bool)
{
  button := Signal(CreateBox(.{layout = .{sizing = .{Grow(), Fit()},
                                          padding = UI_BUTTON_PAD,
                                          child_align.y = .CENTER,
                                          child_gap = UI_WINDOW_GAP}}));
  Parent(button);

  if button.flags & .HOVER then button.bg_color = Color32_SetAlphai(UI_YELLOW1, 128);

  square_text := "";
  if checkbox_value
  {
    if button.flags & .CLICK then checkbox_value.* = !checkbox_value.*;
    square_text = ifx checkbox_value.* then "X" else "";
  }
  else square_text = "/";

  {
    square_length := Rem(1.5);
    Parent(CreateBox(.{layout = .{sizing = Size2(square_length),
                                  child_align = .{x = .CENTER, y = .CENTER}},
                       bg_color = UI_BUTTON_BG,
                       border = .{color = UI_FG, widths = UI_BORDER_WIDTH},
                       corner_radius = UI_RADIUS},));
    CreateText(square_text, .{text.font_size = square_length});
  }
  CreateText(label);
}

Layout :: #import,file "brick_ui_layout.jai"(Color=Color32);
#scope_file;
using Layout;
