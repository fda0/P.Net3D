typedef struct
{
  Mat4 *mats;
  U32 mats_count;
} ANIM_Pose;

typedef struct
{
  bool is_active;
  bool loop;
  U32 animation_index;
  U64 start_at_tick;
} ANIM_PlaybackRequest;

typedef struct
{
  ANIM_PlaybackRequest req;
  float time;
  float weight;
} ANIM_Playback;
