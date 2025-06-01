V2 :: Vector2;
V3 :: Vector3;
V4 :: Vector4;
Quat :: Quaternion;

Mat4 :: struct // @todo move to row-major matrices in the future
{
  // column-major Matrix 4x4
  // 0 4 8 c
  // 1 5 9 d
  // 2 6 a e
  // 3 7 b f

  // _yx
  _11, _21, _31, _41 : float;
  _12, _22, _32, _42 : float;
  _13, _23, _33, _43 : float;
  _14, _24, _34, _44 : float;

  #place _11; flat: [16]   float = ---;
  #place _11; elem: [4][4] float = ---;
  #place _11; cols: [4]    V4    = ---;
};

// AXIS MATH
Axis3 :: enum
{
  X;
  Y;
  Z;
};
AxisV3 :: (axis: Axis3) -> V3
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

Mix :: (a: Quat, b: Quat, weight_a: float, weight_b: float) -> Quat
{
  res := Quat.{
    a.x*weight_a + b.x*weight_b,
    a.y*weight_a + b.y*weight_b,
    a.z*weight_a + b.z*weight_b,
    a.w*weight_a + b.w*weight_b,
  };
  return res;
}

// MATRIX MATH
ScaleMatrix :: (scale: V3) -> Mat4
{
  res: Mat4;
  res.elem[0][0] = scale.x;
  res.elem[1][1] = scale.y;
  res.elem[2][2] = scale.z;
  res.elem[3][3] = 1.0;
  return res;
}

ScaleMatrix :: (scale: float) -> Mat4
{
  res: Mat4;
  res.elem[0][0] = scale;
  res.elem[1][1] = scale;
  res.elem[2][2] = scale;
  res.elem[3][3] = 1.0;
  return res;
}

RotationMatrix :: (q: Quat) -> Mat4
{
  norm := normalize(q);
  xx_ := norm.x * norm.x;
  yy := norm.y * norm.y;
  zz := norm.z * norm.z;
  xy := norm.x * norm.y;
  xz := norm.x * norm.z;
  yz := norm.y * norm.z;
  wx := norm.w * norm.x;
  wy := norm.w * norm.y;
  wz := norm.w * norm.z;

  res: Mat4 = ---;
  res.elem[0][0] = 1.0 - 2.0 * (yy + zz);
  res.elem[0][1] = 2.0 * (xy + wz);
  res.elem[0][2] = 2.0 * (xz - wy);
  res.elem[0][3] = 0.0;

  res.elem[1][0] = 2.0 * (xy - wz);
  res.elem[1][1] = 1.0 - 2.0 * (xx_ + zz);
  res.elem[1][2] = 2.0 * (yz + wx);
  res.elem[1][3] = 0.0;

  res.elem[2][0] = 2.0 * (xz + wy);
  res.elem[2][1] = 2.0 * (yz - wx);
  res.elem[2][2] = 1.0 - 2.0 * (xx_ + yy);
  res.elem[2][3] = 0.0;

  res.elem[3][0] = 0.0;
  res.elem[3][1] = 0.0;
  res.elem[3][2] = 0.0;
  res.elem[3][3] = 1.0;
  return res;
}

DiagonalMatrix :: (value: float = 1) -> Mat4
{
  res: Mat4;
  res.elem[0][0] = value;
  res.elem[1][1] = value;
  res.elem[2][2] = value;
  res.elem[3][3] = value;
  return res;
}

TranslationMatrix :: (move: V3) -> Mat4
{
  res := DiagonalMatrix();
  res.elem[3][0] = move.x;
  res.elem[3][1] = move.y;
  res.elem[3][2] = move.z;
  return res;
}

RotationMatrixAroundAxis :: (axis_: V3, turns: float) -> Mat4
{
  axis := normalize(axis_);
  ts, tc := SinCos(turns);
  c1 := 1.0 - tc;

  res := DiagonalMatrix();
  res.elem[0][0] = (axis.x * axis.x * c1) + tc;
  res.elem[0][1] = (axis.x * axis.y * c1) + (axis.z * ts);
  res.elem[0][2] = (axis.x * axis.z * c1) - (axis.y * ts);

  res.elem[1][0] = (axis.y * axis.x * c1) - (axis.z * ts);
  res.elem[1][1] = (axis.y * axis.y * c1) + tc;
  res.elem[1][2] = (axis.y * axis.z * c1) + (axis.x * ts);

  res.elem[2][0] = (axis.z * axis.x * c1) + (axis.y * ts);
  res.elem[2][1] = (axis.z * axis.y * c1) - (axis.x * ts);
  res.elem[2][2] = (axis.z * axis.z * c1) + tc;
  return res;
}

