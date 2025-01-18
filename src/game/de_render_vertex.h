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

    U16 wall_indices[1024 * 36];
    Rdr_Vertex wall_verts[1024 * 8];
    U32 wall_vert_count;
} Rdr_State;
