#include "aegis/Routing.hpp"

namespace aegis {
namespace {

bool isFresh(const Neighbor& neighbor, uint32_t nowMs) {
    return neighbor.occupied &&
           static_cast<uint32_t>(nowMs - neighbor.lastHeardAtMs) <= kNeighborTimeoutMs;
}

bool hasFreshRoute(const Neighbor& neighbor, uint8_t destinationId, uint32_t nowMs) {
    return static_cast<uint32_t>(nowMs - neighbor.routeUpdatedAtMs[destinationId]) <=
           kNeighborTimeoutMs;
}

}  // namespace

void RoutingTable::update(uint8_t nodeId, uint8_t destinationId, uint8_t cost,
                          uint8_t bufferPercent, int8_t rssi, uint32_t nowMs) {
    if (nodeId == 0 || nodeId > kNodeCount || destinationId == 0 ||
        destinationId > kNodeCount || bufferPercent > 100) {
        return;
    }

    size_t slot = kNeighborCapacity;
    for (size_t i = 0; i < kNeighborCapacity; ++i) {
        if (neighbors_[i].occupied && neighbors_[i].nodeId == nodeId) {
            slot = i;
            break;
        }
    }
    if (slot == kNeighborCapacity) {
        size_t oldest = kNeighborCapacity;
        for (size_t i = 0; i < kNeighborCapacity; ++i) {
            if (!neighbors_[i].occupied) {
                slot = i;
                break;
            }
            if (oldest == kNeighborCapacity || !isFresh(neighbors_[i], nowMs) ||
                (isFresh(neighbors_[oldest], nowMs) &&
                 static_cast<int32_t>(neighbors_[i].lastHeardAtMs -
                                      neighbors_[oldest].lastHeardAtMs) < 0)) {
                oldest = i;
            }
        }
        if (slot == kNeighborCapacity) {
            slot = oldest;
        }
    }

    if (!neighbors_[slot].occupied || neighbors_[slot].nodeId != nodeId) {
        neighbors_[slot] = Neighbor{};
        neighbors_[slot].nodeId = nodeId;
        neighbors_[slot].occupied = true;
    }

    Neighbor& neighbor = neighbors_[slot];
    neighbor.costToDestination[destinationId] = cost;
    neighbor.routeUpdatedAtMs[destinationId] = nowMs;
    neighbor.bufferPercent = bufferPercent;
    neighbor.rssi = rssi;
    neighbor.lastHeardAtMs = nowMs;
}

uint8_t RoutingTable::costTo(uint8_t destinationId, uint32_t nowMs) const {
    if (destinationId == 0 || destinationId > kNodeCount) {
        return 0xFF;
    }
    if (destinationId == kNodeId) {
        return 0;
    }

    if (neighbor(destinationId, nowMs) != nullptr) {
        return 1;
    }

    uint8_t best = 0xFF;
    for (const Neighbor& candidate : neighbors_) {
        const uint8_t advertisedCost = candidate.costToDestination[destinationId];
        if (isFresh(candidate, nowMs) && hasFreshRoute(candidate, destinationId, nowMs) &&
            advertisedCost < 0xFE &&
            static_cast<uint8_t>(advertisedCost + 1U) < best) {
            best = static_cast<uint8_t>(advertisedCost + 1U);
        }
    }
    return best;
}

uint8_t RoutingTable::nextHop(uint8_t destinationId, uint32_t visitedMask, uint32_t nowMs,
                              bool requireRelay) const {
    if (destinationId == 0 || destinationId > kNodeCount) {
        return 0;
    }

    uint8_t bestNode = 0;
    uint8_t bestCost = 0xFF;
    int8_t bestRssi = -127;
    for (const Neighbor& candidate : neighbors_) {
        const uint8_t advertisedCost = candidate.costToDestination[destinationId];
        const bool isDestination = candidate.nodeId == destinationId;
        const bool hasRoute = isDestination ||
                              (hasFreshRoute(candidate, destinationId, nowMs) &&
                               advertisedCost != 0xFF);
        if (!isFresh(candidate, nowMs) || !hasRoute || (requireRelay && isDestination) ||
            (visitedMask & nodeMask(candidate.nodeId)) != 0U) {
            continue;
        }
        const uint8_t effectiveCost = isDestination ? 0 : advertisedCost;
        if (bestNode == 0 || candidate.rssi > bestRssi ||
            (candidate.rssi == bestRssi && effectiveCost < bestCost) ||
            (candidate.rssi == bestRssi && effectiveCost == bestCost &&
             candidate.nodeId < bestNode)) {
            bestNode = candidate.nodeId;
            bestCost = effectiveCost;
            bestRssi = candidate.rssi;
        }
    }
    return bestNode;
}

uint8_t RoutingTable::nextHopTinyMl(uint8_t destinationId, uint32_t visitedMask,
                                    uint32_t nowMs, int16_t& selectedScore,
                                    bool requireRelay) const {
    if (destinationId == 0 || destinationId > kNodeCount) {
        selectedScore = -32768;
        return 0;
    }
    uint8_t bestNode = 0;
    int16_t bestScore = -32768;
    int8_t bestRssi = -127;
    for (const Neighbor& candidate : neighbors_) {
        const uint8_t advertisedCost = candidate.costToDestination[destinationId];
        const bool isDestination = candidate.nodeId == destinationId;
        const bool hasRoute = isDestination ||
                              (hasFreshRoute(candidate, destinationId, nowMs) &&
                               advertisedCost != 0xFF);
        if (!isFresh(candidate, nowMs) || !hasRoute || (requireRelay && isDestination) ||
            (visitedMask & nodeMask(candidate.nodeId)) != 0U) {
            continue;
        }

        // Strongest RSSI is primary; route length and buffer only break RSSI ties.
        const uint8_t effectiveCost = isDestination ? 0 : advertisedCost;
        const int16_t score = static_cast<int16_t>(
            160 - static_cast<int16_t>(effectiveCost) * 100 -
            static_cast<int16_t>(candidate.bufferPercent));
        if (bestNode == 0 || candidate.rssi > bestRssi ||
            (candidate.rssi == bestRssi && score > bestScore) ||
            (candidate.rssi == bestRssi && score == bestScore &&
             candidate.nodeId < bestNode)) {
            bestNode = candidate.nodeId;
            bestScore = score;
            bestRssi = candidate.rssi;
        }
    }
    selectedScore = bestNode == 0 ? -32768 : bestScore;
    return bestNode;
}

uint8_t RoutingTable::freeSlots(uint32_t nowMs) const {
    uint8_t count = 0;
    for (const Neighbor& candidate : neighbors_) {
        if (isFresh(candidate, nowMs)) {
            ++count;
        }
    }
    return count;
}

const Neighbor* RoutingTable::neighbor(uint8_t nodeId, uint32_t nowMs) const {
    for (const Neighbor& candidate : neighbors_) {
        if (candidate.nodeId == nodeId && isFresh(candidate, nowMs)) {
            return &candidate;
        }
    }
    return nullptr;
}

}  // namespace aegis
