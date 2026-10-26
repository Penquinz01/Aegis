#include "aegis/BundleQueue.hpp"

namespace aegis {
namespace {

int priorityRank(Priority priority) {
    return static_cast<int>(priority);
}

bool reached(uint32_t nowMs, uint32_t deadlineMs) {
    return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
}

}  // namespace

bool BundleQueue::enqueue(const Packet& packet, uint32_t nowMs, bool& evictedLowerPriority,
                          uint64_t& evictedBundleId) {
    evictedLowerPriority = false;
    evictedBundleId = 0;
    size_t slot = kQueueCapacity;
    for (size_t i = 0; i < kQueueCapacity; ++i) {
        if (!items_[i].occupied) {
            slot = i;
            break;
        }
    }

    if (slot == kQueueCapacity) {
        size_t lowest = 0;
        for (size_t i = 1; i < kQueueCapacity; ++i) {
            const int candidateRank = priorityRank(items_[i].packet.priority);
            const int lowestRank = priorityRank(items_[lowest].packet.priority);
            if (candidateRank < lowestRank ||
                (candidateRank == lowestRank &&
                 static_cast<int32_t>(items_[i].enqueuedAtMs - items_[lowest].enqueuedAtMs) < 0)) {
                lowest = i;
            }
        }
        if (priorityRank(packet.priority) <= priorityRank(items_[lowest].packet.priority)) {
            return false;
        }
        slot = lowest;
        evictedLowerPriority = true;
        evictedBundleId = items_[lowest].packet.bundleId;
    } else {
        ++size_;
    }

    QueuedBundle item;
    item.packet = packet;
    item.enqueuedAtMs = nowMs;
    item.retryAtMs = nowMs;
    item.occupied = true;
    items_[slot] = item;
    return true;
}

bool BundleQueue::nextDue(uint32_t nowMs, QueuedBundle& bundle, size_t& slotIndex) {
    size_t best = kQueueCapacity;
    for (size_t i = 0; i < kQueueCapacity; ++i) {
        const QueuedBundle& item = items_[i];
        if (!item.occupied) {
            continue;
        }
        if (item.awaitingAck && !reached(nowMs, item.retryAtMs + kAckTimeoutMs)) {
            continue;
        }
        if (!item.awaitingAck && !reached(nowMs, item.retryAtMs)) {
            continue;
        }
        if (best == kQueueCapacity ||
            priorityRank(item.packet.priority) > priorityRank(items_[best].packet.priority) ||
            (item.packet.priority == items_[best].packet.priority &&
             static_cast<int32_t>(item.enqueuedAtMs - items_[best].enqueuedAtMs) < 0)) {
            best = i;
        }
    }
    if (best == kQueueCapacity) {
        return false;
    }

    bundle = items_[best];
    slotIndex = best;
    return true;
}

void BundleQueue::markSent(size_t slotIndex, uint32_t nowMs) {
    if (slotIndex >= kQueueCapacity || !items_[slotIndex].occupied) {
        return;
    }
    QueuedBundle& item = items_[slotIndex];
    item.awaitingAck = true;
    item.retryAtMs = nowMs;
    if (item.retryCount < 5) {
        ++item.retryCount;
    }
}

void BundleQueue::defer(size_t slotIndex, uint32_t nowMs, uint32_t delayMs) {
    if (slotIndex >= kQueueCapacity || !items_[slotIndex].occupied) {
        return;
    }
    items_[slotIndex].awaitingAck = false;
    items_[slotIndex].retryAtMs = nowMs + delayMs;
}

bool BundleQueue::acknowledge(uint64_t bundleId) {
    for (size_t i = 0; i < kQueueCapacity; ++i) {
        if (items_[i].occupied && items_[i].packet.bundleId == bundleId) {
            items_[i] = QueuedBundle{};
            --size_;
            return true;
        }
    }
    return false;
}

size_t BundleQueue::size() const {
    return size_;
}

uint8_t BundleQueue::bufferPercent() const {
    return static_cast<uint8_t>((size_ * 100U) / kQueueCapacity);
}

}  // namespace aegis
