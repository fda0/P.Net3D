typedef enum
{
  AnimChannel_Translation,
  AnimChannel_Rotation,
  AnimChannel_Scale,
} Anim_ChannelType;

typedef struct
{
  U32 joint_index : 24;
  Anim_ChannelType type : 8;
  U32 count;
  float *inputs; // 1 * count
  float *outputs; // (type == Rotation ? 4 : 3) * count
  // add interpolation type? assume linear for now
} Anim_Channel;

typedef struct
{
  Anim_Channel *channels;
  U32 channel_count;
  float t_min, t_max;
} Animation;
