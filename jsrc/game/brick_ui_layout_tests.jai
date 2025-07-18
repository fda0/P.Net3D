DEBUG_LOGGING :: false;
PRINT_ON_PASS :: false;

DefaultLayoutSpec :: LayoutSpec.{
  root_dim_px = V2.{2000, 1500},
  rem_to_px = 16,
  debug_logging = DEBUG_LOGGING,
};

tests :: ()
{
  tests_grow();
  tests_fit_and_grow();
  tests_child_gap();
}

tests_fit_and_grow :: ()
{
  // TEST: Fit on parent & grow on children.
  // Same as below but uses direction = .DEFAULT (.LEFT_TO_RIGHT)
  {
    LayoutBegin(DefaultLayoutSpec);
    parent := CreateBox(.{layout.sizing = .{Fit(), Fit(SizeMin(Px(100)))},
                          layout.padding = PaddingAll(Px(3))});
    Parent(parent);
    child0 := CreateBox(.{layout.sizing = .{Size(Px(10)), Size(Px(15))},
                          layout.padding = PaddingAll(Px(10))});
    child1 := CreateBox(.{layout.sizing = .{Grow(), Grow()},
                          layout.padding = .{Px(11), Px(12), Px(13), Px(14)}});
    LayoutEnd();

    c0_padding := V2.{10+10, 10+10};
    c0_inner := V2.{10, 15};
    c0_outer := c0_inner + c0_padding;
    CheckEQ(InnerDim2(child0), c0_inner);
    CheckEQ(OuterDim2(child0), c0_outer);

    c1_padding := V2.{11+12, 13+14};
    c1_inner := V2.{0, 100.0 - c1_padding.y};
    c1_outer := c1_inner + c1_padding;
    CheckEQ(InnerDim2(child1), c1_inner);
    CheckEQ(OuterDim2(child1), c1_outer);

    parent_inner := V2.{c0_outer.x + c1_outer.x, max(c0_outer.y, c1_outer.y)};
    CheckEQ(parent.inner_final, parent_inner);
  }

  // TEST: Fit on parent & grow on children.
  // Same as above but uses direction = .TOP_TO_BOTTOM
  {
    LayoutBegin(DefaultLayoutSpec);
    parent := CreateBox(.{layout.sizing = .{Fit(), Fit(SizeMin(Px(100)))},
                          layout.padding = PaddingAll(Px(3)),
                          layout.direction = .TOP_TO_BOTTOM});
    Parent(parent);
    child0 := CreateBox(.{layout.sizing = .{Size(Px(10)), Size(Px(15))},
                          layout.padding = PaddingAll(Px(10))});
    child1 := CreateBox(.{layout.sizing = .{Grow(), Grow()},
                          layout.padding = .{Px(11), Px(12), Px(13), Px(14)}});
    LayoutEnd();

    c0_padding := V2.{10+10, 10+10};
    c0_inner := V2.{10, 15};
    c0_outer := c0_inner + c0_padding;
    CheckEQ(InnerDim2(child0), c0_inner);
    CheckEQ(OuterDim2(child0), c0_outer);

    c1_padding := V2.{11+12, 13+14};
    c1_inner := V2.{c0_outer.x - c1_padding.x, 100.0 - c0_outer.y - c1_padding.y};
    c1_outer := c1_inner + c1_padding;
    CheckEQ(InnerDim2(child1), c1_inner);
    CheckEQ(OuterDim2(child1), c1_outer);

    parent_inner := V2.{max(c0_outer.x, c1_outer.x), c0_outer.y + c1_outer.y};
    CheckEQ(parent.inner_final, parent_inner);
  }
}

