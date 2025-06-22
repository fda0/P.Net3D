UI_State :: struct
{
  dev: struct
  {
    show := true;
    win_p: V2;

    category: enum {switches; stats; assets; network;};

    // switches
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

UI_HEADER_TEXT  :: #run BoxSpec.{text = .{font = xx FONT_Family.PlayfairDisplay, font_size = Rem(1.5)},
                                 layout.padding = PaddingV(Rem(0.2), Rem(0.6))};
UI_REGULAR_TEXT :: #run BoxSpec.{text = .{font = xx FONT_Family.PlayfairDisplay, font_size = Rem(1)}};
UI_MONO_TEXT    :: #run BoxSpec.{text = .{font = xx FONT_Family.Inconsolata, font_size = Rem(0.85)},
                                 layout.padding = PaddingV(Em(0.4))};

UI_DeveloperKeyShortcuts :: ()
{
  using G.ui.dev;
  if (KEY_Pressed(.GRAVE)) then show = !show;
  if (KEY_Pressed(.RETURN)) then noclip = !noclip;
  if (KEY_Pressed(.BACKSPACE)) then sun_camera = !sun_camera;
}

UI_BuildDeveloperWindow :: ()
{
  using G.ui.dev;
  if !show return;

  Layer(.{text.color = UI_FG});
  Layer(UI_REGULAR_TEXT);

  clamped_win_p := clamp(win_p, .{}, G.window_dim * 0.9);

  // Floating developer window
  Parent(CreateBox(.{layout = .{direction = .TOP_TO_BOTTOM,
                                sizing = .{width = Fit(SizeMin(Rem(35))), height = Size(Rem(30))}},
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
    Parent(CreateBox(.{layout = .{sizing = .{Grow(Fit()), Grow()}}}));
    category_names := enum_names(type_of(category));
    category_values := enum_values_as_enum(type_of(category));

    // Category vertical bar
    {
      Parent(CreateBox(.{layout = .{direction = .TOP_TO_BOTTOM,
                                    sizing = .{Fit(), Grow()},
                                    padding = UI_WINDOW_PAD,
                                    child_gap = UI_WINDOW_GAP,
                                    child_align.x = .CENTER},
                         border = .{color = UI_BORDER_BG,
                                    widths = UI_BORDER_WIDTH}}));
      for category_names
      {
        UI_Button(it, *category, category_values[it_index]);
      }
    }

    // Content box
    Parent(CreateBox(.{layout = .{direction = .TOP_TO_BOTTOM,
                                  sizing = .{Grow(Fit()), Fit()},
                                  padding = UI_WINDOW_PAD},
                       corner_radius = UI_RADIUS}));

    // Header
    CreateText(category_names[category], UI_HEADER_TEXT);

    // Per category body
    if #complete category == {
      case .switches;
      UI_Checkbox("Noclip camera", *noclip);
      UI_Checkbox("Sun camera", *sun_camera);
      UI_Checkbox("Show colliders", *show_colliders);
      UI_Checkbox("Render normals", null);

      case .stats;
      Layers(UI_MONO_TEXT);
      CreateText(tprint("dt: %ms; fps: %", Round(G.dt*1000), Round(1.0 / G.dt)));
      CreateText(tprint("World Camera: %", G.camera_p));
      CreateText(tprint("Camera - Player: %", G.camera_p - OBJ_Get(G.client.player_key, .NETWORK).s.p));
      CreateText(tprint("Camera angles: %", G.camera_angles));

      case .assets;
      Layers(UI_MONO_TEXT);
      CreateText("what got loaded from .pie file?");
      CreateText("hot-reload state");

      case .network;
      Layers(UI_MONO_TEXT);
      CreateText("network stuff here");
    }
  }
}

// UI element library
UI_Checkbox :: (label: string, checkbox_value: *bool)
{
  Layer(.{autokey_seed = Hash64Any(label, checkbox_value.(u64))});

  button := Signal(CreateBox(.{layout = .{sizing = .{Grow(), Fit()},
                                          padding = UI_BUTTON_PAD,
                                          child_align.y = .CENTER,
                                          child_gap = UI_WINDOW_GAP}}));
  Parent(button);

  if button.flags & .HOVER then button.bg_color = UI_BUTTON_HOVER_BG;

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

UI_Button :: (label: string, button_value: *$T, set_value: T)
{
  Layer(.{autokey_seed = Hash64Any(label, button_value.(u64), set_value)});

  button := Signal(CreateBox(.{layout = .{sizing = .{Grow(Fit()), Fit()},
                                          padding = UI_BUTTON_PAD,
                                          child_align.x = .RIGHT,
                                          child_align.y = .CENTER,
                                          child_gap = UI_WINDOW_GAP}}));
  Parent(button);

  button.bg_color = UI_BUTTON_BG;
  if button.flags & .HOVER then button.bg_color = UI_BUTTON_HOVER_BG;

  label_box := CreateText(label);

  if button_value
  {
    if button.flags & .CLICK then button_value.* = set_value;
    if button_value.* == set_value
    {
      button.bg_color = UI_FG;
      label_box.text.color = UI_BG;
    }
  }
  else label_box.text.color = UI_RED2;
}

Layout :: #import,file "brick_ui_layout.jai"(Color=Color32);
#scope_file;
using Layout;
