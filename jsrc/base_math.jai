Axis3 :: enum
{
  X;
  Y;
  Z;
};

AxisToVector :: (axis: Axis3) -> V3
{
  if axis == {
    case; #through;
    case .X; return V3.{1,0,0};
    case .Y; return V3.{0,1,0};
    case .Z; return V3.{0,0,1};
  }
}

RotationAroundAxis :: (axis_: V3, turns: float) -> Quat // Assumes Right Handed coordinate system
{
  sin, cos = SinCos(turns * 0.5);
  axis := normalize(axis_);
  axis *= sin;
  res := Quat.{
    axis.x,
    axis.y,
    axis.z,
    cos
  };
  return res;
}
RotationAroundAxis :: (axis: Axis3, turns: float) -> Quat
{
  return RotationAroundAxis(AxisToVector(axis), turns);
}


// This file wraps some math operations to chage its units from radians to turns.
// For now there is a bunch of needless converting which will be eliminated
// once I create my own math implementations (by forking modules/Math).
using,except(sin,cos) Math :: #import "Math";

TURNS_TO_RAD :: TAU;
RAD_TO_TURNS :: 1 / TAU;

SinCos :: (turns: float) -> sin:float, cos:float
{
  rad := TURNS_TO_RAD * turns;
  // @speed add optimized function that calculates both sin & cos at once at reduced total cost
  s := Math.sin(rad);
  c := Math.cos(rad);
  return s, c;
}

Sin :: (turns: float) -> float
{
  rad := TURNS_TO_RAD * turns;
  return Math.sin(turns);
}

Cos :: (turns: float) -> float
{
  rad := TURNS_TO_RAD * turns;
  return Math.cos(turns);
}
