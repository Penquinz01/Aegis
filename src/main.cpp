#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "aegis/BundleQueue.hpp"
#include "aegis/Config.hpp"
#include "aegis/EspNowTransport.hpp"
#include "aegis/Protocol.hpp"
#include "aegis/Routing.hpp"

namespace {

using namespace aegis;

struct Counters {
    uint32_t received = 0;
    uint32_t transmitted = 0;
    uint32_t malformed = 0;
    uint32_t retries = 0;
    uint32_t delivered = 0;
    uint32_t dropped = 0;
};

EspNowTransport transport;
BundleQueue bundleQueue;
RoutingTable routingTable;
Counters counters;
uint64_t recentBundleIds[kRecentBundleCapacity]{};
size_t recentBundleCursor = 0;
uint16_t sequence = 0;
uint32_t lastBeaconMs = 0;
uint32_t lastStatusMs = 0;
char serialLine[256]{};
size_t serialLineLength = 0;

bool wasSeen(uint64_t bundleId) {
    for (uint64_t recentId : recentBundleIds) {
        if (recentId == bundleId && bundleId != 0) {
            return true;
        }
    }
    return false;
}

void remember(uint64_t bundleId) {
    recentBundleIds[recentBundleCursor] = bundleId;
    recentBundleCursor = (recentBundleCursor + 1U) % kRecentBundleCapacity;
}

void forget(uint64_t bundleId) {
    for (uint64_t& recentId : recentBundleIds) {
        if (recentId == bundleId) {
            recentId = 0;
        }
    }
}

bool transmit(const Packet& packet, bool broadcast, uint8_t peerNodeId = 0) {
    uint8_t frame[kMaximumFrameBytes];
    size_t frameLength = 0;
    if (!encodePacket(packet, frame, sizeof(frame), frameLength)) {
        return false;
    }
    const bool sent = broadcast ? transport.sendBroadcast(frame, frameLength)
                                : transport.sendToNode(peerNodeId, frame, frameLength);
    if (sent) {
        ++counters.transmitted;
    }
    return sent;
}

void sendAck(uint8_t destinationId, uint64_t bundleId) {
    Packet ack;
    ack.type = FrameType::Ack;
    ack.originId = kNodeId;
    ack.lastHopId = kNodeId;
    ack.destinationId = destinationId;
    ack.sequence = ++sequence;
    ack.bundleId = bundleId;
    if (!transmit(ack, false, destinationId)) {
        Serial.printf("event=ack_send_failed node=%u peer=%u bundle=%llu\n",
                      kNodeId, destinationId, static_cast<unsigned long long>(bundleId));
    }
}

void sendBeacon(uint32_t nowMs) {
    Packet beacon;
    beacon.type = FrameType::Beacon;
    beacon.originId = kNodeId;
    beacon.lastHopId = kNodeId;
    beacon.destinationId = kDestinationId;
    beacon.sequence = ++sequence;
    beacon.payloadLength = 3;
    beacon.payload[0] = routingTable.costTo(kDestinationId, nowMs);
    beacon.payload[1] = bundleQueue.bufferPercent();
    beacon.payload[2] = static_cast<uint8_t>(kQueueCapacity - bundleQueue.size());
    if (!transmit(beacon, true)) {
        Serial.printf("event=beacon_send_failed node=%u\n", kNodeId);
    }
}

void processBeacon(const Packet& packet, const ReceivedFrame& frame, uint32_t nowMs) {
    if (packet.payloadLength != 3 || packet.originId != packet.lastHopId ||
        packet.destinationId != kDestinationId) {
        ++counters.malformed;
        return;
    }
    if (!transport.registerPeer(packet.lastHopId, frame.mac)) {
        Serial.printf("event=peer_register_failed node=%u peer=%u\n", kNodeId, packet.lastHopId);
        return;
    }
    routingTable.update(packet.lastHopId, packet.destinationId, packet.payload[0],
                        packet.payload[1], frame.rssi, nowMs);
}

void processAck(const Packet& packet) {
    if (packet.destinationId != kNodeId || packet.payloadLength != 0) {
        ++counters.malformed;
        return;
    }
    if (bundleQueue.acknowledge(packet.bundleId)) {
        Serial.printf("event=bundle_acked node=%u bundle=%llu by=%u queue=%u\n",
                      kNodeId, static_cast<unsigned long long>(packet.bundleId),
                      packet.lastHopId, static_cast<unsigned>(bundleQueue.size()));
    }
}

void processBundle(const Packet& packet, uint32_t nowMs) {
    if (packet.payloadLength == 0 || packet.ttlSeconds == 0) {
        ++counters.dropped;
        return;
    }
    if (wasSeen(packet.bundleId)) {
        sendAck(packet.lastHopId, packet.bundleId);
        return;
    }

    if (packet.destinationId == kNodeId) {
        remember(packet.bundleId);
        ++counters.delivered;
        Serial.printf("event=bundle_delivered node=%u origin=%u bundle=%llu priority=%u payload=",
                      kNodeId, packet.originId, static_cast<unsigned long long>(packet.bundleId),
                      static_cast<unsigned>(packet.priority));
        Serial.write(packet.payload, packet.payloadLength);
        Serial.println();
        sendAck(packet.lastHopId, packet.bundleId);
        return;
    }

    if (packet.hopLimit == 0 || (packet.visitedMask & nodeMask(kNodeId)) != 0U) {
        ++counters.dropped;
        Serial.printf("event=bundle_route_rejected node=%u bundle=%llu reason=loop_or_hop_limit\n",
                      kNodeId, static_cast<unsigned long long>(packet.bundleId));
        return;
    }

    bool evictedLowerPriority = false;
    uint64_t evictedBundleId = 0;
    if (!bundleQueue.enqueue(packet, nowMs, evictedLowerPriority, evictedBundleId)) {
        Serial.printf("event=bundle_queue_full node=%u bundle=%llu priority=%u\n",
                      kNodeId, static_cast<unsigned long long>(packet.bundleId),
                      static_cast<unsigned>(packet.priority));
        return;
    }
    if (evictedLowerPriority) {
        ++counters.dropped;
        forget(evictedBundleId);
        Serial.printf("event=low_priority_evicted node=%u queue=%u\n",
                      kNodeId, static_cast<unsigned>(bundleQueue.size()));
    }
    remember(packet.bundleId);
    sendAck(packet.lastHopId, packet.bundleId);
    Serial.printf("event=bundle_buffered node=%u bundle=%llu queue=%u\n",
                  kNodeId, static_cast<unsigned long long>(packet.bundleId),
                  static_cast<unsigned>(bundleQueue.size()));
}

void processFrames(uint32_t nowMs) {
    ReceivedFrame frame;
    while (transport.receive(frame)) {
        Packet packet;
        if (!decodePacket(frame.data, frame.length, packet)) {
            ++counters.malformed;
            continue;
        }
        ++counters.received;
        if (packet.lastHopId == kNodeId) {
            continue;
        }

        switch (packet.type) {
            case FrameType::Beacon:
                processBeacon(packet, frame, nowMs);
                break;
            case FrameType::Bundle:
                transport.registerPeer(packet.lastHopId, frame.mac);
                processBundle(packet, nowMs);
                break;
            case FrameType::Ack:
                transport.registerPeer(packet.lastHopId, frame.mac);
                processAck(packet);
                break;
        }
    }
}

void processForwarding(uint32_t nowMs) {
    QueuedBundle queued;
    size_t slotIndex = 0;
    if (!bundleQueue.nextDue(nowMs, queued, slotIndex)) {
        return;
    }

    const uint32_t elapsedMs = static_cast<uint32_t>(nowMs - queued.enqueuedAtMs);
    const uint32_t ttlMs = queued.packet.ttlSeconds * 1000UL;
    if (elapsedMs >= ttlMs || queued.packet.hopLimit == 0) {
        ++counters.dropped;
        forget(queued.packet.bundleId);
        bundleQueue.acknowledge(queued.packet.bundleId);
        Serial.printf("event=bundle_expired node=%u bundle=%llu\n",
                      kNodeId, static_cast<unsigned long long>(queued.packet.bundleId));
        return;
    }

    const uint8_t nextHop = routingTable.nextHop(queued.packet.destinationId,
                                                  queued.packet.visitedMask | nodeMask(kNodeId), nowMs);
    if (nextHop == 0) {
        bundleQueue.defer(slotIndex, nowMs, 1000);
        return;
    }

    Packet outgoing = queued.packet;
    outgoing.lastHopId = kNodeId;
    outgoing.hopLimit = static_cast<uint8_t>(outgoing.hopLimit - 1U);
    outgoing.visitedMask |= nodeMask(kNodeId);
    outgoing.sequence = ++sequence;
    outgoing.ttlSeconds -= elapsedMs / 1000U;
    if (transmit(outgoing, false, nextHop)) {
        bundleQueue.markSent(slotIndex, nowMs);
        if (queued.retryCount > 0) {
            ++counters.retries;
        }
        Serial.printf("event=bundle_forwarded node=%u next=%u bundle=%llu queue=%u\n",
                      kNodeId, nextHop, static_cast<unsigned long long>(outgoing.bundleId),
                      static_cast<unsigned>(bundleQueue.size()));
    } else {
        bundleQueue.defer(slotIndex, nowMs, 500);
    }
}

void printStatus(uint32_t nowMs) {
    Serial.printf("event=status node=%u target=%u queue=%u buffer_pct=%u cost=%u rx=%lu tx=%lu malformed=%lu retries=%lu delivered=%lu dropped=%lu free_heap=%lu\n",
                  kNodeId, kDestinationId, static_cast<unsigned>(bundleQueue.size()),
                  bundleQueue.bufferPercent(), routingTable.costTo(kDestinationId, nowMs),
                  static_cast<unsigned long>(counters.received),
                  static_cast<unsigned long>(counters.transmitted),
                  static_cast<unsigned long>(counters.malformed),
                  static_cast<unsigned long>(counters.retries),
                  static_cast<unsigned long>(counters.delivered),
                  static_cast<unsigned long>(counters.dropped),
                  static_cast<unsigned long>(ESP.getFreeHeap()));
    for (uint8_t id = 1; id <= kNodeCount; ++id) {
        const Neighbor* neighbor = routingTable.neighbor(id, nowMs);
        if (neighbor != nullptr) {
            Serial.printf("event=neighbor node=%u peer=%u cost=%u rssi=%d buffer_pct=%u\n",
                          kNodeId, id, neighbor->costToDestination, neighbor->rssi,
                          neighbor->bufferPercent);
        }
    }
}

bool parsePriority(const char* text, Priority& priority) {
    if (std::strcmp(text, "critical") == 0) {
        priority = Priority::Critical;
        return true;
    }
    if (std::strcmp(text, "normal") == 0) {
        priority = Priority::Normal;
        return true;
    }
    if (std::strcmp(text, "bulk") == 0) {
        priority = Priority::Bulk;
        return true;
    }
    return false;
}

void handleSendCommand(char* command) {
    char* destinationText = command + 5;
    char* priorityText = std::strchr(destinationText, ' ');
    if (priorityText == nullptr) {
        Serial.println(F("usage: send <node 1..5> <critical|normal|bulk> <message>"));
        return;
    }
    *priorityText++ = '\0';
    char* payload = std::strchr(priorityText, ' ');
    if (payload == nullptr) {
        Serial.println(F("usage: send <node 1..5> <critical|normal|bulk> <message>"));
        return;
    }
    *payload++ = '\0';

    char* end = nullptr;
    const long destination = std::strtol(destinationText, &end, 10);
    Priority priority;
    if (end == destinationText || *end != '\0' || destination < 1 || destination > kNodeCount ||
        destination == kNodeId || !parsePriority(priorityText, priority) || *payload == '\0') {
        Serial.println(F("invalid command; use: send <other node 1..5> <critical|normal|bulk> <message>"));
        return;
    }

    const size_t payloadLength = std::strlen(payload);
    if (payloadLength > kMaximumPayloadBytes) {
        Serial.printf("error=payload_too_large max=%u\n", static_cast<unsigned>(kMaximumPayloadBytes));
        return;
    }

    Packet bundle;
    bundle.type = FrameType::Bundle;
    bundle.originId = kNodeId;
    bundle.lastHopId = kNodeId;
    bundle.destinationId = static_cast<uint8_t>(destination);
    bundle.priority = priority;
    bundle.hopLimit = kInitialHopLimit;
    bundle.sequence = ++sequence;
    bundle.bundleId = (static_cast<uint64_t>(kNodeId) << 56U) |
                      (static_cast<uint64_t>(esp_random()) << 24U) | bundle.sequence;
    bundle.ttlSeconds = kBundleLifetimeSeconds;
    bundle.visitedMask = nodeMask(kNodeId);
    bundle.payloadLength = static_cast<uint16_t>(payloadLength);
    std::memcpy(bundle.payload, payload, payloadLength);

    bool evictedLowerPriority = false;
    uint64_t evictedBundleId = 0;
    if (!bundleQueue.enqueue(bundle, millis(), evictedLowerPriority, evictedBundleId)) {
        Serial.println(F("error=queue_full; critical bundles may replace lower priority bundles"));
        return;
    }
    if (evictedLowerPriority) {
        ++counters.dropped;
        forget(evictedBundleId);
        Serial.printf("event=low_priority_evicted node=%u queue=%u\n",
                      kNodeId, static_cast<unsigned>(bundleQueue.size()));
    }
    Serial.printf("event=bundle_created node=%u destination=%u bundle=%llu priority=%u\n",
                  kNodeId, bundle.destinationId, static_cast<unsigned long long>(bundle.bundleId),
                  static_cast<unsigned>(priority));
}

void processSerial(uint32_t nowMs) {
    while (Serial.available() > 0) {
        const char value = static_cast<char>(Serial.read());
        if (value == '\r') {
            continue;
        }
        if (value == '\n') {
            serialLine[serialLineLength] = '\0';
            if (std::strncmp(serialLine, "send ", 5) == 0) {
                handleSendCommand(serialLine);
            } else if (std::strcmp(serialLine, "status") == 0) {
                printStatus(nowMs);
            } else if (std::strcmp(serialLine, "help") == 0) {
                Serial.println(F("commands: status | send <node 1..5> <critical|normal|bulk> <message>"));
            } else if (serialLineLength > 0) {
                Serial.println(F("unknown command; type help"));
            }
            serialLineLength = 0;
            continue;
        }
        if (serialLineLength + 1 < sizeof(serialLine)) {
            serialLine[serialLineLength++] = value;
        }
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    const uint32_t startMs = millis();
    while (!Serial && static_cast<uint32_t>(millis() - startMs) < 1500U) {
        delay(1);
    }

    Serial.printf("event=boot project=Aegis node=%u destination=%u channel=%u framework=Arduino\n",
                  aegis::kNodeId, aegis::kDestinationId, aegis::kWifiChannel);
    if (!transport.begin(aegis::kWifiChannel)) {
        Serial.println(F("event=boot_error reason=espnow_initialization_failed"));
        return;
    }
    Serial.printf("event=ready node=%u mac=%s channel=%u\n", aegis::kNodeId,
                  WiFi.macAddress().c_str(), aegis::kWifiChannel);
    Serial.println(F("commands: status | send <node 1..5> <critical|normal|bulk> <message>"));
}

void loop() {
    const uint32_t nowMs = millis();
    processSerial(nowMs);
    processFrames(nowMs);

    if (static_cast<uint32_t>(nowMs - lastBeaconMs) >= aegis::kHeartbeatIntervalMs) {
        lastBeaconMs = nowMs;
        sendBeacon(nowMs);
    }

    processForwarding(nowMs);

    if (static_cast<uint32_t>(nowMs - lastStatusMs) >= 10000U) {
        lastStatusMs = nowMs;
        printStatus(nowMs);
    }
    delay(1);
}
