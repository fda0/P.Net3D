Color32 :: u32;

Color32_RGBAi :: (r: u32, g: u32, b: u32, a: u32) -> Color32
{
  res := ((a << 24) |
    (b << 16) |
    (g << 8) |
    r);
  return res;
}

Color32_RGBi :: (r: u32, g: u32, b: u32) -> Color32 { return Color32_RGBAi(r, g, b, 255); }
Color32_Grayi :: (rgb: u32) -> Color32 { return Color32_RGBi(rgb, rgb, rgb); }

Color32_RGBAf :: (r: float, g: float, b: float, a: float) -> Color32
{
  ri := (r * 255).(u32) & 255;
  gi := (g * 255).(u32) & 255;
  bi := (b * 255).(u32) & 255;
  ai := (a * 255).(u32) & 255;
  return Color32_RGBAi(ri, gi, bi, ai);
}

Color32_RGBf :: (r: float, g: float, b: float) -> Color32
{
  ri := (r * 255).(u32) & 255;
  gi := (g * 255).(u32) & 255;
  bi := (b * 255).(u32) & 255;
  return Color32_RGBAi(ri, gi, bi, 255);
}

Color32_Grayf :: (rgb: float) -> Color32 { return Color32_RGBf(rgb, rgb, rgb); }
Color32_V3 :: (color: V3) -> Color32 { return Color32_RGBf(color.x, color.y, color.z); }
Color32_V4 :: (color: V4) -> Color32 { return Color32_RGBAf(color.x, color.y, color.z, color.w); }

Color32_Lerp :: (c0: Color32, c1: Color32, t: float) -> Color32
{
  // @todo Do a lerp in a different color space - like Oklab or something
  // https://raphlinus.github.io/color/2021/01/18/oklab-critique.html
  vec0 := ColorV4_32(c0);
  vec1 := ColorV4_32(c1);
  lerped := lerp(vec0, vec1, t);
  res := Color32_V4(lerped);
  return res;
}


ColorV4 :: V4;

ColorV4_32 :: (packed: Color32) -> ColorV4
{
  inv: float = 1.0 / 255.0;
  res: ColorV4 = ---;
  res.x = (packed & 255) * inv;
  res.y = ((packed >> 8) & 255) * inv;
  res.z = ((packed >> 16) & 255) * inv;
  res.w = ((packed >> 24) & 255) * inv;
  return res;
}
