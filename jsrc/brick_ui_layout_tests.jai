PRINT_ON_PASS :: false;
DUMP_TREES :: true;

#import,file "brick_ui_layout.jai";
#import "Basic";
#import "Math";
V2 :: Vector2;

CheckCount := 0;
CheckErrorCount := 0;

main :: ()
{
  window_dim := V2.{640, 480};
  rem :: 16;

  // TEST: Fit root & Grow child; child_layout = .LEFT_TO_RIGHT
  {
    LayoutBegin(window_dim, rem);
    parent := CreateBox(.{layout.sizing = .{Fit(), Fit(SizeMin(Px(100)))},
                          layout.padding = Padding_All(Px(3))}).box;
    Parent(parent);
    child0 := CreateBox(.{layout.sizing = .{Size(Px(10)), Size(Px(15))},
                          layout.padding = Padding_All(Px(10))});
    child1 := CreateBox(.{layout.sizing = .{Grow(), Grow()},
                          layout.padding = .{Px(11), Px(12), Px(13), Px(14)}});
    LayoutEnd(DUMP_TREES);

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

  // TEST: Fit root & Grow child; child_layout = .TOP_TO_BOTTOM
  {
    LayoutBegin(window_dim, rem);
    parent := CreateBox(.{layout.sizing = .{Fit(), Fit(SizeMin(Px(100)))},
                          layout.padding = Padding_All(Px(3)),
                          layout.child_layout = .TOP_TO_BOTTOM}).box;
    Parent(parent);
    child0 := CreateBox(.{layout.sizing = .{Size(Px(10)), Size(Px(15))},
                          layout.padding = Padding_All(Px(10))});
    child1 := CreateBox(.{layout.sizing = .{Grow(), Grow()},
                          layout.padding = .{Px(11), Px(12), Px(13), Px(14)}});
    LayoutEnd(DUMP_TREES);

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

  if CheckErrorCount print("% out of % checks failed.\n", CheckErrorCount, CheckCount);
  else               print("All % checks passed.\n", CheckCount);
  exit(xx CheckErrorCount);
}

CheckEQ :: (a: $T, b: T, loc := #caller_location) #no_debug #expand
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
