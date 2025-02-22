#pragma once

#define DEBUGLOG(x) if (settings::VERBOSITY) {x};

#include <iostream>
#include <stdio.h>
#include <stdint.h>
#ifdef _WIN32
// windows
    #ifndef NOMINMAX
    #define NOMINMAX
    #endif
    #define M_PI 3.14159265358979323846
    #define _WINSOCK_DEPRECATED_NO_WARNINGS
    #include <winsock2.h>
    #include <windows.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "Ws2_32.lib")

    typedef char buffer_t;
    #define PollFD WSAPOLLFD
    #define poll WSAPoll
    #define OF_ERRNO WSAGetLastError()
    #define OF_EWOULD WSAEWOULDBLOCK
    #define SOCKETINVALID(x) (x == INVALID_SOCKET)
    #define SOCKETERROR(x) (x == SOCKET_ERROR)
#else
// posix platform
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <poll.h>
    #include <unistd.h>
    #include <errno.h>

    typedef int SOCKET;
    typedef void buffer_t;
    #define PollFD struct pollfd
    #define OF_ERRNO errno
    #define OF_EWOULD EWOULDBLOCK
    #define SOCKETINVALID(x) (x < 0)
    #define SOCKETERROR(x) (x == -1)
#endif
#include <fcntl.h>

#include <string>
#include <cstring>
#include <csignal>
#include <list>
#include <queue>
#include <unordered_map>
#include <vector>
#include <algorithm>

#include "Defines.hpp"
#include "settings.hpp"

#if defined(__MINGW32__) && !defined(_GLIBCXX_HAS_GTHREADS)
    #include "mingw/mingw.mutex.h"
#else
    #include <mutex>
#endif

/*
 * Packets format (sent from the client):
 *     [4 bytes] - size of packet including the 4 byte packet type
 *     [size bytes] - Encrypted packet (byte swapped && xor'd with 8 byte key; see CNSocketEncryption)
 *         [4 bytes] - packet type (which is a combination of the first 4 bytes of the packet and a checksum in some versions)
 *         [structure] - one member contains length of trailing data (expressed in packet-dependant structures)
 *         [trailing data] - optional variable-length data that only some packets make use of
 */

// error checking calloc wrapper
inline void* xmalloc(size_t sz) {
    void* res = calloc(1, sz);

    if (res == NULL) {
        std::cerr << "[FATAL] OpenFusion: out of memory!" << std::endl;
        exit(EXIT_FAILURE);
    }

    return res;
}

// overflow-safe validation of variable-length packets
// for outbound packets
inline bool validOutVarPacket(size_t base, int32_t npayloads, size_t plsize) {
    // check for multiplication overflow
    if (npayloads > 0 && (CN_PACKET_BUFFER_SIZE - 8) / (size_t)npayloads < plsize)
        return false;

    // it's safe to multiply
    size_t trailing = npayloads * plsize;

    // does it fit in a packet?
    if (base + trailing > CN_PACKET_BUFFER_SIZE - 8)
        return false;

    // everything is a-ok!
    return true;
}

// for inbound packets
inline bool validInVarPacket(size_t base, int32_t npayloads, size_t plsize, size_t datasize) {
    // check for multiplication overflow
    if (npayloads > 0 && (CN_PACKET_BUFFER_SIZE - 8) / (size_t)npayloads < plsize)
        return false;

    // it's safe to multiply
    size_t trailing = npayloads * plsize;

    // make sure size is exact
    // datasize has already been validated against CN_PACKET_BUFFER_SIZE
    if (datasize != base + trailing)
        return false;

    // everything is a-ok!
    return true;
}

void printSocketError(const char *func);
bool setSockNonblocking(SOCKET listener, SOCKET newSock);

namespace CNSocketEncryption {
    // you won't believe how complicated they made it in the client :facepalm:
    static constexpr const char* defaultKey = "a>$rT~!Q";
    static const unsigned int keyLength = 8;
    static const unsigned int iV = 268403508;

    int Encrypt_byte_change_A(int ERSize, uint8_t* data, int size);
    int xorData(uint8_t* buffer, uint8_t* key, int size);
    uint32_t validateSum(uint8_t* buffer, uint32_t type, int size);
    uint64_t createNewKey(uint64_t uTime, int32_t iv1, int32_t iv2);
    int encryptData(uint8_t* buffer, uint8_t* key, int size);
    int decryptData(uint8_t* buffer, uint8_t* key, int size);
}

struct CNPacketData {
    void* buf;
    int size;
    uint32_t type;

    CNPacketData(void* b, uint32_t t, int l);
};

enum ACTIVEKEY {
    SOCKETKEY_E,
    SOCKETKEY_FE
};

class CNSocket;
typedef void (*PacketHandler)(CNSocket* sock, CNPacketData* data);

class CNSocket {
private:
    uint64_t EKey;
    uint64_t FEKey;
    int32_t readSize = 0;
    uint8_t readBuffer[CN_PACKET_BUFFER_SIZE];
    int readBufferIndex = 0;
    bool activelyReading = false;
    bool alive = true;

    ACTIVEKEY activeKey;

    bool sendData(uint8_t* data, int size);
    int recvData(buffer_t* data, int size);

public:
    SOCKET sock;
    sockaddr_in sockaddr;
    PacketHandler pHandler;

    CNSocket(SOCKET s, struct sockaddr_in &addr, PacketHandler ph);

    void setEKey(uint64_t k);
    void setFEKey(uint64_t k);
    uint64_t getEKey();
    uint64_t getFEKey();
    void setActiveKey(ACTIVEKEY t);

    void kill();
    void sendPacket(void* buf, uint32_t packetType, size_t size);
    void step();
    bool isAlive();
};

class CNServer;
typedef void (*TimerHandler)(CNServer* serv, time_t time);

// timer struct
struct TimerEvent {
    TimerHandler handlr;
    time_t delta; // time to be added to the current time on reset
    time_t scheduledEvent; // time to call handlr()

    TimerEvent(TimerHandler h, time_t d): handlr(h), delta(d) {
        scheduledEvent = 0;
    }
};

// in charge of accepting new connections and making sure each connection is kept alive
class CNServer {
protected:
    std::unordered_map<SOCKET, CNSocket*> connections;
    std::mutex activeCrit;

    const size_t STARTFDSCOUNT = 8; // number of initial PollFD slots
    std::vector<PollFD> fds;

    SOCKET sock;
    uint16_t port;
    socklen_t addressSize;
    struct sockaddr_in address;
    void init();

    bool active = true;

public:
    PacketHandler pHandler;

    CNServer();
    CNServer(uint16_t p);

    void addPollFD(SOCKET s);
    void removePollFD(int i);

    void start();
    void kill();
    static void printPacket(CNPacketData *data, int type);
    virtual bool checkExtraSockets(int i);
    virtual void newConnection(CNSocket* cns);
    virtual void killConnection(CNSocket* cns);
    virtual void onStep();
};
