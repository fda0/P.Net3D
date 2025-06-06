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
  grow_pct := -1.0; // expand to fit % of parent

  grow: Bool3; // expand to fit parent if there is free space available
  fit_children: Bool3; // expand to fit children
  measure_text: Bool3; // expand to fit containing text

  #place min; range: [2] float;
};

SizeMin :: (type: SizeType, min: float) -> Size
{
  s: Size;
  s.type = type;
  s.min = min;
  return s;
}
SizeMax :: (type: SizeType, max: float) -> Size
{
  s: Size;
  s.type = type;
  s.max = max;
  return s;
}
SizeMinMax :: (type: SizeType, min: float, max: float) -> Size
{
  s: Size;
  s.type = type;
  s.min = min;
  s.max = max;
  return s;
}

SizeMinPx  :: (min: float)             -> Size { return SizeMin(.PX, Min); }
SizeMaxPx  :: (max: float)             -> Size { return SizeMax(.PX, max); }
SizePx     :: (min: float, max: float) -> Size { return SizeMinMax(.PX, min, max); }
SizePx     :: (minmax: float)          -> Size { return SizeMinMax(.PX, minmax, minmax); }

SizeMinRem :: (min: float)             -> Size { return SizeMin(.REM, min); }
SizeMaxRem :: (max: float)             -> Size { return SizeMax(.REM, max); }
SizeRem    :: (min: float, max: float) -> Size { return SizeMinMax(.REM, min, max); }
SizeRem    :: (minmax: float)          -> Size { return SizeMinMax(.REM, minmax, minmax); }

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
  #place width; axes: [2] Size;
};
Size2MinPx :: (min: Vector2) -> Size2 { return Size2.{SizeMinPx(min.x), SizeMinPx(min.y)}; }
Size2MaxPx :: (max: Vector2) -> Size2 { return Size2.{SizeMaxPx(max.x), SizeMaxPx(max.y)}; }
Size2Px :: (min: Vector2, max: Vector2) -> Size2 { return Size2.{SizePx(min.x, max.x), SizePx(min.y, max.y)}; }
Size2Px :: (minmax: Vector2) -> Size2 { return Size2.{SizePx(minmax.x, minmax.x), SizePx(minmax.y, minmax.y)}; }

BoxKey :: struct
{
  hash: u64;
};
operator == :: (a: BoxKey, b: BoxKey) -> bool { return a.hash == b.hash; }
operator ! :: (a: BoxKey) -> bool { return !a.hash; }

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
    child_gap: Length;
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
    width: BorderWidths;
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
  next, prev, first, last, parent: *Box;
  loc: Source_Code_Location; // creation loc

  // in pixels
  min_dim: Vector2 = .{-1,-1};
  max_dim: Vector2 = .{-1,-1};
  final_dim: Vector2;
  rel_p: Vector2;
  abs_p: Vector2;

  #place min_dim; dim_range: [2] Vector2;
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

