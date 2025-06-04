#module_parameters(
  ColorType: Type = u32,
  V2Type: Type = struct {x, y: float;}
  );

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

SizeType :: enum u8 // @todo rename to Unit?
{
  NONE;
  PX;
  REM;
  /*EM; PT;*/
  TEXT;
};

Length :: struct
{
  type: SizeType;
  value: float;
};
LengthPx  :: (v: float) -> Length { return .{.PX, v}; }
LengthRem :: (v: float) -> Length { return .{.REM, v}; }

Length2 :: struct
{
  width, height: Length;
  #place width; array: [2] Length;
}
Length2Px  :: (wh: float) -> Length2 { return .{LengthPx(wh), LengthPx(wh)}; }
Length2Rem :: (wh: float) -> Length2 { return .{LengthRem(wh), LengthRem(wh)}; }
Length2Px  :: (w: float, h: float) -> Length2 { return .{LengthPx(w), LengthPx(h)}; }
Length2Rem :: (w: float, h: float) -> Length2 { return .{LengthRem(w), LengthRem(h)}; }
Length2Px  :: (vec: V2Type) -> Length2 { return .{LengthPx(vec.x), LengthPx(vec.y)}; }
Length2Rem :: (vec: V2Type) -> Length2 { return .{LengthRem(vec.x), LengthRem(vec.y)}; }

Size :: struct
{
  type: SizeType;
  min := -1.0;
  max := -1.0;
  grow_pct := -1.0;
  grow: Bool3; // expand to fit parent
  fit_children: Bool3; // expand to fit children
};

SizeTypeMin :: (type: SizeType, min: float) -> Size
{
  s: Size;
  s.type = type;
  s.min = min;
  return s;
}
SizeTypeMinMax :: (type: SizeType, min: float, max: float) -> Size
{
  s: Size;
  s.type = type;
  s.min = min;
  s.max = max;
  return s;
}

SizePx  :: (min: float) -> Size { return SizeTypeMin(.PX,  min); }
SizeRem :: (min: float) -> Size { return SizeTypeMin(.REM, min); }
SizePx  :: (min: float, max: float) -> Size { return SizeTypeMinMax(.PX,  min, max); }
SizeRem :: (min: float, max: float) -> Size { return SizeTypeMinMax(.REM, min, max); }
SizeText :: () -> Size { return .{.TEXT}; }

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

Size2 :: struct
{
  width, height: Size;
  #place width; array: [2] Size;
};
Size2Px :: (vec: V2Type) -> Size2 { return Size2.{SizePx(vec.x), SizePx(vec.y)}; }

BoxKey :: struct
{
  hash_or_something: u64;
};

CornerRadius :: struct
{
  // @todo replace float (px) with Size
  top_left, top_right, bottom_left, bottom_right: float;
};
CornerRadius_All :: (r: float) -> CornerRadius { return .{r, r, r, r}; }

BorderWidths :: struct
{
  // @todo replace float (px) with Size
  left, right, top, bottom: float;
  between_children: float;
};
BorderWidths_All   :: (w: float) -> BorderWidths { return .{w,w,w,w, w}; }
BorderWidths_Sides :: (w: float) -> BorderWidths { return .{w,w,w,w, 0}; }

Padding :: struct
{
  // @todo replace float (px) with Size
  left, right, top, bottom: float;
};

BoxSpec :: struct
{
  layout: struct
  {
    sizing: Size2;
    padding: Padding;
    child_gap: Size;
    child_align: struct { x: AlignX; y: AlignY; };
    child_layout: enum { LEFT_TO_RIGHT; TOP_TO_BOTTOM; };
  };

  bg_color: ColorType;
  corner_radius: CornerRadius;

  floating: struct
  {
    attach_to: enum
    {
      NONE; // disables floating
      PARENT;
      ELEMENT_WITH_ID;
      ROOT;
    };

    offset: Length2;
    // expand: [Axis2.COUNT] Size; do we need this?
    parent: BoxKey;
    z_index: s16;
    attach_points: struct { element: AttachPoint; parent: AttachPoint; };
    cursor_passthrough: bool;
  };

  border: struct
  {
    color: ColorType;
    widths: BorderWidths;
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

BoxSpecFromLayer :: (layer: enum { MAIN; TEXT; }) -> BoxSpec
{
  // @impl
  return .{};
}

InheritBoxSpecs :: (child: BoxSpec, parents: ..BoxSpec) -> BoxSpec
{
  // @impl
  return child;
}

CreateText :: (text: string, spec := BoxSpec.{}) -> Signal
{
  merged_spec := InheritBoxSpecs(spec, BoxSpecFromLayer(.TEXT), BoxSpecFromLayer(.MAIN));
  spec.text.str = text;
  return CreateBoxDirect(spec);
}

CreateBox :: (spec: BoxSpec) -> Signal
{
  merged_spec := InheritBoxSpecs(spec, BoxSpecFromLayer(.MAIN));
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
