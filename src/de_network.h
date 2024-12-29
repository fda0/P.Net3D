#define NET_DEFAULT_SEVER_PORT 21037
#define NET_MAGIC_VALUE 0xfda0
#define NET_MAX_TICK_HISTORY (TICK_RATE) // 1 second of history
#define NET_MAX_NETWORK_OBJECTS 16
#define NET_MAX_PACKET_SIZE (1200)
#define NET_MAX_PAYLOAD_SIZE (NET_MAX_PACKET_SIZE - sizeof(Net_PacketHeader))
#define NET_SIMULATE_PACKETLOSS 0 // doesn't seem to work on localhost

// More/less logging switch
#define NET_VERBOSE_LOG(...)
//#define NET_VERBOSE_LOG(...) SDL_Log(__VA_ARGS__)

#pragma pack(push, 1)
typedef struct
{
    Uint16 magic_value; // use this as seed for hash calculation instead
    Uint16 payload_hash;
} Net_PacketHeader;
#pragma pack(pop)
