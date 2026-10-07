#pragma once

#include <cstddef>
#include <cstdint>

#ifndef AEGIS_NODE_ID
#define AEGIS_NODE_ID 1
#endif

namespace aegis {

constexpr uint8_t kNodeId = AEGIS_NODE_ID;
constexpr uint8_t kDestinationId = 5;
constexpr uint8_t kNodeCount = 5;
constexpr uint8_t kWifiChannel = 6;
constexpr uint32_t kHeartbeatIntervalMs = 2000;
constexpr uint32_t kNeighborTimeoutMs = 7000;
constexpr uint32_t kAckTimeoutMs = 1200;
constexpr uint32_t kBundleLifetimeSeconds = 3600;
constexpr uint8_t kInitialHopLimit = 8;
constexpr size_t kQueueCapacity = 8;
constexpr size_t kNeighborCapacity = 8;
constexpr size_t kRecentBundleCapacity = 16;
constexpr size_t kMaximumPayloadBytes = 200;
constexpr size_t kMaximumFrameBytes = 250;

static_assert(kNodeId >= 1 && kNodeId <= kNodeCount, "AEGIS_NODE_ID must be 1..5");
static_assert(kMaximumPayloadBytes + 31 <= kMaximumFrameBytes,
              "Bundle frame must fit ESP-NOW v1 frame capacity");

}  // namespace aegis
