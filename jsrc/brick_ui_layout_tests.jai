#import,file "brick_ui_layout.jai";
#import "Math";

main :: ()
{
  window_dim := Vector2.{640, 480};
  {
    LayoutBegin(window_dim, 16);
    {
      Parent(CreateBox(.{layout.sizing = .{Fit(), Fit()},
                         layout.padding = Padding_All(Px(1))}));

      CreateBox(.{layout.sizing = .{Size(Px(40)), Size(Px(10))},
                  layout.padding = Padding_All(Px(2))});
      CreateBox(.{layout.sizing = .{Size(Px(30)), Size(Px(5))},
                  layout.padding = .{Px(3), Px(4), Px(5), Px(6)}});
    }
    LayoutEnd(true);
  }

  // layout_state.root_box;
}
