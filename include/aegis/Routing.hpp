#pragma once

#include "aegis/Protocol.hpp"

namespace aegis {

struct Neighbor {
    uint8_t nodeId = 0;
    uint8_t destinationId = 0;
    uint8_t costToDestination = 0xFF;
    uint8_t bufferPercent = 0;
    int8_t rssi = -127;
    uint32_t lastHeardAtMs = 0;
    bool occupied = false;
};

class RoutingTable {
public:
    void update(uint8_t nodeId, uint8_t destinationId, uint8_t cost,
                uint8_t bufferPercent, int8_t rssi, uint32_t nowMs);
    uint8_t costTo(uint8_t destinationId, uint32_t nowMs) const;
    uint8_t nextHop(uint8_t destinationId, uint32_t visitedMask, uint32_t nowMs) const;
    uint8_t freeSlots(uint32_t nowMs) const;
    const Neighbor* neighbor(uint8_t nodeId, uint32_t nowMs) const;

private:
    Neighbor neighbors_[kNeighborCapacity]{};
};

}  // namespace aegis
