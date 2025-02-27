typedef enum
{
  AN_Translation,
  AN_Rotation,
  AN_Scale,
} AN_TransformType;

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
  char *name;
  AN_Channel *channels;
  U32 channels_count;
  float t_min, t_max;
} AN_Animation;

typedef struct
{
  AN_Animation *animations;
  U32 animations_count;

  // --- joints section ---
  // all pointers point to arrays with
  // joints_count number of elements
  U32 joints_count;
  const char **names;
  Mat4 *inverse_bind_matrices;
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
  Mat4 *matrices;
  U32 matrices_count;
} AN_Pose;
