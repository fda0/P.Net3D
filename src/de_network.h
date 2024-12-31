#define NET_DEFAULT_SEVER_PORT 21037
#define NET_MAGIC_VALUE 0xfda0

#define NET_MAX_TICK_HISTORY TICK_RATE // 1 second of history
#define NET_CLIENT_MAX_SNAPSHOTS TICK_RATE
#define NET_MAX_NETWORK_OBJECTS 24

#define NET_MAX_INPUT_TICKS (TICK_RATE/4)
#define NET_MAX_PLAYERS 10

#define NET_MAX_PACKET_SIZE 1200
#define NET_MAX_PAYLOAD_SIZE (NET_MAX_PACKET_SIZE - sizeof(Net_PacketHeader))
#define NET_SIMULATE_PACKETLOSS 0 // doesn't seem to work on localhost

typedef struct
{
    SDLNet_Address *address;
    Uint16 port;
} Net_User;

typedef enum
{
    NetCmd_None,
    NetCmd_Ping,
    NetCmd_ObjUpdate,
    NetCmd_ObjEmpty,
    NetCmd_NetworkTest,
    NetCmd_Inputs,
} Net_CmdKind;

typedef struct
{
    Uint64 tick_id; // this is already send via packet header
    Net_CmdKind kind;
} Net_Cmd;

typedef struct
{
    Tick_Input ticks[NET_MAX_INPUT_TICKS];
    Uint16 tick_count;
} Net_Inputs;

typedef struct
{
    Uint64 number;
} Net_Ping;

typedef struct
{
    Uint32 net_slot;
    Object obj;
} Net_ObjUpdate;

typedef struct
{
    Uint32 net_slot;
} Net_ObjEmpty;

typedef struct
{
    Uint32 numbers[290];
} Net_Payload_NetworkTest;

#pragma pack(push, 1)
typedef struct
{
    Uint16 magic_value; // use this as seed for hash calculation instead
    Uint16 payload_hash;
} Net_PacketHeader;
#pragma pack(pop)

static bool Net_IsServer(AppState *app);
static bool Net_IsClient(AppState *app);
static const char *Net_Label(AppState *app);
