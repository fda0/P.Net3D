static U32 UI_ActiveClipIndex()
{
  AssertBounds(APP.gpu.ui.clip_stack_index, APP.gpu.ui.clip_stack);
  U32 clip_index = APP.gpu.ui.clip_stack[APP.gpu.ui.clip_stack_index];
  AssertBounds(clip_index, APP.gpu.ui.clips);
  return clip_index;
}

static UI_Clip UI_ActiveClip()
{
  return APP.gpu.ui.clips[UI_ActiveClipIndex()];
}

static void UI_PushClip(UI_Clip clip)
{
  if (APP.gpu.ui.clip_stack_index + 1 >= ArrayCount(APP.gpu.ui.clip_stack))
    return;
  if (APP.gpu.ui.clips_count >= ArrayCount(APP.gpu.ui.clips))
    return;

  UI_Clip current = UI_ActiveClip();
  // intersection of clip & current
  if (current.p_min.x > clip.p_min.x) clip.p_min.x = current.p_min.x;
  if (current.p_min.y > clip.p_min.y) clip.p_min.y = current.p_min.y;
  if (current.p_max.x < clip.p_max.x) clip.p_max.x = current.p_max.x;
  if (current.p_max.y < clip.p_max.y) clip.p_max.y = current.p_max.y;

  U32 clip_index = APP.gpu.ui.clips_count;
  APP.gpu.ui.clips_count += 1;
  AssertBounds(clip_index, APP.gpu.ui.clips);
  APP.gpu.ui.clips[clip_index] = clip;

  APP.gpu.ui.clip_stack_index += 1;
  APP.gpu.ui.clip_stack[APP.gpu.ui.clip_stack_index] = clip_index;
}

static void UI_PopClip()
{
  if (APP.gpu.ui.clip_stack_index == 0)
    return;
  APP.gpu.ui.clip_stack_index -= 1;
}

static void UI_DrawRaw(UI_Shape shape)
{
  if (APP.gpu.ui.indices_count + 6 > ArrayCount(APP.gpu.ui.indices))
    return;
  if (APP.gpu.ui.shapes_count + 6 > ArrayCount(APP.gpu.ui.shapes))
    return;

  U32 clip_i = UI_ActiveClipIndex();

  U32 shape_i = APP.gpu.ui.shapes_count;
  APP.gpu.ui.shapes_count += 1;
  APP.gpu.ui.shapes[shape_i] = shape;

  U32 index_i = APP.gpu.ui.indices_count;
  APP.gpu.ui.indices_count += 6;
  U32 encoded = ((shape_i << 2) | (clip_i << 18));
  APP.gpu.ui.indices[index_i + 0] = 0 | encoded;
  APP.gpu.ui.indices[index_i + 1] = 1 | encoded;
  APP.gpu.ui.indices[index_i + 2] = 2 | encoded;
  APP.gpu.ui.indices[index_i + 3] = 2 | encoded;
  APP.gpu.ui.indices[index_i + 4] = 1 | encoded;
  APP.gpu.ui.indices[index_i + 5] = 3 | encoded;
}

static void UI_DrawRect(UI_Shape shape)
{
  shape.tex_layer = -1.f;
  UI_DrawRaw(shape);
}

static void UI_PostFrame()
{
  // ui
  APP.gpu.ui.indices_count = 0;
  APP.gpu.ui.shapes_count = 0;
  APP.gpu.ui.clips[0] = (UI_Clip){0,0,FLT_MAX,FLT_MAX};
  APP.gpu.ui.clips_count = 1;
  APP.gpu.ui.clip_stack[0] = 0;
  APP.gpu.ui.clip_stack_index = 0;
}
