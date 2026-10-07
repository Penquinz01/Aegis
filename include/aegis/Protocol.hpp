#pragma once

#include "aegis/Config.hpp"

namespace aegis {

enum class FrameType : uint8_t {
    Beacon = 1,
    Bundle = 2,
    Ack = 3,
};

enum class Priority : uint8_t {
    Bulk = 0,
    Normal = 1,
    Critical = 2,
};

struct Packet {
    FrameType type = FrameType::Beacon;
    uint8_t originId = 0;
    uint8_t lastHopId = 0;
    uint8_t destinationId = 0;
    Priority priority = Priority::Normal;
    uint8_t hopLimit = 0;
    uint16_t sequence = 0;
    uint64_t bundleId = 0;
    uint32_t ttlSeconds = 0;
    uint32_t visitedMask = 0;
    uint16_t payloadLength = 0;
    uint8_t payload[kMaximumPayloadBytes]{};
};

constexpr size_t kPacketHeaderBytes = 29;
constexpr size_t kPacketTrailerBytes = 2;

bool encodePacket(const Packet& packet, uint8_t* output, size_t capacity, size_t& outputLength);
bool decodePacket(const uint8_t* input, size_t inputLength, Packet& packet);
uint32_t nodeMask(uint8_t nodeId);

}  // namespace aegis
