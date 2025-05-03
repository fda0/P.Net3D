typedef struct
{
  U32 joint_index : 30;
  AN_TransformType type : 2;
  U32 count;
  float *inputs; // 1 * count
  float *outputs; // (type == Rotation ? 4 : 3) * count
  // add interpolation type? assume linear for now
} AN_Channel;

typedef struct
{
  char *name; // @todo delete
  S8 name_s8;
  float t_min, t_max;
  AN_Channel *channels;
  U32 channels_count;
} AN_Animation;

typedef struct
{
  Mat4 root_transform;

  // --- animations ---
  AN_Animation *animations;
  U32 animations_count;

  // --- joints section ---
  // all pointers point to arrays with
  // joints_count number of elements
  U32 joints_count;
  const char **names; // @todo delete
  S8 *names_s8;
  Mat4 *inv_bind_mats;
  //
  U32 *child_index_buf;
  RngU32 *child_index_ranges;
  // rest pose translations, rotations, scales
  V3 *translations;
  Quat *rotations;
  V3 *scales;
} AN_Skeleton;

typedef struct
{
  Mat4 *mats;
  U32 mats_count;
} AN_Pose;
