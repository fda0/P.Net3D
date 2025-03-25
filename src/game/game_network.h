#define NET_DEFAULT_SEVER_PORT 21037
#define NET_MAGIC_VALUE 0xfda0

#define NET_CLIENT_MAX_SNAPSHOTS (TICK_RATE)

#define NET_MAX_INPUT_TICKS (TICK_RATE/8)
#define NET_MAX_PLAYERS 10

#define NET_MAX_PACKET_SIZE 1200
#define NET_MAX_PAYLOAD_SIZE (NET_MAX_PACKET_SIZE - sizeof(NET_PacketHeader))
#define NET_SIMULATE_PACKETLOSS 0 // doesn't seem to work on localhost

#define NET_INACTIVE_MS (100)
#define NET_TIMEOUT_DISCONNECT_MS (250)

typedef struct
{
  SDLNet_Address *address;
  U16 port;
  U64 last_msg_timestamp;
} NET_User;

typedef enum
{
  NetSendKind_None,
  NetSendKind_Ping = 10000,
  NetSendKind_ObjUpdate,
  NetSendKind_ObjEmpty,
  NetSendKind_NetworkTest,
  NetSendKind_Inputs,
  NetSendKind_AssignPlayerKey,
  NetSendKind_WindowLayout,
} NET_SendKind;

typedef struct
{
  U64 tick_id; // this is already send via packet header
  NET_SendKind kind;
} NET_SendHeader;

typedef struct
{
  TICK_Input inputs[NET_MAX_INPUT_TICKS];
  U16 input_count;
} NET_SendInputs;

typedef struct
{
  U64 number;
} NET_SendPing;

typedef struct
{
  U32 net_index;
  OBJ_Sync sync;
} NET_SendObjSync;

typedef struct
{
  U32 net_index;
} NET_SendObjEmpty;

typedef struct
{
  OBJ_Key player_key;
} NET_SendAssignPlayerKey;

typedef struct
{
  U32 user_count;
  I32 px, py, w, h;
} NET_SendWindowLayout;

#pragma pack(push, 1)
typedef struct
{
  U16 magic_value; // use this as seed for hash calculation instead
  U16 payload_hash;
} NET_PacketHeader;
#pragma pack(pop)

static bool NET_IsServer();
static bool NET_IsClient();
static const char *NET_Label();