tests_grow :: ()
{
  // 3x just grow.
  {
    LayoutBegin(DefaultLayoutSpec);
    parent := CreateBox(.{layout.sizing = .{Size(Px(900)), Size(Px(600))}});
    Parent(parent);
    child0 := CreateBox(.{layout.sizing = .{Grow(), Grow()}});
    child1 := CreateBox(.{layout.sizing = .{Grow(), Grow()}});
    child2 := CreateBox(.{layout.sizing = .{Grow(), Grow()}});
    LayoutEnd();

    c_inner := V2.{300, 600};
    CheckEQ(OuterDim2(child0), c_inner);
    CheckEQ(OuterDim2(child1), c_inner);
    CheckEQ(OuterDim2(child2), c_inner);
    CheckEQ(parent.inner_final, V2.{900, 600});
  }

  // 3x grow with 1 max size contrain - should be indentical to above.
  {
    LayoutBegin(DefaultLayoutSpec);
    parent := CreateBox(.{layout.sizing = .{Size(Px(900)), Size(Px(600))}});
    Parent(parent);
    child0 := CreateBox(.{layout.sizing = .{Grow(), Grow()}});
    child1 := CreateBox(.{layout.sizing = .{Grow(SizeMax(Px(300))), Grow()}});
    child2 := CreateBox(.{layout.sizing = .{Grow(), Grow()}});
    LayoutEnd();

    c_inner := V2.{300, 600};
    CheckEQ(OuterDim2(child0), c_inner);
    CheckEQ(OuterDim2(child1), c_inner);
    CheckEQ(OuterDim2(child2), c_inner);
    CheckEQ(parent.inner_final, V2.{900, 600});
  }

  // 3x grow with 1 max size contrain.
  // Middle box (child1) will be initialized with its max size; it's siblings will get what's left.
  {
    LayoutBegin(DefaultLayoutSpec);
    parent := CreateBox(.{layout.sizing = .{Size(Px(900)), Size(Px(600))}});
    Parent(parent);
    child0 := CreateBox(.{layout.sizing = .{Grow(), Grow()}});
    child1 := CreateBox(.{layout.sizing = .{Grow(SizeMax(Px(600))), Grow()}});
    child2 := CreateBox(.{layout.sizing = .{Grow(), Grow()}});
    LayoutEnd();

    c1_inner := V2.{600, 600};
    c02_inner := V2.{150, 600};
    CheckEQ(OuterDim2(child0), c02_inner);
    CheckEQ(OuterDim2(child1), c1_inner);
    CheckEQ(OuterDim2(child2), c02_inner);
    CheckEQ(parent.inner_final, V2.{900, 600});
  }

  // 3x grow with 1 min size contrain.
  // Min size of Px(300) was picked because it makes calculations a bit easier to follow.
  // Middle box (child1) starts with its min size of 300px before grow calculations begin.
  //
  // free_space -> (900 - 300) -> 600
  // children_growable_sum (sum of how much each child could grow in isolation) -> 900 + 600 + 900 -> 2400
  // free_space / children_growable_sum -> 0.25
  //
  // child0_to_grow -> 0.25 * 900 -> 225
  // child1_to_grow -> 0.25 * 600 -> 150
  // child2_to_grow -> 0.25 * 900 -> 225
  //
  // child0_new_dim ->   0 + 225 -> 225
  // child1_new_dim -> 300 + 150 -> 450
  // child2_new_dim ->   0 + 225 -> 225
  {
    LayoutBegin(DefaultLayoutSpec);
    parent := CreateBox(.{layout.sizing = .{Size(Px(900)), Size(Px(600))}});
    Parent(parent);
    child0 := CreateBox(.{layout.sizing = .{Grow(), Grow()}});
    child1 := CreateBox(.{layout.sizing = .{Grow(SizeMin(Px(300))), Grow()}});
    child2 := CreateBox(.{layout.sizing = .{Grow(), Grow()}});
    LayoutEnd();

    c1_inner := V2.{450, 600};
    c02_inner := V2.{225, 600};
    CheckEQ(OuterDim2(child0), c02_inner);
    CheckEQ(OuterDim2(child1), c1_inner);
    CheckEQ(OuterDim2(child2), c02_inner);
    CheckEQ(parent.inner_final, V2.{900, 600});
  }
}

tests_child_gap :: ()
{
  // Test if child gap is applied between 3 children.
  {
    LayoutBegin(DefaultLayoutSpec);
    parent := CreateBox(.{layout.sizing = .{Size(Px(900)), Size(Px(600))},
                          layout.child_gap = Px(40)});
    Parent(parent);
    child0 := CreateBox(.{layout.sizing = .{Size(Px(100)), Grow()}});
    child1 := CreateBox(.{layout.sizing = .{Size(Px(200)), Grow()},
                          layout.padding = PaddingAll(Px(15))});
    child2 := CreateBox(.{layout.sizing = .{Grow(), Grow()}});
    LayoutEnd();

    gap := 40.0;
    pad1 := 15.0;
    CheckEQ(InnerDim2(child0), V2.{100, 600});
    CheckEQ(InnerDim2(child1), V2.{200, 600 - pad1*2});
    CheckEQ(InnerDim2(child2), V2.{900 - (100+200 + pad1*2 + gap*2), 600});
    CheckEQ(parent.inner_final, V2.{900, 600});

    CheckEQ(child0.abs_p, V2.{0, 0});
    CheckEQ(child1.abs_p, V2.{100 + gap, 0});
    CheckEQ(child2.abs_p, V2.{100+200 + pad1*2 + gap*2, 0});
  }
}

//
// Tests setup
//
#import,file "brick_ui_layout.jai";
#import "Basic";
#import "Math";
V2 :: Vector2;
CheckCount := 0;
CheckErrorCount := 0;

main :: ()
{
  tests();
  if CheckErrorCount print("% out of % checks failed.\n", CheckErrorCount, CheckCount);
  else               print("All % checks passed.\n", CheckCount);
  exit(xx CheckErrorCount);
}

CheckEQ :: (a: $T, b: T, loc := #caller_location)
{
  did_pass := a == b;
  Check(did_pass, "Condtion: % % %", a, ifx did_pass then "==" else "!=", b, loc=loc);
}

Check :: (did_pass: bool, message := "", args: ..Any, loc := #caller_location)
{
  CheckCount += 1;

  passfail := ifx did_pass then "passed" else "FAILED";
  if !did_pass then CheckErrorCount += 1;

  if !did_pass || PRINT_ON_PASS
  {
    builder := String_Builder.{allocator=temp};
    print_to_builder(*builder, "Test check % % at: %; ", CheckCount, passfail, loc);
    print_to_builder(*builder, message, ..args);
    print_to_builder(*builder, "\n");
    final_message := builder_to_string(*builder);
    print(final_message);
  }
}
