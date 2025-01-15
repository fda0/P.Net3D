typedef struct
{
    V3 p, color;
    V3 normal;
} Rdr_Vertex;

typedef struct
{
    Mat4 transform;
    V4 color;
} Rdr_ModelInstanceData;

typedef struct
{
#define RDR_MAX_MODEL_INSTANCES 16
    Rdr_ModelInstanceData instance_data[RDR_MAX_MODEL_INSTANCES];
    U32 instance_count;
} Rdr_State;