Parent :: (box: *Box) -> *Box #expand
{
  PushParent(box);
  `defer PopParent(box);
  return box;
}
Parent :: (signal: Signal) -> Signal #expand
{
  PushParent(signal.box);
  `defer PopParent(signal.box);
  return signal;
}
PushParent :: (value: *Box)
{
  using state;
  parents_index += 1;
  i := clamp(parents_index, 0, MAX_NESTING-1);
  if (i != parents_index) log_error("[BRICK_UI_LAYOUT] Invalid parents_index: %", parents_index);
  parents[i] = value;
}
PopParent :: (value: *Box)
{
  using state;
  parents_index -= 1;
}
PeekParent :: () -> *Box
{
  using state;
  i := clamp(parents_index, 0, MAX_NESTING-1);
  if (i != parents_index) log_error("[BRICK_UI_LAYOUT] Invalid parents_index: %", parents_index);
  return parents[i];
}

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
  if !spec.key  spec.key = CreateKey(loc);

  state.boxes_created_on_this_layout += 1;

  box := array_add(BoxesArray(.CURRENT));
  box.spec = spec;
  box.loc = loc;

  // links
  LinkToNil(box);
  parent := PeekParent();
  if !IsNil(parent)
  {
    box.parent = parent;
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
      if it.key == box.key
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

BoxesArray :: (which: enum {CURRENT; PREVIOUS;}) -> *[..] Box
{
  using state;
  index := ifx which == .CURRENT then current_boxes_index else xx !current_boxes_index;
  res := *boxes[index];
  return res;
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
IsNil :: (box: *Box) -> bool { return !box || box == *state.nil_box; }

InitState :: (state: *State)
{
}

State :: struct
{
  boxes_created_on_this_layout: s32;

  // Double-buffer system.
  // Each LayoutBegin call flips boxes_index between 0-1.
  // current_boxes_index points to boxes array used by current layout run.
  // Other boxes array points to boxes from the previous frame.
  current_boxes_index: u8;
  boxes: [2][..] Box; // @todo replace with HashTable

  nil_box: Box;
  root_box: *Box;

  parents: [MAX_NESTING] *Box; // @todo make this dynamic? Or no?
  parents_index: s32;
};
state: State; // @todo allow user to create a state and to attach it to context instead of using a global variable

LayoutBegin :: (root_dimensions_in_pixels: Vector2)
{
  using state;
  current_boxes_index = xx !current_boxes_index;
  array_reset_keeping_memory(BoxesArray(.CURRENT));

  nil_box = .{};
  LinkToNil(*nil_box);

  parents_index = 0;
  parents[0] = null;

  root_box = CreateBox(.{layout = .{sizing = Size2Px(root_dimensions_in_pixels)}}).box;
  PushParent(root_box);
}

LayoutEnd :: (print_stuff: bool)
{
  using state;

  // Validate that nil_box wasn't modified.
  // This wouldn't be needed if one could use read_only memory in jai for nil_box.
  {
    err := false;
    err |= nil_box.next   != *nil_box;
    err |= nil_box.prev   != *nil_box;
    err |= nil_box.first  != *nil_box;
    err |= nil_box.last   != *nil_box;
    err |= nil_box.parent != *nil_box;
    if err
    {
      log_error("[BRICK_UI_LAYOUT] error detected - nil_box was modified. Make sure you check with IsNil before modifying a box.");
      nil_box = .{};
      LinkToNil(*nil_box);
    }
  }

  if print_stuff
  {
    print("[TreeDepthFirst] of root_box\n");
    for :TreeDepthFirst root_box
    {
      for 0..it_depth-1 print("    ");
      print("(d %; i %) %\n", it_depth, it_index, it.loc);
    }
    print("[TreeBreadthFirst] of root_box\n");
    for :TreeBreadthFirst root_box
    {
      for 0..it_depth-1 print("    ");
      print("(d %; i %) %\n", it_depth, it_index, it.loc);
    }

    print("[TreeDepthFirst] of root_box.first\n");
    for :TreeDepthFirst root_box.first
    {
      for 0..it_depth-1 print("    ");
      print("(d %; i %) %\n", it_depth, it_index, it.loc);
    }
    print("[TreeBreadthFirst] of root_box.first\n");
    for :TreeBreadthFirst root_box.first
    {
      for 0..it_depth-1 print("    ");
      print("(d %; i %) %\n", it_depth, it_index, it.loc);
    }
  }

  for axis: Axis2.X .. Axis2.Y // @todo the code might be actually simpler and faster if we just do layout on 2 dimensions at the same time?
  {
    // Calculate standalone sizes (px, rem, text) & sizes dependent on parent (percent)
    for :TreeBreadthFirst root_box.first
    {
      using it.spec;
      using it.spec.layout;
      size := sizing.axes[axis];

      if size.type == {
        case .PX; #through;
        case .REM;
        for r: 0..1 // Resolve values for min(r=0) and max(r=1)
        {
          if size.range[r] < 0.0 continue;
          coef := 1.0;
          if size.type == {
            case .REM; coef = 16.0;
          }
          it.dim_range[r].component[axis] = size.range[r]*coef;
        }
        it.final_dim.component[axis] = max(it.min_dim.component[axis], it.max_dim.component[axis]);

        case .TEXT;
        text_dim := Vector2.{text.str.count.(float) * 16.0, 32.0}; // @impl calculate text dimensions
        it.final_dim.component[axis] = text_dim.component[axis];
      }

      // @todo percentage / grow here...
    }

    // Calculate sizes dependent on children (fit)
    for :TreeDepthFirst root_box.first
    {
    }

    // Solve overflow violations
    for :TreeBreadthFirst root_box.first
    {
    }

    // Compute relative and absolute positions
    // (Could be merged with the step above)
    for :TreeBreadthFirst root_box.first
    {
    }
  }
}

TreeDepthFirst :: (root: *Box, body: Code, flags: For_Flags) #expand
{
  #assert !(flags & (.REVERSE | .POINTER)); // Support for .REVERSE could be added but it shouldn't be needed.

  /* Example traversal order for a tree with 10 boxes.
        [10 (root)]
        /    |    \
       2     6     9
      /|    /|\    |\
     0 1   3 4 5   7 8
  */

  box := root;
  box_depth := 0;
  box_index := 0;

  coming_from_child_to_parent := false;
  while root_loop := true // Parent loop
  {
    siblings_and_children_not_found := false;
    while !siblings_and_children_not_found // Child/Sibling loop
    {
      if !coming_from_child_to_parent
      {
        // Descend depth-first to 'youngest' child
        while !IsNil(box.first)
        {
          box_depth += 1;
          box = box.first;
        }
      }

      `it := box;
      `it_depth := box_depth;
      `it_index := box_index;
      box_index += 1;

      defer {
        // Go to the next sibling or exit and go to parent
        coming_from_child_to_parent = false;
        if !IsNil(box.next) box = box.next;
        else siblings_and_children_not_found = true;
      };

      #insert(break=break root_loop) body;
    }

    // Ascend to parent or exit the tree traversal
    if !IsNil(box.parent) && box_depth > 0
    {
      coming_from_child_to_parent = true;
      box_depth -= 1;
      box = box.parent;
    }
    else break;
  }
}

TreeBreadthFirst :: (root: *Box, body: Code, flags: For_Flags) #expand
{
  #assert !(flags & (.REVERSE | .POINTER)); // Support for .REVERSE could be added but it shouldn't be needed.

  /* Example traversal order for a tree with 10 boxes.
        [ 0 (root)]
        /    |    \
       1     4     8
      /|    /|\    |\
     2 3   5 6 7   9 10
  */

  box := root;
  box_depth := 0;
  box_index := 0;

  look_for_sibling := false;
  while root_loop := true // Parent loop
  {
    while true // Sibling/Child loop
    {
      if look_for_sibling
      {
        look_for_sibling = false; // Look for sibling only once until requested again
        // Go to the next sibling or exit loop
        if !IsNil(box.next) box = box.next;
        else break;
      }

      `it := box;
      `it_depth := box_depth;
      `it_index := box_index;
      box_index += 1;

      defer {
        // Go to the child or look_for_sibling
        if !IsNil(box.first)
        {
          box_depth += 1;
          box = box.first;
        }
        else look_for_sibling = true;
      };

      #insert(break=break root_loop) body;
    }

    // Ascend to parent or exit the tree traversal
    if !IsNil(box.parent) && box_depth > 0
    {
      look_for_sibling = true;
      box_depth -= 1;
      box = box.parent;
    }
    else break;
  }
}

#scope_file;
#import "Basic"; // for array_add, clamp, log_error
