#include "net_client.h"
#include "net_server.h"
#include <stdlib.h>
#include <string.h>

NetClient gNetClient;
NetServer gNetServer;
int enet_initialize(void) { return 0; }
void enet_deinitialize(void) {}
int enet_address_set_host(ENetAddress *a, const char *name)
{ (void)a; (void)name; return -1; }
int enet_address_get_host_ip(const ENetAddress *a, char *name, size_t size)
{ (void)a; if (size) name[0] = '\0'; return -1; }
ENetPacket *enet_packet_create(const void *data, size_t size, enet_uint32 flags)
{
    (void)flags;
    ENetPacket *p = calloc(1, sizeof *p);
    if (!p) return NULL;
    p->data = calloc(1, size);
    if (!p->data) { free(p); return NULL; }
    p->dataLength = size;
    /* NetEncode passes only the four-byte message id, then fills the body. */
    if (data && size >= sizeof(uint32_t)) memcpy(p->data, data, sizeof(uint32_t));
    return p;
}
void enet_packet_destroy(ENetPacket *p)
{ if (p) { free(p->data); free(p); } }
void NetClientInit(NetClient *n, const uint16_t port)
{
    memset(n, 0, sizeof *n);
    n->ClientId = -1;
    n->scanner = ENET_SOCKET_NULL;
    n->port = port;
    CArrayInit(&n->ScannedAddrs, sizeof(ScanInfo));
    CArrayInit(&n->scannedAddrBuf, sizeof(ScanInfo));
}
void NetClientTerminate(NetClient *n)
{ CArrayTerminate(&n->ScannedAddrs); CArrayTerminate(&n->scannedAddrBuf); }
void NetClientFindLANServers(NetClient *n) { (void)n; }
bool NetClientTryConnect(NetClient *n, const ENetAddress addr)
{ (void)n; (void)addr; return false; }
bool NetClientTryScanAndConnect(NetClient *n, const enet_uint32 host)
{ (void)n; (void)host; return false; }
void NetClientDisconnect(NetClient *n) { n->peer = NULL; n->Ready = false; }
void NetClientPoll(NetClient *n) { (void)n; }
void NetClientFlush(NetClient *n) { (void)n; }
void NetClientSendMsg(NetClient *n, const GameEventType e, const void *data)
{ (void)n; (void)e; (void)data; }
bool NetClientIsConnected(const NetClient *n) { (void)n; return false; }
void NetServerInit(NetServer *n) { memset(n, 0, sizeof *n); n->listen = ENET_SOCKET_NULL; }
void NetServerTerminate(NetServer *n) { NetServerClose(n); }
void NetServerReset(NetServer *n) { NetServerInit(n); }
void NetServerOpen(NetServer *n, const uint16_t port) { (void)n; (void)port; }
void NetServerClose(NetServer *n) { n->server = NULL; }
void NetServerPoll(NetServer *n) { (void)n; }
void NetServerFlush(NetServer *n) { (void)n; }
void NetServerSendMsg(NetServer *n, const int peer, const GameEventType e, const void *data)
{ (void)n; (void)peer; (void)e; (void)data; }
void NetServerSendGameStartMessages(NetServer *n, const int peer) { (void)n; (void)peer; }
