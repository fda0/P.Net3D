typedef struct
{
  float scale; // set to 1 if equal to 0
  float rot_x, rot_y, rot_z;
} M_ModelSpec;

typedef struct
{
  const char *path;
  S8 src;
  U64 at;
} M_ObjParser;

typedef enum
{
  M_ObjToken_EOF,
  M_ObjToken_String,
  M_ObjToken_Slash,
  M_ObjToken_Float,
  M_ObjToken_Int,
} M_ObjTokenKind;

typedef struct
{
  M_ObjTokenKind kind;
  S8 text;
} M_ObjToken;

typedef struct
{
  I32 pos, tex, nrm;
  S8 material;
} M_ObjFacePart;