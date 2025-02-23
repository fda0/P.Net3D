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
  V3 *translations;
  Quat *rotations;
  V3 *scales;
  Mat4 *inverse_bind_matrices;
  U32 count;
} AN_RestPose;

typedef struct
{
  AN_Animation *animations;
  U32 animations_count;
  AN_RestPose rest_pose;
} AN_Skeleton;
