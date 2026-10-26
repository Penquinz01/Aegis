#pragma once

#include <cstddef>
#include <cstdint>

#include <esp_now.h>

#include "aegis/Config.hpp"

namespace aegis {

struct ReceivedFrame {
    uint8_t mac[6]{};
    int8_t rssi = -127;
    uint16_t length = 0;
    uint8_t data[kMaximumFrameBytes]{};
};

class EspNowTransport {
public:
    bool begin(uint8_t channel);
    bool receive(ReceivedFrame& frame);
    bool sendBroadcast(const uint8_t* data, size_t length);
    bool sendToNode(uint8_t nodeId, const uint8_t* data, size_t length);
    bool registerPeer(uint8_t nodeId, const uint8_t mac[6]);
    bool macForNode(uint8_t nodeId, uint8_t mac[6]) const;

private:
    struct PeerEntry {
        uint8_t nodeId = 0;
        uint8_t mac[6]{};
        bool occupied = false;
    };

    static void onReceive(const esp_now_recv_info_t* info, const uint8_t* data, int length);
    static bool addMacAsPeer(const uint8_t mac[6]);

    static void* receiveQueue_;
    PeerEntry peers_[kNeighborCapacity]{};
    uint8_t broadcastMac_[6]{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
};

}  // namespace aegis
