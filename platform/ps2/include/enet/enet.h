/* Only the data types used by the local game. No sockets or ENet runtime. */
#pragma once
#include <stddef.h>
#include <stdint.h>
typedef uint8_t enet_uint8;
typedef uint16_t enet_uint16;
typedef uint32_t enet_uint32;
typedef int ENetSocket;
typedef struct { enet_uint32 host; enet_uint16 port; } ENetAddress;
typedef struct { ENetAddress address; } ENetPeer;
typedef struct ENetHost ENetHost;
typedef struct { enet_uint8 *data; size_t dataLength; } ENetPacket;
#define ENET_SOCKET_NULL (-1)
#define ENET_PACKET_FLAG_RELIABLE 1
int enet_initialize(void);
void enet_deinitialize(void);
int enet_address_set_host(ENetAddress *address, const char *name);
int enet_address_get_host_ip(const ENetAddress *address, char *name, size_t size);
ENetPacket *enet_packet_create(const void *data, size_t size, enet_uint32 flags);
void enet_packet_destroy(ENetPacket *packet);
