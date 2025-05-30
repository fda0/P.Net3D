V2 :: Vector2;
V3 :: Vector3;
V4 :: Vector4;
Quat :: Quaternion;
Mat4 :: Matrix4;

// AXIS MATH
Axis3 :: enum
{
  X;
  Y;
  Z;
};
ToVec :: (axis: Axis3) -> V3
{
  if axis == {
    case; #through;
    case .X; return V3.{1,0,0};
    case .Y; return V3.{0,1,0};
    case .Z; return V3.{0,0,1};
  }
}

// INT MATH
CeilPow2 :: (v: u32) -> u32
{
  if (!v) return 0;
  v -= 1;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v += 1;
  return v;
}

// FLOAT MATH
WrapFloat :: (min: float, max: float, value: float) -> float
{
  range := max - min;
  offset := value - min;
  return (offset - (floor(offset / range) * range) + min);
}

// VECTOR2 MATH
Rotate :: (v: V2, turns: float) -> V2
{
  sin, cos := SinCos(turns);
  res := Vector2.{
    v.x * cos + v.y * -sin,
    v.x * sin + v.y * cos,
  };
  return res;
}

Tan :: (turns: float) -> float
{
  r64 := tan(turns * TURNS_TO_RAD);
  r32 := cast(float32)r64;
  return r32;
}

NormalFromSegment :: (a: V2, b: V2) -> V2
{
  direction := normalize(b - a);
  res := Rotate(direction, -0.25);
  return res;
}

// QUATERNION MATH
RotationAroundAxis :: (axis_: V3, turns: float) -> Quat // Assumes Right Handed coordinate system
{
  sin, cos := SinCos(turns * 0.5);
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

RotationFromNormalizedPair :: (a: V3, b: V3) -> Quat
{
  cr := cross(a, b);
  res := Quat.{cr.x, cr.y, cr.z, 1.0 + dot(a, b)};
  return res;
}

RotationFromPair :: (a: V3, b: V3) -> Quat
{
  return RotationFromNormalizedPair(normalize(a), normalize(b));
}

// MATRIX MATH
RotationMatrix :: (q: Quat) -> Mat4 { return Math.rotation_matrix(Mat4, q); }

DiagonalMatrix :: (value: float = 1) -> Mat4
{
  res: Mat4;
  res.coef[0][0] = value;
  res.coef[1][1] = value;
  res.coef[2][2] = value;
  res.coef[3][3] = value;
  return res;
}

TranslationMatrix :: (move: V3) -> Mat4
{
  res := DiagonalMatrix();
  res.coef[3][0] = move.x;
  res.coef[3][1] = move.y;
  res.coef[3][2] = move.z;
  return res;
}

RotationMatrixAroundAxis :: (axis_: V3, turns: float) -> Mat4
{
  axis := normalize(axis_);
  ts, tc := SinCos(turns);
  c1 := 1.0 - tc;

  res := DiagonalMatrix();
  res.coef[0][0] = (axis.x * axis.x * c1) + tc;
  res.coef[0][1] = (axis.x * axis.y * c1) + (axis.z * ts);
  res.coef[0][2] = (axis.x * axis.z * c1) - (axis.y * ts);

  res.coef[1][0] = (axis.y * axis.x * c1) - (axis.z * ts);
  res.coef[1][1] = (axis.y * axis.y * c1) + tc;
  res.coef[1][2] = (axis.y * axis.z * c1) + (axis.x * ts);

  res.coef[2][0] = (axis.z * axis.x * c1) + (axis.y * ts);
  res.coef[2][1] = (axis.z * axis.y * c1) - (axis.x * ts);
  res.coef[2][2] = (axis.z * axis.z * c1) + tc;
  return res;
}

PerspectiveMatrix :: (fov_y: float, aspect_ratio: float, near: float, far: float) -> Mat4
{
  // Modified to work with +x fordward, -y right, +z up coordinate system (same as Source engine).
  cotangent := 1.0 / Tan(fov_y * 0.5);
  res: Mat4;
  res.coef[0][2] = -far / (near - far); // X -> -Z
  res.coef[0][3] = 1.0; // X -> W
  res.coef[1][0] = -cotangent / aspect_ratio; // -Y -> X
  res.coef[2][1] = cotangent; // Z -> Y
  res.coef[3][2] = (near * far) / (near - far); // W -> Z
  return res;
}

OrthographicMatrix :: (left: float, right: float, bottom: float, top: float, near: float, far: float) -> Mat4
{
  res: Mat4;
  res.coef[0][2] = -1.0 / (near - far); // X -> -Z
  res.coef[1][0] = -2.0 / (right - left); // -Y -> X
  res.coef[2][1] = 2.0 / (top - bottom); // Z -> Y
  res.coef[3][3] = 1.0; // W -> W

  res.coef[3][0] = (left + right) / (left - right); // W -> X
  res.coef[3][1] = (bottom + top) / (bottom - top); // W -> Y
  res.coef[3][2] = near / (near - far); // W -> Z
  return res;
}



// We wrap some math operations here to change its units from radians to turns.
// For now there is a bunch of needless unit conversions which will be eliminated
// once I create my own math implementations (by forking modules/Math).
using,except(sin,cos,rotate,translate,rotation_matrix) Math :: #import "Math";

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

//
Rotate :: (v: V3, q: Quat) -> V3 { return Math.rotate(v, q); }
