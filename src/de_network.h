#define NET_DEFAULT_SEVER_PORT 21037
#define NET_MAGIC_VALUE 0xfda0
#define NET_MAX_TICK_HISTORY (TICK_RATE) // 1 second of history
#define NET_MAX_NETWORK_OBJECTS 16
#define NET_MAX_PACKET_SIZE (32768/64)
#define NET_MAX_PAYLOAD_SIZE (NET_MAX_PACKET_SIZE - sizeof(Net_PacketHeader))
#define NET_MAX_PACKET_CHAIN_LENGTH 64
#define NET_SIMULATE_PACKETLOSS 0 // doesn't seem to work on localhost

// More/less logging switch
//#define NET_VERBOSE_LOG(...)
#define NET_VERBOSE_LOG(...) SDL_Log(__VA_ARGS__)

typedef struct
{
    Uint16 magic_value;
    Uint8 packet_count;
    Uint8 packet_id;
    Uint32 packet_payload_hash;
    Uint64 tick_id;
} Net_PacketHeader;

typedef struct
{
    Uint64 tick_id;
    Uint8 packet_count;
    Uint32 packet_sizes[NET_MAX_PACKET_CHAIN_LENGTH];
    Uint8 packet_buf[NET_MAX_PACKET_CHAIN_LENGTH * NET_MAX_PAYLOAD_SIZE];
} Net_PacketChain;

static S8 Net_GetChainBuffer(Net_PacketChain *chain, Uint32 packet_index)
{
    S8 res = {};
    if (packet_index < NET_MAX_PACKET_CHAIN_LENGTH)
    {
        res = S8_Make(chain->packet_buf + packet_index*NET_MAX_PAYLOAD_SIZE,
                      NET_MAX_PAYLOAD_SIZE);
    }
    return res;
}
