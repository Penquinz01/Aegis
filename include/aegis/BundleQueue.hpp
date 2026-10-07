#pragma once

#include "aegis/Protocol.hpp"

namespace aegis {

struct QueuedBundle {
    Packet packet{};
    uint32_t enqueuedAtMs = 0;
    uint32_t retryAtMs = 0;
    uint8_t retryCount = 0;
    bool occupied = false;
    bool awaitingAck = false;
};

class BundleQueue {
public:
    bool enqueue(const Packet& packet, uint32_t nowMs, bool& evictedLowerPriority,
                 uint64_t& evictedBundleId);
    bool nextDue(uint32_t nowMs, QueuedBundle& bundle, size_t& slotIndex);
    void markSent(size_t slotIndex, uint32_t nowMs);
    void defer(size_t slotIndex, uint32_t nowMs, uint32_t delayMs);
    bool acknowledge(uint64_t bundleId);
    size_t size() const;
    uint8_t bufferPercent() const;

private:
    QueuedBundle items_[kQueueCapacity]{};
    size_t size_ = 0;
};

}  // namespace aegis
