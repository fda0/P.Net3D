#import,file "brick_ui_layout.jai";
#import "Basic";
#import "Math";
V2 :: Vector2;

main :: ()
{
  window_dim := V2.{640, 480};
  rem :: 16;

  // Fit test
  {
    LayoutBegin(window_dim, rem);
    parent := CreateBox(.{layout.sizing = .{Fit(), Fit(SizeMin(Px(100)))},
                          layout.padding = Padding_All(Px(3))}).box;
    Parent(parent);
    child0 := CreateBox(.{layout.sizing = .{Size(Px(10)), Size(Px(15))},
                          layout.padding = Padding_All(Px(10))});
    child1 := CreateBox(.{layout.sizing = .{Size(Px(20)), Size(Px(25))},
                          layout.padding = .{Px(11), Px(12), Px(13), Px(14)}});
    LayoutEnd(true);

    Check(parent.rel_p == V2.{});
    // Check(parent.rel_p == V2.{});
  }

  exit(xx CheckErrorCount);
}

CheckErrorCount := 0;
CheckFail :: (arg: bool, message := "", args: ..Any, loc := #caller_location)
{
  CheckErrorCount += 1;

  builder := String_Builder.{allocator=temp};
  print_to_builder(*builder, "Test check failed (%) at: %\n", CheckErrorCount, loc);
  print_to_builder(*builder, message, ..args);
  final_message := builder_to_string(*builder);

  print(final_message);
}

Check :: (arg: bool, message := "", args: ..Any, loc := #caller_location) #no_debug #expand
{
  if !arg then CheckFail(arg, message, ..args, loc);
}
