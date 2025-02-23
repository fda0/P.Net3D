typedef enum
{
  AnimChannel_Translation,
  AnimChannel_Rotation,
  AnimChannel_Scale,
} Anim_ChannelType;

typedef struct
{
  Anim_ChannelType type;
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
