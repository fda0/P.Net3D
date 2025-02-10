#define NET_DEFAULT_SEVER_PORT 21037
#define NET_MAGIC_VALUE 0xfda0

#define NET_MAX_TICK_HISTORY TICK_RATE // 1 second of history
#define NET_CLIENT_MAX_SNAPSHOTS TICK_RATE

#define NET_MAX_INPUT_TICKS (TICK_RATE/8)
#define NET_MAX_PLAYERS 10

#define NET_MAX_PACKET_SIZE 1200
#define NET_MAX_PAYLOAD_SIZE (NET_MAX_PACKET_SIZE - sizeof(Net_PacketHeader))
#define NET_SIMULATE_PACKETLOSS 0 // doesn't seem to work on localhost

typedef struct
{
    SDLNet_Address *address;
    U16 port;
    U64 last_msg_frame_time;
} Net_User;

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
} Net_SendKind;

typedef struct
{
    U64 tick_id; // this is already send via packet header
    Net_SendKind kind;
} Net_SendHeader;

typedef struct
{
    Tick_Input inputs[NET_MAX_INPUT_TICKS];
    U16 input_count;
} Net_SendInputs;

typedef struct
{
    U64 number;
} Net_SendPing;

typedef struct
{
    U32 net_index;
    Obj_Sync sync;
} Net_SendObjSync;

typedef struct
{
    U32 net_index;
} Net_SendObjEmpty;

typedef struct
{
    Obj_Key player_key;
} Net_SendAssignPlayerKey;

typedef struct
{
    U32 user_count;
    I32 px, py, w, h;
} Net_SendWindowLayout;

#pragma pack(push, 1)
typedef struct
{
    U16 magic_value; // use this as seed for hash calculation instead
    U16 payload_hash;
} Net_PacketHeader;
#pragma pack(pop)

static bool Net_IsServer();
static bool Net_IsClient();
static const char *Net_Label();
