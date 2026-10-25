#include "aegis/Routing.hpp"

namespace aegis {
namespace {

bool isFresh(const Neighbor& neighbor, uint32_t nowMs) {
    return neighbor.occupied &&
           static_cast<uint32_t>(nowMs - neighbor.lastHeardAtMs) <= kNeighborTimeoutMs;
}

}  // namespace

void RoutingTable::update(uint8_t nodeId, uint8_t destinationId, uint8_t cost,
                          uint8_t bufferPercent, int8_t rssi, uint32_t nowMs) {
    if (nodeId == 0 || nodeId > kNodeCount || destinationId == 0 ||
        destinationId > kNodeCount || bufferPercent > 100) {
        return;
    }

    size_t slot = kNeighborCapacity;
    size_t oldest = 0;
    for (size_t i = 0; i < kNeighborCapacity; ++i) {
        if (neighbors_[i].occupied && neighbors_[i].nodeId == nodeId) {
            slot = i;
            break;
        }
        if (!neighbors_[i].occupied) {
            slot = i;
            break;
        }
        if (!isFresh(neighbors_[i], nowMs) ||
            static_cast<int32_t>(neighbors_[i].lastHeardAtMs - neighbors_[oldest].lastHeardAtMs) < 0) {
            oldest = i;
        }
    }
    if (slot == kNeighborCapacity) {
        slot = oldest;
    }

    neighbors_[slot] = Neighbor{nodeId, destinationId, cost, bufferPercent, rssi, nowMs, true};
}

uint8_t RoutingTable::costTo(uint8_t destinationId, uint32_t nowMs) const {
    if (destinationId == kNodeId) {
        return 0;
    }

    if (neighbor(destinationId, nowMs) != nullptr) {
        return 1;
    }

    uint8_t best = 0xFF;
    for (const Neighbor& candidate : neighbors_) {
        if (isFresh(candidate, nowMs) && candidate.destinationId == destinationId &&
            candidate.costToDestination < 0xFE &&
            static_cast<uint8_t>(candidate.costToDestination + 1U) < best) {
            best = static_cast<uint8_t>(candidate.costToDestination + 1U);
        }
    }
    return best;
}

uint8_t RoutingTable::nextHop(uint8_t destinationId, uint32_t visitedMask, uint32_t nowMs) const {
    if (destinationId != kNodeId &&
        (visitedMask & nodeMask(destinationId)) == 0U && neighbor(destinationId, nowMs) != nullptr) {
        return destinationId;
    }

    uint8_t bestNode = 0;
    uint8_t bestCost = 0xFF;
    int8_t bestRssi = -127;
    for (const Neighbor& candidate : neighbors_) {
        if (!isFresh(candidate, nowMs) || candidate.destinationId != destinationId ||
            candidate.costToDestination == 0xFF ||
            (visitedMask & nodeMask(candidate.nodeId)) != 0U) {
            continue;
        }
        if (candidate.costToDestination < bestCost ||
            (candidate.costToDestination == bestCost && candidate.rssi > bestRssi)) {
            bestNode = candidate.nodeId;
            bestCost = candidate.costToDestination;
            bestRssi = candidate.rssi;
        }
    }
    return bestNode;
}

uint8_t RoutingTable::nextHopTinyMl(uint8_t destinationId, uint32_t visitedMask,
                                    uint32_t nowMs, int16_t& selectedScore) const {
    uint8_t bestNode = 0;
    int16_t bestScore = -32768;
    for (const Neighbor& candidate : neighbors_) {
        if (!isFresh(candidate, nowMs) || candidate.destinationId != destinationId ||
            candidate.costToDestination == 0xFF ||
            (visitedMask & nodeMask(candidate.nodeId)) != 0U) {
            continue;
        }

        // Quantized linear model: favor shorter routes, then stronger links and spare buffer.
        const int16_t signalFeature = static_cast<int16_t>(candidate.rssi + 100);
        const int16_t score = static_cast<int16_t>(
            160 - static_cast<int16_t>(candidate.costToDestination) * 100 +
            signalFeature * 2 - static_cast<int16_t>(candidate.bufferPercent));
        if (bestNode == 0 || score > bestScore ||
            (score == bestScore && candidate.nodeId < bestNode)) {
            bestNode = candidate.nodeId;
            bestScore = score;
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
