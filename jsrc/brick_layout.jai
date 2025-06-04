#module_parameters(ColorType: Type = u32);

Axis2 :: enum u8 { X; Y; COUNT; };
Inverse :: (a: Axis2) -> Axis2 { return ifx a == .X then .Y else .X; }

Side2 :: enum u8 { LEFT; RIGHT; TOP; BOTTOM; COUNT; };
Corner2 :: enum u8 { TOP_LEFT; TOP_RIGHT; BOTTOM_LEFT; BOTTOM_RIGHT; COUNT; };

AttachPoint :: enum u8
{
  LEFT_TOP; LEFT_CENTER; LEFT_BOTTOM;
  CENTER_TOP; CENTER_CENTER; CENTER_BOTTOM;
  RIGHT_TOP; RIGHT_CENTER; RIGHT_BOTTOM;
};

AlignX :: enum u8 { LEFT; RIGHT; CENTER; };
AlignY :: enum u8 { TOP; BOTTOM; CENTER; };
Bool3 :: enum u8 { DEFAULT; FALSE; TRUE; };

Size :: struct
{
  type: enum
  {
    NONE;
    PX;
    REM;
    /*EM; PT;*/
    TEXT;
  };

  min := -1.0;
  max := -1.0;
  grow_pct := -1.0;
  grow: Bool3; // expand to fit parent
  fit_children: Bool3; // expand to fit children
};

Px  :: (min: float) -> Size { return .{.PX,  min}; }
Rem :: (min: float) -> Size { return .{.REM, min}; }
Px  :: (min: float, max: float) -> Size { return .{.PX,  min, max}; }
Rem :: (min: float, max: float) -> Size { return .{.REM, min, max}; }

Text :: () -> Size { return .{.TEXT}; }
Pct :: (percentage_of_parent: float, size := Size.{}) -> Size
{
  s := size;
  s.grow_pct = percentage_of_parent;
  s.grow = .TRUE;
  return s;
}
Grow :: (size := Size.{}) -> Size
{
  s := size;
  s.grow = .TRUE;
  return s;
}
Fit :: (size := Size.{}) -> Size
{
  s := size;
  s.fit_children = .TRUE;
  return s;
}

BoxKey :: struct
{
  hash_or_something: u64;
};

Sizing :: struct
{
  width: Size;
  height: Size;
  #place width; array: [Axis2.COUNT] Size;
};

BoxSpec :: struct
{
  layout: struct
  {
    sizing: [Axis2.COUNT] Size;
    padding: [Side2.COUNT] Size;
    child_gap: Size;
    child_align: struct { x: AlignX; y: AlignY; };
    child_layout: enum { LEFT_TO_RIGHT; TOP_TO_BOTTOM; };
  };

  bg_color: ColorType;
  corner_radius: [Corner2.COUNT] float;

  floating: struct
  {
    attach_to: enum
    {
      NONE; // disables floating
      PARENT;
      ELEMENT_WITH_ID;
      ROOT;
    };

    offset: [Axis2.COUNT] Size;
    // expand: [Axis2.COUNT] Size; do we need this?
    parent: BoxKey;
    z_index: s16;
    attach_points: struct { element: AttachPoint; parent: AttachPoint; };
    cursor_passthrough: bool;
  };

  border: struct
  {
    color: ColorType;
    widths: struct
    {
      sides: [Side2.COUNT] Size;
      between_children: Size;
    };
  };

  text: struct
  {
    str: string;
    color: ColorType;
    font: s16;
    pt_size: float;
  };
};

Box :: struct
{
  payload: u64;
};

SignalFlags :: enum_flags u32
{
  CLICK; DOUBLE_CLICK; TRIPLE_CLICK;
  RIGHT_CLICK; RIGHT_DOUBLE_CLICK; RIGHT_TRIPLE_CLICK;
  DRAG;
};

Signal :: struct
{
  box: *Box;
  flags: SignalFlags;
};

CreateText :: (text: string, spec := BoxSpec.{}) -> Signal
{
  merged_spec := InheritSpecs(spec, LayerTopSpec(.TEXT), LayerTopSpec(.MAIN));
  spec.text.str = text;
  return CreateBoxDirect(spec);
}

CreateBox :: (spec: BoxSpec) -> Signal
{
  merged_spec := InheritSpecs(spec, LayerTopSpec(.MAIN));
  return CreateBoxDirect(spec);
}

CreateBoxDirect :: (spec: BoxSpec) -> Signal
{
  return .{};
}

Parent :: (box: Box)
{
  // 1. take signal.box and push it on parent stack
}

Parent :: (signal: Signal) -> Signal
{
  // 1. take signal.box and push it on parent stack
  return signal;
}

State :: struct
{
  todo: int;
};
