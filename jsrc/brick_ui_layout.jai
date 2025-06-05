#module_parameters(
  ColorType: Type = u32,
  Vector2: Type = Math.Vector2,
  HashLocFunc := DefaultHashLocFunc,
  MAX_NESTING := 256)
{
  Math :: #import "Math";
  DefaultHashLocFunc :: (loc: Source_Code_Location) -> u64
  {
    Hash :: #import "Hash";
    h := Hash.fnv1a_hash(loc.fully_pathed_filename.data, loc.fully_pathed_filename.count);
    h  = Hash.fnv1a_hash(xx,no_check loc.line_number, h);
    h  = Hash.fnv1a_hash(xx,no_check loc.character_number, h);
    return h;
  }
}

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
Length2Px  :: (vec: Vector2) -> Length2 { return .{LengthPx(vec.x), LengthPx(vec.y)}; }
Length2Rem :: (vec: Vector2) -> Length2 { return .{LengthRem(vec.x), LengthRem(vec.y)}; }

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
Size2Px :: (vec: Vector2) -> Size2 { return Size2.{SizePx(vec.x), SizePx(vec.y)}; }

BoxKey :: struct
{
  hash: u64;
};

CreateKey :: (loc: Source_Code_Location) -> BoxKey
{
  hash := HashLocFunc(loc);
  key := BoxKey.{hash};
  return key;
}

CreateKey :: (text: string) -> BoxKey
{
  hash := Hash64Any(text);
  key := BoxKey.{hash};
  return key;
}

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
  left, right, top, bottom: Length;
};
Padding_All :: (l: Length) -> Padding { return .{l,l,l,l}; }

BoxSpec :: struct
{
  key: BoxKey;

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
  using spec: BoxSpec;
  next: *#this;
  prev, first, last, parent: *Box;
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

InheritBoxSpecs :: (child_spec: BoxSpec, parent_specs: ..BoxSpec) -> BoxSpec
{
  res := child_spec;
  for parent_specs
  {
    // @impl
  }
  return res;
}

CreateText :: (text: string, spec := BoxSpec.{}, loc := #caller_location) -> Signal
{
  merged_spec := InheritBoxSpecs(spec, BoxSpecFromLayer(.TEXT), BoxSpecFromLayer(.MAIN));
  merged_spec.text.str = text;
  return CreateBoxDirect(merged_spec, loc);
}

CreateBox :: (spec: BoxSpec, loc := #caller_location) -> Signal
{
  merged_spec := InheritBoxSpecs(spec, BoxSpecFromLayer(.MAIN));
  return CreateBoxDirect(merged_spec, loc);
}


CreateBoxDirect :: (source_spec: BoxSpec, loc := #caller_location, is_root_box := false) -> Signal
{
  spec := source_spec;
  if !spec.key.hash  spec.key = CreateKey(loc);

  state.boxes_created_on_this_layout += 1;

  box := Basic.array_add(BoxesArray(.CURRENT));
  box.spec = spec;

  // links
  LinkToNil(box);
  if !is_root_box
  {
    box.parent = PeekParent();
    if IsNil(box.parent.first)
    {
      box.parent.first = box;
      box.parent.last = box;
    }
    else
    {
      box.prev = box.parent.last;
      box.parent.last.next = box;
      box.parent.last = box;
    }
  }

  // Find prev_box
  prev_box: *Box;
  {
    prev_boxes := BoxesArray(.PREVIOUS);
    for * prev_boxes.*
    {
      if it.key.hash == box.key.hash
      {
        prev_box = it;
        break;
      }
    }
  }

  if prev_box
  {
    // @impl copy some state from prev_box
  }

  signal: Signal;
  signal.box = box;
  return signal;
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

BoxesArray :: (which: enum {CURRENT; PREVIOUS;}) -> *[..] Box
{
  using state;
  index := ifx which == .CURRENT then current_boxes_index else xx !current_boxes_index;
  res := *boxes[index];
  return res;
}

PushParent :: (value: *Box)
{
  using state.stacks;
  parents_index += 1;
  i := Basic.clamp(parents_index, 0, MAX_NESTING-1);
  if (i != parents_index) Basic.log_error("[BRICK_UI_LAYOUT] Invalid parents_index: %", parents_index);
  parents[i] = value;
}
PopParent :: (value: *Box)
{
  using state.stacks;
  parents_index -= 1;
}
PeekParent :: () -> *Box
{
  using state.stacks;
  i := Basic.clamp(parents_index, 0, MAX_NESTING-1);
  if (i != parents_index) Basic.log_error("[BRICK_UI_LAYOUT] Invalid parents_index: %", parents_index);
  return parents[i];
}

State :: struct
{
  boxes_created_on_this_layout: s32;

  // Double-buffer system.
  // Each LayoutBegin call flips boxes_index between 0-1.
  // current_boxes_index points to boxes array used by current layout run.
  // Other boxes array points to boxes from the previous frame.
  current_boxes_index: u8;
  boxes: [2][..] Box; // @todo replace that with MAX_BOXES module parameter

  nil_box: Box;

  stacks: struct
  {
    parents: [MAX_NESTING] *Box;
    parents_index: s32;
  };
};
state: State;

InitState :: (state: *State)
{
  using state;
  LinkToNil(nil_box);
}

LinkToNil :: (box: *Box)
{
  using state;
  box.next   = *nil_box;
  box.prev   = *nil_box;
  box.first  = *nil_box;
  box.last   = *nil_box;
  box.parent = *nil_box;
}

IsNil :: (box: *Box) -> bool { return box == *state.nil_box; }

LayoutBegin :: ()
{

}

DefaultHashLocFunc :: (loc: Source_Code_Location) -> u64
{
  // FNV-1a hash: http://www.isthe.com/chongo/tech/comp/fnv/index.html
  // Taken from modules/Hash.jai
  FNV_64_PRIME        : u64 : 0x100000001b3;
  FNV_64_OFFSET_BIAS  : u64 : 0xcbf29ce484222325;
  fnv1a_hash :: inline (val : u64, h := FNV_64_OFFSET_BIAS) -> u64 #no_aoc {
    h ^= val;
    return h * FNV_64_PRIME;
  }
  fnv1a_hash :: (data: *void, size: int, h := FNV_64_OFFSET_BIAS) -> u64 {
    for 0..size-1 #no_abc #no_aoc {
      h = fnv1a_hash((cast(*u8)data)[it], h);
    }
    return h;
  }

  hash := fnv1a_hash(loc.fully_pathed_filename.data, loc.fully_pathed_filename.count);
  hash = fnv1a_hash(loc.line_number, hash);
  hash = fnv1a_hash(loc.character_number, hash);
  return hash;
}

#scope_file;
Basic :: #import "Basic"; // for array_add, clamp, log_error
