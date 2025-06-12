#module_parameters(
  Color: Type = u32,
  V2: Type = Vector2,
  HashLocFunc := DefaultHashLocFunc,
  MAX_NESTING := 512)
{
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
AxisMatchesLayoutDirection :: inline (axis: Axis2, lay_dir: LayoutDirection) -> bool
{
  matches := false;
  matches |= axis == .X && (lay_dir == .DEFAULT || lay_dir == .LEFT_TO_RIGHT);
  matches |= axis == .Y && lay_dir == .TOP_TO_BOTTOM;
  return matches;
}

Side2 :: enum u8 { LEFT; RIGHT; TOP; BOTTOM; COUNT; };
Corner2 :: enum u8 { TOP_LEFT; TOP_RIGHT; BOTTOM_LEFT; BOTTOM_RIGHT; COUNT; };

AttachPoint :: enum u8
{
  LEFT_TOP; LEFT_CENTER; LEFT_BOTTOM;
  CENTER_TOP; CENTER_CENTER; CENTER_BOTTOM;
  RIGHT_TOP; RIGHT_CENTER; RIGHT_BOTTOM;
};

Align :: enum u8
{
  DEFAULT :: 0;
  LEFT    :: 1;
  TOP     :: 1;
  CENTER  :: 2;
  RIGHT   :: 3;
  BOTTOM  :: 3;
};
Bool3 :: enum u8 { DEFAULT; FALSE; TRUE; };

Unit :: enum u8
{
  PX;
  REM;
  EM;
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
Px  :: (v: V2) -> Length2 { return .{Px(v.x), Px(v.y)}; }
Rem :: (v: V2) -> Length2 { return .{Rem(v.x), Rem(v.y)}; }

BoxSize :: struct
{
  min: Length;
  max: Length;

  fit_children: Bool3; // Measure sum of children sizes and try to match their size.
  measure_text: Bool3; // Measure containing text and try to match its size.
  percent_of_parent := -1.0; // Try to have size that's equal to some exact percentage of parent box.
  grow: Bool3; // Try to grow to fill size left in parent box.

  #place min; range: [2] Length;
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

Fit :: (size := BoxSize.{}) -> BoxSize
{
  s := size;
  s.fit_children = .TRUE;
  return s;
}
MeasureText :: (size := BoxSize.{}) -> BoxSize
{
  s := size;
  s.measure_text = .TRUE;
  return s;
}
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

BoxSize2 :: struct
{
  width, height: BoxSize;
  #place width; axes: [2] BoxSize;
};
// Make Size2 out of Length2
Size2Min :: (min: Length2) -> BoxSize2 { return BoxSize2.{SizeMin(min.x), SizeMin(min.y)}; }
Size2Max :: (max: Length2) -> BoxSize2 { return BoxSize2.{SizeMax(max.x), SizeMax(max.y)}; }
Size2 :: (min: Length2, max: Length2) -> BoxSize2 { return BoxSize2.{Size(min.x, max.x), Size(min.y, max.y)}; }
Size2 :: (range: Length2) -> BoxSize2 { return BoxSize2.{Size(range.x, range.x), Size(range.y, range.y)}; }
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
  top_left, top_right, bottom_left, bottom_right: Length;
};
CornerRadius_All :: (r: Length) -> CornerRadius { return .{r, r, r, r}; }

BorderWidths :: struct
{
  left, right, top, bottom: Length;
  between_children: Length;
};
BorderWidths_All   :: (w: Length) -> BorderWidths { return .{w,w,w,w, w}; }
BorderWidths_Sides :: (w: Length) -> BorderWidths { return .{w,w,w,w, .{}}; }

Padding :: struct
{
  left, right, top, bottom: Length;
  #place left; axes: [2] struct
  {
    lower, upper: Length;
    #place lower; ends: [2] Length;
  };
};
Padding_All :: (l: Length) -> Padding { return .{l,l,l,l}; }

LayoutDirection :: enum { DEFAULT; LEFT_TO_RIGHT; TOP_TO_BOTTOM; };

PushLayer :: (spec: BoxSpec)
{
  using layout_state;
  if layers_count < layers.count
    layers[layers_count] = spec;
  layers_count += 1;
}
PopLayer :: ()
{
  using layout_state;
  layers_count -= 1;
}
Layer :: (spec: BoxSpec) #expand
{
  PushLayer(spec);
  `defer PopLayer();
}

BoxSpec :: struct
{
  key: BoxKey;

  layout: struct
  {
    sizing: BoxSize2;
    padding: Padding;
    child_gap: Length;
    child_align: struct { x, y: Align; #place x; axes: [2] Align; };
    child_layout: LayoutDirection;
  };

  bg_color: Color;
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
    color: Color;
    widths: BorderWidths;
  };

  text: struct
  {
    str: string;
    color: Color;
    font: s32 = -1;
    font_size: float = -1;
  };
};

Box :: struct
{
  using spec: BoxSpec;
  next, prev, first, last, parent: *Box;
  loc: Source_Code_Location; // creation loc

  // in pixels
  inner_min: V2 = .{-1, -1};
  inner_max: V2 = .{-1, -1};
  inner_final: V2;

  padding: struct
  {
    left, right, top, bottom: float;
    #place left; axes: [2] struct
    {
      lower, upper: float;
      #place lower; ends: [2] float;
    };
  };

  rel_p: V2; // offset from outer dimensions of parent
  abs_p: V2; // offset from 0,0 of root box
};

SignalFlags :: enum_flags u32
{
  CLICK; DOUBLE_CLICK; TRIPLE_CLICK;
  RIGHT_CLICK; RIGHT_DOUBLE_CLICK; RIGHT_TRIPLE_CLICK;
  DRAG;
};

Signal :: struct
{
  using #as box: *Box;
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
  using layout_state;
  if parents_count < parents.count
    parents[parents_count] = value;
  parents_count += 1;
}
PopParent :: (value: *Box)
{
  using layout_state;
  parents_count -= 1;
}
PeekParent :: () -> *Box
{
  using layout_state;
  if parents_count > 0 && parents_count <= parents.count
    return parents[parents_count-1];
  return *nil_box;
}

Merge :: (c: *$T, p: T)
#modify { return T.(*Type_Info).type == .ENUM; }
{
  if c.* == .DEFAULT then c.* = p;
}
Merge :: (c: *Length, p: Length) {
  if c.value < 0.0 then c.* = p;
}
Merge :: (c: *s32, p: s32) {
  if c.* < 0 then c.* = p;
}
Merge :: (c: *float, p: float) {
  if c.* < 0.0 then c.* = p;
}
Merge :: (c: *Color, p: Color) {
  if c.* == 0 then c.* = p;
}
Merge :: (c: *BoxSize, p: BoxSize) {
  Merge(*c.min, p.min);
  Merge(*c.max, p.max);
  Merge(*c.fit_children, p.fit_children);
  Merge(*c.measure_text, p.measure_text);
  Merge(*c.percent_of_parent, p.percent_of_parent);
  Merge(*c.grow, p.grow);
}
Merge :: (c: *BoxSize2, p: BoxSize2) {
  Merge(*c.width, p.width);
  Merge(*c.height, p.height);
}
Merge :: (c: *Padding, p: Padding) {
  Merge(*c.left, p.left);
  Merge(*c.right, p.right);
  Merge(*c.top, p.top);
  Merge(*c.bottom, p.bottom);
}
Merge :: (c: *CornerRadius, p: CornerRadius) {
  Merge(*c.top_left,     p.top_left);
  Merge(*c.top_right,    p.top_right);
  Merge(*c.bottom_left,  p.bottom_left);
  Merge(*c.bottom_right, p.bottom_right);
}
Merge :: (c: *BorderWidths, p: BorderWidths) {
  Merge(*c.left,             p.left);
  Merge(*c.right,            p.right);
  Merge(*c.top,              p.top);
  Merge(*c.bottom,           p.bottom);
  Merge(*c.between_children, p.between_children);
}

Merge :: (c: *BoxSpec, p: BoxSpec) {
  Merge(*c.layout.sizing,        p.layout.sizing);
  Merge(*c.layout.padding,       p.layout.padding);
  Merge(*c.layout.child_gap,     p.layout.child_gap);
  Merge(*c.layout.child_align.x, p.layout.child_align.x);
  Merge(*c.layout.child_align.y, p.layout.child_align.y);
  Merge(*c.layout.child_layout,  p.layout.child_layout);

  Merge(*c.bg_color,      p.bg_color);
  Merge(*c.corner_radius, p.corner_radius);

  // floating isn't inherited - at least for now

  Merge(*c.border.color,  p.border.color);
  Merge(*c.border.widths, p.border.widths);

  Merge(*c.text.color,     p.text.color);
  Merge(*c.text.font,      p.text.font);
  Merge(*c.text.font_size, p.text.font_size);
}

Merge :: (child_spec: BoxSpec, parent_specs: ..BoxSpec) -> BoxSpec
{
  merged := child_spec;
  for parent_specs
    Merge(*merged, it);
  return merged;
}

ActiveLayers :: () -> [] BoxSpec
{
  using layout_state;
  result: [] BoxSpec;
  result.data = *layers[0];
  result.count = layers_count;
  return result;
}

CreateText :: (text: string, spec := BoxSpec.{}, to_merge_specs: ..BoxSpec, loc := #caller_location) -> Signal
{
  merged := spec;
  if to_merge_specs.count then merged = Merge(merged, ..to_merge_specs);
  merged = Merge(merged, ..ActiveLayers());

  if text.count then merged.text.str = text;

  {
    using merged.text;
    font_size = layout_state.rem_to_px;
  }

  {
    using merged.layout.sizing;
    if width.measure_text  == .DEFAULT then width.measure_text  = .TRUE;
    if height.measure_text == .DEFAULT then height.measure_text = .TRUE;
  }

  return CreateBoxDirect(merged, loc);
}

CreateBox :: (spec: BoxSpec, to_merge_specs: ..BoxSpec, loc := #caller_location) -> Signal
{
  merged := spec;
  if to_merge_specs.count then merged = Merge(merged, ..to_merge_specs);
  merged = Merge(merged, ..ActiveLayers());
  return CreateBoxDirect(merged, loc);
}

CreateBoxDirect :: (source_spec: BoxSpec, loc := #caller_location, is_root_box := false) -> Signal
{
  spec := source_spec;
  if !spec.key  spec.key = CreateKey(loc);
  if spec.text.str then spec.text.str = copy_temporary_string(spec.text.str);

  layout_state.boxes_created_on_this_layout += 1;

  box := array_add(BoxesArray(.CURRENT));
  box.spec = spec;
  box.loc = loc;

  // links
  LinkToNil(box);
  box.parent = PeekParent();
  if !IsNil(box.parent)
  {
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
  using layout_state;
  index := ifx which == .CURRENT then current_boxes_index else xx !current_boxes_index;
  res := *boxes[index];
  return res;
}

LinkToNil :: (box: *Box)
{
  using layout_state;
  box.next   = *nil_box;
  box.prev   = *nil_box;
  box.first  = *nil_box;
  box.last   = *nil_box;
  box.parent = *nil_box;
}
IsNil :: (box: *Box) -> bool { return !box || box == *layout_state.nil_box; }

LengthToPx :: (length: Length, owner_box: *Box, clamp_at_zero: bool) -> float
{
  using layout_state;
  value := length.value;
  if clamp_at_zero then value = max(value, 0.0);

  if length.unit == {
  case .EM;
    box := owner_box;
    while !IsNil(box)
    {
      if box.text.font_size
        return value * box.text.font_size;

      box = box.parent;
    }
    #through;

    case .REM;
    return value * rem_to_px;
  }
  return value;
}

Length2ToPx :: (len2: Length2, owner_box: *Box, clamp_at_zero: bool) -> V2
{
  return .{LenghtToPx(len2.width, owner_box, clamp_at_zero),
           LengthToPx(len2.height, owner_box, clamp_at_zero)};
}

RenderCommand :: struct
{
  p: V2;
  dim: V2;
  color: Color;

  type: enum {RECTANGLE; BORDER; TEXT;};
  corner_radius: float; // @todo [4] float
  border_width: float;
  // edge_softness: float; add?

  box: *Box;
};

LayoutState :: struct
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

  render_commands: [..] RenderCommand;

  measure_text_callback := MeasureTextStub;
  rem_to_px: float;

  layers: [MAX_NESTING] BoxSpec;
  layers_count: s64;

  parents: [MAX_NESTING] *Box; // @todo make this dynamic? Or no?
  parents_count: s64;
};
layout_state: LayoutState; // @todo allow user to create a layout_state and to attach it to context instead of using a global variable

MeasureTextParams :: struct
{
  text: string;
  font: s32;
  font_size: float;
  flags: enum_flags u8
  {
    MEASURE_X;
    MEASURE_Y;
  };
};
MeasureTextStub :: (params: MeasureTextParams) -> Vector2 { return .{}; }

SetMeasureTextCallback :: (callback: #type (params: MeasureTextParams) -> Vector2)
{
  layout_state.measure_text_callback = callback;
}

LayoutBegin :: (root_dimensions_in_pixels: V2, set_rem_to_px: float)
{
  using layout_state;
  current_boxes_index = xx !current_boxes_index;
  array_reset_keeping_memory(BoxesArray(.CURRENT));

  nil_box = .{};
  LinkToNil(*nil_box);

  array_reset_keeping_memory(*render_commands);

  rem_to_px = set_rem_to_px;

  parents_count = 0;
  layers_count = 0;

  root_box = CreateBox(.{layout = .{sizing = Size2(Px(root_dimensions_in_pixels))}}).box;
  PushParent(root_box);
}

PaddingLower :: (box: *Box, axis: Axis2) -> float #expand #no_debug {
  return box.padding.axes[axis].lower;
}
PaddingUpper :: (box: *Box, axis: Axis2) -> float #expand #no_debug {
  return box.padding.axes[axis].upper;
}
PaddingDim :: (box: *Box, axis: Axis2) -> float #expand #no_debug {
  return box.padding.axes[axis].lower + box.padding.axes[axis].upper;
}
OuterDim :: (box: *Box, axis: Axis2) -> float #expand #no_debug {
  return box.inner_final.component[axis] +
         box.padding.axes[axis].lower +
         box.padding.axes[axis].upper;
}
InnerDim :: (box: *Box, axis: Axis2) -> float #expand #no_debug {
  return box.inner_final.component[axis];
}
InnerDimMin :: (box: *Box, axis: Axis2) -> float #expand #no_debug {
  return box.inner_min.component[axis];
}
InnerDimMax :: (box: *Box, axis: Axis2) -> float #expand #no_debug {
  return box.inner_max.component[axis];
}

OuterDim2 :: (box: *Box) -> V2 #expand #no_debug {
  return V2.{OuterDim(box, .X), OuterDim(box, .Y)};
}
InnerDim2 :: (box: *Box) -> V2 #expand #no_debug {
  return box.inner_final;
}

LayoutEnd :: (debug_print_tree := false)
{
  using layout_state;

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

  for axis: Axis2.X .. Axis2.Y
  {
    // - SIZING PASS 1 -
    // Steps:
    //   1. Calculate standalone sizes (px, rem, text).
    //   2. Calculate values for measure_text, percent_of_parent, grow (+ temp value for fit_children).
    //   3. Calculate padding.
    for :TreeBreadthFirst root_box
    {
      using it.spec;
      using it.spec.layout;
      size := sizing.axes[axis];

      // 1. Calculate standalone sizes (px, rem, text)
      it.inner_min.component[axis] = LengthToPx(size.min, it, false);
      it.inner_max.component[axis] = LengthToPx(size.max, it, false);

      // Default to maximal size (it will be decreased in later stages if overflow happens).
      it.inner_final.component[axis] = max(it.inner_min.component[axis], it.inner_max.component[axis]);
      it.inner_final.component[axis] = max(it.inner_final.component[axis], 0); // Default to 0 if both values were negative.

      // 2. Calculate values for measure_text, percent_of_parent, grow (+ temp value for fit_children).
      new_dim := -1.0;
      parent_dim := it.parent.inner_final.component[axis];
      if size.fit_children == .TRUE
      {
        // This will be overwriten by the next stage;
        // For now assign parent_dim to provide some reasonable behavior for (miss)cofigurations
        // where parent uses `fit_children` and child uses `grow`/`percentage_of_parent`.
        new_dim = parent_dim;
      }
      if size.measure_text == .TRUE
      {
        params := MeasureTextParams.{
          text = text.str,
          font = text.font,
          font_size = text.font_size,
          flags = ifx axis == .X then .MEASURE_X else .MEASURE_Y,
        };
        text_dim := measure_text_callback(params);
        new_dim = text_dim.component[axis];
      }
      if size.percent_of_parent >= 0.0
      {
        new_dim = parent_dim*size.percent_of_parent;
      }
      if new_dim >= 0.0 // Store new_dim
      {
        it.inner_final.component[axis] = new_dim;
      }

      // 3. Calculate padding.
      it.padding.axes[axis].lower = LengthToPx(padding.axes[axis].lower, it, true);
      it.padding.axes[axis].upper = LengthToPx(padding.axes[axis].upper, it, true);
    }

    // - SIZING PASS 2 -
    // Steps:
    //   1. Calculate sizes dependent on children (fit).
    //   2. Clamp inner_final to inner_min and inner_max.
    for :TreeDepthFirst root_box
    {
      using it.spec.layout;
      size := sizing.axes[axis];

      // 1. Calculate size from children (fit).
      if size.fit_children == .TRUE
      {
        children_outer_dim: float;
        if AxisMatchesLayoutDirection(axis, child_layout)
        {
          for :Siblings child: it.first
            children_outer_dim += OuterDim(child, axis);
        }
        else
        {
          for :Siblings child: it.first
            children_outer_dim = max(children_outer_dim, OuterDim(child, axis));
        }
        it.inner_final.component[axis] = children_outer_dim;
      }

      // 2. Clamp inner_final to inner_min and inner_max.
      if it.inner_max.component[axis] >= 0.0
        it.inner_final.component[axis] = min(it.inner_max.component[axis], it.inner_final.component[axis]);

      if it.inner_min.component[axis] >= 0.0
        it.inner_final.component[axis] = max(it.inner_min.component[axis], it.inner_final.component[axis]);
    }

    // - SIZING PASS 3 -
    // Steps:
    //   1. Solve overflow violations.
    //   2. Use free space from parent if grow == .TRUE.
    for :TreeBreadthFirst parent: root_box
    {
      if AxisMatchesLayoutDirection(axis, parent.layout.child_layout)
      {
        children_outer_dim_sum := 0.0;
        children_growable_sum := 0.0;
        children_shrinkable_sum := 0.0;

        for :Siblings child: parent.first
        {
          children_outer_dim_sum += OuterDim(child, axis);
          if child.layout.sizing.axes[axis].grow == .TRUE
          {
            max_child_inner_dim := PositiveMin(InnerDim(parent, axis) - PaddingDim(child, axis), InnerDimMax(child, axis));
            growable := max_child_inner_dim - InnerDim(child, axis);
            children_growable_sum += growable;
          }

          shrinkable := InnerDim(child, axis) - max(0.0, InnerDimMin(child, axis));
          children_shrinkable_sum += shrinkable;
        }

        free_space := InnerDim(parent, axis) - children_outer_dim_sum;

        // Detect overflow. Shrink children.
        if free_space < 0.0 && children_shrinkable_sum > 0.0
        {
          shrink_ratio := children_shrinkable_sum / -free_space;
          shrink_ratio = min(shrink_ratio, 1.0); // If there isn't enough free space then allow boxes to overflow.

          for :Siblings child: parent.first
          {
            shrinkable := InnerDim(child, axis) - max(0.0, InnerDimMin(child, axis));
            to_shrink := shrinkable * shrink_ratio;
            child.inner_final.component[axis] -= to_shrink;
          }
        }

        // Detect free space. Extend dimensions of children with grow == .TRUE
        if free_space > 0.0 && children_growable_sum > 0.0
        {
          grow_ratio := free_space / children_growable_sum;
          grow_ratio = min(grow_ratio, 1.0); // Don't let box grow past its "growable" (based on max_child_inner_dim).

          for :Siblings child: parent.first
          {
            if child.layout.sizing.axes[axis].grow == .TRUE
            {
              max_child_inner_dim := PositiveMin(InnerDim(parent, axis) - PaddingDim(child, axis), InnerDimMax(child, axis));
              growable := max_child_inner_dim - InnerDim(child, axis);
              to_grow := growable * grow_ratio;
              child.inner_final.component[axis] += to_grow;
            }
          }
        }
      }
      else
      {
        for :Siblings child: parent.first
        {
          free_space := InnerDim(parent, axis) - OuterDim(child, axis);

          // shrink child
          if free_space < 0.0
          {
            max_child_inner_dim := InnerDim(parent, axis) - PaddingDim(child, axis);
            child.inner_final.component[axis] = PositiveMax(max_child_inner_dim, InnerDimMin(child, axis));
          }

          // grow child
          if free_space > 0.0 && child.layout.sizing.axes[axis].grow == .TRUE
          {
            max_child_inner_dim := InnerDim(parent, axis) - PaddingDim(child, axis);
            child.inner_final.component[axis] = PositiveMin(max_child_inner_dim, InnerDimMax(child, axis));
          }
        }
      }
    }

    // - POSITION PASS 1 -
    // Compute relative positions.
    for :TreeBreadthFirst parent: root_box
    {
      align_shift_ratio := 0.0;
      if parent.layout.child_align.axes[axis] == {
        case .LEFT;  /*or TOP*/    align_shift_ratio = 0.0;
        case .CENTER;              align_shift_ratio = 0.5;
        case .RIGHT; /*or BOTTOM*/ align_shift_ratio = 1.0;
      };

      if AxisMatchesLayoutDirection(axis, parent.layout.child_layout)
      {
        // Set base relative positions
        {
          offset := PaddingLower(parent, axis);
          for :Siblings child: parent.first
          {
            child.rel_p.component[axis] = offset;
            offset += OuterDim(child, axis);
          }
        }

        // Add alignment if there is free space in parent
        if align_shift_ratio > 0.0
        {
          children_outer_dim_sum := 0.0;
          for :Siblings child: parent.first
          {
            children_outer_dim_sum += OuterDim(child, axis);
          }
          free_space := InnerDim(parent, axis) - children_outer_dim_sum;

          if free_space > 0.0
          {
            shift_by := free_space * align_shift_ratio;
            for :Siblings child: parent.first
              child.rel_p.component[axis] += shift_by;
          }
        }
      }
      else
      {
        // Clear relative positions
        for :Siblings child: parent.first
          child.rel_p.component[axis] = PaddingLower(parent, axis);

        // Add alignment if there is free space in parent
        if align_shift_ratio > 0.0
        {
          for :Siblings child: parent.first
          {
            free_space := InnerDim(parent, axis) - OuterDim(child, axis);
            if free_space > 0.0
            {
              shift_by := free_space * align_shift_ratio;
              child.rel_p.component[axis] += shift_by;
            }
          }
        }
      }
    }
  }

  // - POSITION PASS 2 -
  // Compute absolute positions.
  for :TreeBreadthFirst root_box
  {
    it.abs_p = it.parent.abs_p + it.rel_p;
  }

  // - DEBUG PRINT PASS -
  if debug_print_tree
  {
    print("[TreeBreadthFirst] Debug dump of root_box\n");
    for :TreeBreadthFirst root_box
    {
      inner_dim := it.inner_final;
      outer_dim := V2.{OuterDim(it, .X), OuterDim(it, .Y)};

      for 0..it_depth-1 print("  ");
      print("[d %; i %]; ", it_depth, it_index);
      print("inner% + padding% = outer%;\t", inner_dim, it.padding, outer_dim);
      print("rel_p %; abs_p %;\t", it.rel_p, it.abs_p);
      print("%\n", it.loc);
    }
  }

  // - RENDER COMMANDS PASS -
  for :TreeBreadthFirst root_box.first
  {
    base_cmd: RenderCommand;
    base_cmd.p = it.abs_p;
    base_cmd.dim = V2.{OuterDim(it, .X), OuterDim(it, .Y)};
    base_cmd.box = it;

    if it.bg_color
    {
      cmd := base_cmd;
      cmd.color = it.bg_color;
      cmd.type = .RECTANGLE;
      array_add(*render_commands, cmd);
    }

    if it.border.color
    {
      // cmd := base_cmd;
      // cmd.color = it.border.color;
      // cmd.type = .BORDER;
      // cmd.border_width = it.border.widths.left;
      // array_add(*render_commands, cmd);
    }

    if it.text.color
    {
      cmd := base_cmd;
      cmd.color = it.text.color;
      cmd.type = .TEXT;
      array_add(*render_commands, cmd);
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
#import "Math"; // Vector2, operator +

// Math helpers
min :: (a: V2, b: V2) -> V2 { return V2.{min(a.x,b.x), min(a.y,b.y)}; }
max :: (a: V2, b: V2) -> V2 { return V2.{max(a.x,b.x), max(a.y,b.y)}; }

PositiveMin :: (a: float, b: float) -> float
{
  result := max(a,b);
  candidate := min(a,b);
  if candidate >= 0.0 then result = min(result, candidate);
  return result;
}
PositiveMax :: (a: float, b: float) -> float
{
  result := max(a,b);
  candidate := min(a,b);
  if candidate >= 0.0 then result = max(result, candidate);
  return result;
}
PositiveMin :: (a: V2, b: V2) -> V2 { return .{PositiveMin(a.x, b.x), PositiveMin(a.y, b.y)}; }
PositiveMax :: (a: V2, b: V2) -> V2 { return .{PositiveMax(a.x, b.x), PositiveMax(a.y, b.y)}; }
