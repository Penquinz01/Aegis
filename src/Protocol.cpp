#include "aegis/Protocol.hpp"

#include <cstring>

namespace aegis {
namespace {

constexpr uint8_t kMagic0 = 0xAE;
constexpr uint8_t kMagic1 = 0x61;
constexpr uint8_t kProtocolVersion = 1;

void writeU16(uint8_t* output, size_t& offset, uint16_t value) {
    output[offset++] = static_cast<uint8_t>(value & 0xFFU);
    output[offset++] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
}

void writeU32(uint8_t* output, size_t& offset, uint32_t value) {
    for (uint8_t i = 0; i < 4; ++i) {
        output[offset++] = static_cast<uint8_t>((value >> (8U * i)) & 0xFFU);
    }
}

void writeU64(uint8_t* output, size_t& offset, uint64_t value) {
    for (uint8_t i = 0; i < 8; ++i) {
        output[offset++] = static_cast<uint8_t>((value >> (8U * i)) & 0xFFU);
    }
}

uint16_t readU16(const uint8_t* input, size_t& offset) {
    const uint16_t value = static_cast<uint16_t>(input[offset]) |
                           static_cast<uint16_t>(input[offset + 1] << 8U);
    offset += 2;
    return value;
}

uint32_t readU32(const uint8_t* input, size_t& offset) {
    uint32_t value = 0;
    for (uint8_t i = 0; i < 4; ++i) {
        value |= static_cast<uint32_t>(input[offset++]) << (8U * i);
    }
    return value;
}

uint64_t readU64(const uint8_t* input, size_t& offset) {
    uint64_t value = 0;
    for (uint8_t i = 0; i < 8; ++i) {
        value |= static_cast<uint64_t>(input[offset++]) << (8U * i);
    }
    return value;
}

uint16_t crc16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFFU;
    for (size_t i = 0; i < length; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8U;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000U) != 0U
                      ? static_cast<uint16_t>((crc << 1U) ^ 0x1021U)
                      : static_cast<uint16_t>(crc << 1U);
        }
    }
    return crc;
}

bool validNode(uint8_t nodeId) {
    return nodeId >= 1 && nodeId <= kNodeCount;
}

}  // namespace

uint32_t nodeMask(uint8_t nodeId) {
    return validNode(nodeId) ? (1UL << (nodeId - 1U)) : 0U;
}

bool encodePacket(const Packet& packet, uint8_t* output, size_t capacity, size_t& outputLength) {
    outputLength = 0;
    if (output == nullptr || packet.payloadLength > kMaximumPayloadBytes ||
        capacity < kPacketHeaderBytes + packet.payloadLength + kPacketTrailerBytes ||
        !validNode(packet.originId) || !validNode(packet.lastHopId) ||
        !validNode(packet.destinationId)) {
        return false;
    }

    size_t offset = 0;
    output[offset++] = kMagic0;
    output[offset++] = kMagic1;
    output[offset++] = kProtocolVersion;
    output[offset++] = static_cast<uint8_t>(packet.type);
    output[offset++] = packet.originId;
    output[offset++] = packet.lastHopId;
    output[offset++] = packet.destinationId;
    output[offset++] = static_cast<uint8_t>(packet.priority);
    output[offset++] = packet.hopLimit;
    writeU16(output, offset, packet.sequence);
    writeU64(output, offset, packet.bundleId);
    writeU32(output, offset, packet.ttlSeconds);
    writeU32(output, offset, packet.visitedMask);
    writeU16(output, offset, packet.payloadLength);
    if (packet.payloadLength > 0) {
        std::memcpy(output + offset, packet.payload, packet.payloadLength);
        offset += packet.payloadLength;
    }

    const uint16_t crc = crc16(output, offset);
    writeU16(output, offset, crc);
    outputLength = offset;
    return true;
}

bool decodePacket(const uint8_t* input, size_t inputLength, Packet& packet) {
    if (input == nullptr || inputLength < kPacketHeaderBytes + kPacketTrailerBytes ||
        inputLength > kMaximumFrameBytes || input[0] != kMagic0 || input[1] != kMagic1 ||
        input[2] != kProtocolVersion) {
        return false;
    }

    size_t offset = 3;
    const uint8_t type = input[offset++];
    if (type < static_cast<uint8_t>(FrameType::Beacon) ||
        type > static_cast<uint8_t>(FrameType::Ack)) {
        return false;
    }

    Packet decoded;
    decoded.type = static_cast<FrameType>(type);
    decoded.originId = input[offset++];
    decoded.lastHopId = input[offset++];
    decoded.destinationId = input[offset++];
    const uint8_t priority = input[offset++];
    decoded.hopLimit = input[offset++];
    decoded.sequence = readU16(input, offset);
    decoded.bundleId = readU64(input, offset);
    decoded.ttlSeconds = readU32(input, offset);
    decoded.visitedMask = readU32(input, offset);
    decoded.payloadLength = readU16(input, offset);

    if (!validNode(decoded.originId) || !validNode(decoded.lastHopId) ||
        !validNode(decoded.destinationId) || priority > static_cast<uint8_t>(Priority::Critical) ||
        decoded.payloadLength > kMaximumPayloadBytes ||
        inputLength != kPacketHeaderBytes + decoded.payloadLength + kPacketTrailerBytes) {
        return false;
    }

    const size_t crcOffset = inputLength - kPacketTrailerBytes;
    size_t crcReadOffset = crcOffset;
    const uint16_t receivedCrc = readU16(input, crcReadOffset);
    if (crc16(input, crcOffset) != receivedCrc) {
        return false;
    }

    decoded.priority = static_cast<Priority>(priority);
    if (decoded.payloadLength > 0) {
        std::memcpy(decoded.payload, input + offset, decoded.payloadLength);
    }

    packet = decoded;
    return true;
}

}  // namespace aegis