PerspectiveMatrix :: (fov_y: float, aspect_ratio: float, near: float, far: float) -> Mat4
{
  // Modified to work with +x fordward, -y right, +z up coordinate system (same as Source engine).
  cotangent := 1.0 / Tan(fov_y * 0.5);
  res: Mat4;
  res.elem[0][2] = -far / (near - far); // X -> -Z
  res.elem[0][3] = 1.0; // X -> W
  res.elem[1][0] = -cotangent / aspect_ratio; // -Y -> X
  res.elem[2][1] = cotangent; // Z -> Y
  res.elem[3][2] = (near * far) / (near - far); // W -> Z
  return res;
}

OrthographicMatrix :: (left: float, right: float, bottom: float, top: float, near: float, far: float) -> Mat4
{
  res: Mat4;
  res.elem[0][2] = -1.0 / (near - far); // X -> -Z
  res.elem[1][0] = -2.0 / (right - left); // -Y -> X
  res.elem[2][1] = 2.0 / (top - bottom); // Z -> Y
  res.elem[3][3] = 1.0; // W -> W

  res.elem[3][0] = (left + right) / (left - right); // W -> X
  res.elem[3][1] = (bottom + top) / (bottom - top); // W -> Y
  res.elem[3][2] = near / (near - far); // W -> Z
  return res;
}

multiply :: (m: Mat4, n: Mat4) -> Mat4
{
  result: Mat4 = ---;

  result._11 = m._11*n._11 + m._12*n._21 + m._13*n._31 + m._14*n._41;
  result._21 = m._21*n._11 + m._22*n._21 + m._23*n._31 + m._24*n._41;
  result._31 = m._31*n._11 + m._32*n._21 + m._33*n._31 + m._34*n._41;
  result._41 = m._41*n._11 + m._42*n._21 + m._43*n._31 + m._44*n._41;

  result._12 = m._11*n._12 + m._12*n._22 + m._13*n._32 + m._14*n._42;
  result._22 = m._21*n._12 + m._22*n._22 + m._23*n._32 + m._24*n._42;
  result._32 = m._31*n._12 + m._32*n._22 + m._33*n._32 + m._34*n._42;
  result._42 = m._41*n._12 + m._42*n._22 + m._43*n._32 + m._44*n._42;

  result._13 = m._11*n._13 + m._12*n._23 + m._13*n._33 + m._14*n._43;
  result._23 = m._21*n._13 + m._22*n._23 + m._23*n._33 + m._24*n._43;
  result._33 = m._31*n._13 + m._32*n._23 + m._33*n._33 + m._34*n._43;
  result._43 = m._41*n._13 + m._42*n._23 + m._43*n._33 + m._44*n._43;

  result._14 = m._11*n._14 + m._12*n._24 + m._13*n._34 + m._14*n._44;
  result._24 = m._21*n._14 + m._22*n._24 + m._23*n._34 + m._24*n._44;
  result._34 = m._31*n._14 + m._32*n._24 + m._33*n._34 + m._34*n._44;
  result._44 = m._41*n._14 + m._42*n._24 + m._43*n._34 + m._44*n._44;

  return result;
}

operator * :: (a: Mat4, b: Mat4) -> Mat4
{
  return inline multiply(a, b);
}

transpose :: (m: Mat4) -> Mat4
{
    r: Mat4 = ---;
    for i: 0..3 for j: 0..3 {
        r.elem[i][j] = m.elem[j][i];
    }
    return r;
}



// VEC2 - generic impl (allows for T != float)
Vec2 :: struct($T: Type)
{
  x, y: T;
};


// We wrap some math operations here to change its units from radians to turns.
// For now there is a bunch of needless unit conversions which will be eliminated
// once I create my own math implementations (by forking modules/Math).
using,except(sin,cos,rotate,translate,rotation_matrix) Math :: #import "Math";

TURNS_TO_RAD :: TAU;
RAD_TO_TURNS :: 1.0 / TAU;

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
  return Math.sin(rad);
}

Cos :: (turns: float) -> float
{
  rad := TURNS_TO_RAD * turns;
  return Math.cos(rad);
}

//
Rotate :: (v: V3, q: Quat) -> V3 { return Math.rotate(v, q); }
