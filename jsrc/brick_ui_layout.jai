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

Unit :: enum u8
{
  PX;
  REM;
  /*EM; PT;*/
};

Length :: struct
{
  unit: Unit;
  value: float = -1.0;
};
Px  :: (v: float) -> Length { return .{.PX, v}; }
Rem :: (v: float) -> Length { return .{.REM, v}; }

Length2 :: struct
{
  x, y: Length;
  #place x; axes: [2] Length;
}
Px  :: (v: Vector2) -> Length2 { return .{Px(v.x), Px(v.y)}; }
Rem :: (v: Vector2) -> Length2 { return .{Rem(v.x), Rem(v.y)}; }

BoxSize :: struct
{
  min: Length;
  max: Length;

  fit_children: Bool3; // Measure sum of children sizes and try to match their size.
  measure_text: Bool3; // Measure containing text and try to match its size.
  percent_of_parent := -1.0; // Try to have size that's equal to some exact percentage of parent box.
  grow: Bool3; // Try to grow to fill size left in parent box.

  #place min; minmax: [2] Length;
};

SizeMin :: (min: Length) -> BoxSize
{
  s: BoxSize;
  s.min = min;
  return s;
}
SizeMax :: (max: Length) -> BoxSize
{
  s: BoxSize;
  s.max = max;
  return s;
}
Size :: (min: Length, max: Length) -> BoxSize // @todo this needs a shorter name
{
  s: BoxSize;
  s.min = min;
  s.max = max;
  return s;
}
Size :: (minmax: Length) -> BoxSize
{
  s: BoxSize;
  s.min = minmax;
  s.max = minmax;
  return s;
}

Text :: () -> BoxSize { return .{.TEXT}; }

Pct :: (percentage_of_parent: float, size := BoxSize.{}) -> BoxSize
{
  s := size;
  s.percent_of_parent = percentage_of_parent;
  s.grow = .TRUE;
  return s;
}
Grow :: (size := BoxSize.{}) -> BoxSize
{
  s := size;
  s.grow = .TRUE;
  return s;
}
Fit :: (size := BoxSize.{}) -> BoxSize
{
  s := size;
  s.fit_children = .TRUE;
  return s;
}

BoxSize2 :: struct
{
  width, height: BoxSize;
  #place width; axes: [2] BoxSize;
};
// Make Size2 out of Length2
Size2Min :: (min: Length2) -> BoxSize2 { return BoxSize2.{SizeMin(min.x), SizeMin(min.y)}; }
Size2Max :: (max: Length2) -> BoxSize2 { return BoxSize2.{SizeMax(max.x), SizeMax(max.y)}; }
Size2 :: (min: Length2, max: Length2) -> BoxSize2 { return BoxSize2.{Size(min.x, max.x), Size(min.y, max.y)}; }
Size2 :: (minmax: Length2) -> BoxSize2 { return BoxSize2.{Size(minmax.x, minmax.x), Size(minmax.y, minmax.y)}; }
// Make Size2 out of Length
Size2Min :: (min: Length) -> BoxSize2 { return BoxSize2.{SizeMin(min), SizeMin(min)}; }
Size2Max :: (max: Length) -> BoxSize2 { return BoxSize2.{SizeMax(max), SizeMax(max)}; }
Size2 :: (min: Length, max: Length) -> BoxSize2 { return BoxSize2.{Size(min, max), Size(min, max)}; }
Size2 :: (minmax: Length) -> BoxSize2 { return BoxSize2.{Size(minmax, minmax), Size(minmax, minmax)}; }

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
    sizing: BoxSize2;
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

  #place min_dim; minmax_dim: [2] Vector2;
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

  root_box = CreateBox(.{layout = .{sizing = Size2(Px(root_dimensions_in_pixels))}}).box;
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
    // Pass 1.
    //  1. Calculate standalone sizes (px, rem, text).
    //  2. Calculate values for measure_text, percent_of_parent, grow (+ temp value for fit_children).
    for :TreeBreadthFirst root_box.first
    {
      using it.spec;
      using it.spec.layout;
      size := sizing.axes[axis];

      // 1. Calculate standalone sizes (px, rem, text)
      for r: 0..1 // Resolve values for min(r=0) and max(r=1)
      {
        length := size.minmax[r];
        value_px := length.value;
        if value_px >= 0.0
        {
          if length.unit == {
            case .REM; value_px *= 16.0;
          }
        }
        it.minmax_dim[r].component[axis] = value_px;
      }
      // Default to maximal size (it will be decreased in later stages if overflow happens).
      it.final_dim.component[axis] = max(it.min_dim.component[axis], it.max_dim.component[axis]);


      // 2. Calculate values for measure_text, percent_of_parent, grow (+ temp value for fit_children).
      new_dim := -1.0;
      parent_dim := it.parent.final_dim.component[axis];
      if size.fit_children == .TRUE
      {
        // This will be overwriten by the next stage;
        // For now assign parent_dim to provide some reasonable behavior for cofigurations
        // where parent uses `fit_children` and child uses `grow`/`percentage_of_parent`.
        new_dim = parent_dim;
      }
      if size.measure_text == .TRUE
      {
        text_dim := Vector2.{text.str.count.(float) * 16.0, 32.0}; // @impl calculate text dimensions
        new_dim = text_dim.component[axis];
      }
      if size.percent_of_parent >= 0.0
      {
        new_dim = parent_dim*size.percent_of_parent;
      }
      if size.grow == .TRUE
      {
        parent_dim := it.parent.final_dim.component[axis];
        new_dim = parent_dim;
      }

      if new_dim >= 0.0
      {
        it.final_dim.component[axis] = new_dim;
      }
    }

    // Pass 2.
    //   1. Calculate sizes dependent on children (fit).
    //   2. Clamp final_dim to min_dim and max_dim.
    for :TreeDepthFirst root_box.first
    {
      using it.spec.layout;
      size := sizing.axes[axis];

      // 1. Calculate sizes dependent on children (fit).
      if size.fit_children == .TRUE
      {
        sum: float;
        for :Siblings child: it.first
          sum += child.final_dim.component[axis];

        it.final_dim.component[axis] = sum;
      }

      // 2. Clamp final_dim to min_dim and max_dim.
      if it.max_dim.component[axis] >= 0.0
        it.final_dim.component[axis] = min(it.max_dim.component[axis], it.final_dim.component[axis]);

      if it.min_dim.component[axis] >= 0.0
        it.final_dim.component[axis] = max(it.min_dim.component[axis], it.final_dim.component[axis]);
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

Siblings :: (first_box: *Box, body: Code, flags: For_Flags) #expand
{
  #assert !(flags & (.REVERSE | .POINTER)); // Support for .REVERSE could be added.

  box := first_box;
  box_index := 0;

  while !IsNil(box)
  {
    `it := box;
    `it_index := box_index;
    box = box.next;
    box_index += 1;

    #insert body;
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
