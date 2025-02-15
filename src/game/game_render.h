typedef enum
{
  RdrModel_Teapot,
  RdrModel_Flag,
  RdrModel_COUNT
} Rdr_ModelType;

typedef struct
{
  Mat4 transform;
  V4 color;
} Rdr_ModelInstanceData;

typedef struct
{
  V3 p;
  V3 color;
  V3 normal;
} Rdr_ModelVertex;

typedef struct
{
  V3 p;
  V3 color;
  V3 normal;
  V3 uv;
} Rdr_WallVertex;

typedef struct
{
#define RDR_MAX_MODEL_INSTANCES 16
  Rdr_ModelInstanceData data[RDR_MAX_MODEL_INSTANCES];
  U32 count;
} Rdr_Model;

typedef struct
{
  Rdr_Model models[RdrModel_COUNT];

  Rdr_WallVertex wall_verts[1024 * 8];
  U32 wall_vert_count;
} Rdr_State;
