#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

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
Counters counters;
uint64_t recentBundleIds[kRecentBundleCapacity]{};
size_t recentBundleCursor = 0;
uint16_t sequence = 0;
uint8_t beaconDestinationId = 1;
char serialLine[256]{};
size_t serialLineLength = 0;

enum class RouteOperation : uint8_t { Update, Cost, NextHop, NextHopTinyMl };

struct RouteMessage {
    RouteOperation operation;
    uint8_t nodeId;
    uint8_t destinationId;
    uint8_t cost;
    uint8_t bufferPercent;
    int8_t rssi;
    uint32_t nowMs;
    uint32_t visitedMask;
    uint16_t requestId;
    bool requireRelay;
};

struct RouteResult {
    uint16_t requestId;
    uint8_t value;
};

struct LogRecord {
    uint16_t length;
    uint8_t bytes[256];
};

QueueHandle_t routeQueue = nullptr;
QueueHandle_t routeResultQueue = nullptr;
QueueHandle_t logQueue = nullptr;
uint16_t routeRequestSequence = 0;
bool tinyMlEnabled = true;
int8_t simulatedRssi[kNodeCount + 1] = {-128, -128, -128, -128, -128, -128};

void logBytes(const uint8_t* bytes, size_t length) {
    if (logQueue == nullptr) {
        Serial.write(bytes, length);
        return;
    }
    LogRecord record{};
    record.length = static_cast<uint16_t>(length > sizeof(record.bytes) ? sizeof(record.bytes) : length);
    std::memcpy(record.bytes, bytes, record.length);
    xQueueSend(logQueue, &record, 0);
}

void logLine(const char* line) {
    LogRecord record{};
    const size_t length = std::strlen(line);
    const size_t copyLength = length < sizeof(record.bytes) - 1U ? length : sizeof(record.bytes) - 1U;
    std::memcpy(record.bytes, line, copyLength);
    record.bytes[copyLength] = '\n';
    record.length = static_cast<uint16_t>(copyLength + 1U);
    if (logQueue == nullptr) {
        Serial.write(record.bytes, record.length);
    } else {
        xQueueSend(logQueue, &record, 0);
    }
}

void logPrintf(const char* format, ...) {
    char line[256];
    va_list args;
    va_start(args, format);
    std::vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    logLine(line);
}

void loggerTask(void*) {
    logPrintf("event=task_start name=host_logger core=%d\n", xPortGetCoreID());
    LogRecord record;
    for (;;) {
        if (xQueueReceive(logQueue, &record, portMAX_DELAY) == pdTRUE) {
            Serial.write(record.bytes, record.length);
        }
    }
}

void routingTask(void*) {
    logPrintf("event=task_start name=router core=%d\n", xPortGetCoreID());
    RoutingTable routingTable;
    RouteMessage request{};
    for (;;) {
        if (xQueueReceive(routeQueue, &request, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (request.operation == RouteOperation::Update) {
            routingTable.update(request.nodeId, request.destinationId, request.cost,
                                request.bufferPercent, request.rssi, request.nowMs);
            continue;
        }
        RouteResult result{};
        result.requestId = request.requestId;
        if (request.operation == RouteOperation::Cost) {
            result.value = routingTable.costTo(request.destinationId, request.nowMs);
        } else if (request.operation == RouteOperation::NextHopTinyMl) {
            int16_t score = 0;
            result.value = routingTable.nextHopTinyMl(request.destinationId, request.visitedMask,
                                                      request.nowMs, score, request.requireRelay);
        } else {
            result.value = routingTable.nextHop(request.destinationId, request.visitedMask,
                                                request.nowMs, request.requireRelay);
        }
        xQueueSend(routeResultQueue, &result, 0);
    }
}

uint8_t requestRoute(RouteMessage request) {
    RouteResult result{};
    request.requestId = ++routeRequestSequence;
    if (routeQueue == nullptr || routeResultQueue == nullptr ||
        xQueueSend(routeQueue, &request, pdMS_TO_TICKS(10)) != pdTRUE) {
        return 0xFF;
    }
    const TickType_t startedAt = xTaskGetTickCount();
    while (static_cast<TickType_t>(xTaskGetTickCount() - startedAt) < pdMS_TO_TICKS(50)) {
        if (xQueueReceive(routeResultQueue, &result, pdMS_TO_TICKS(5)) == pdTRUE &&
            result.requestId == request.requestId) {
            return result.value;
        }
    }
    return 0xFF;
}

uint8_t routeCost(uint8_t destinationId, uint32_t nowMs) {
    return requestRoute(RouteMessage{RouteOperation::Cost, 0, destinationId, 0, 0, 0,
                                     nowMs, 0, 0, false});
}

uint8_t routeNextHop(uint8_t destinationId, uint32_t visitedMask, uint32_t nowMs,
                     bool requireRelay) {
    const RouteOperation operation = tinyMlEnabled ? RouteOperation::NextHopTinyMl
                                                    : RouteOperation::NextHop;
    RouteMessage request{operation, 0, destinationId, 0, 0, 0, nowMs, visitedMask, 0,
                         requireRelay};
    const uint8_t nextHop = requestRoute(request);
    return nextHop == 0xFF ? 0 : nextHop;
}

using ScheduledFunction = void (*)(uint32_t);

struct ScheduledJob {
    ScheduledFunction function;
    uint32_t intervalMs;
    uint32_t lastRunMs;
};

void processSerial(uint32_t nowMs);
void processFrames(uint32_t nowMs);
void processForwarding(uint32_t nowMs);
void sendBeacon(uint32_t nowMs);
void runStatus(uint32_t nowMs);

ScheduledJob scheduledJobs[] = {
    {processSerial, 1U, 0U},
    {processFrames, 1U, 0U},
    {sendBeacon, aegis::kBeaconIntervalMs, 0U},
    {processForwarding, 10U, 0U},
    {runStatus, 10000U, 0U},
};

void runScheduledJobs(uint32_t nowMs) {
    for (ScheduledJob& job : scheduledJobs) {
        if (static_cast<uint32_t>(nowMs - job.lastRunMs) >= job.intervalMs) {
            job.lastRunMs = nowMs;
            job.function(nowMs);
        }
    }
}

void radioBundleTask(void*) {
    logPrintf("event=task_start name=radio_bundle core=%d\n", xPortGetCoreID());
    const uint32_t startMs = millis();
    for (ScheduledJob& job : scheduledJobs) {
        job.lastRunMs = startMs;
    }
    const uint32_t beaconPhaseMs =
        (static_cast<uint32_t>(kNodeId - 1U) * kBeaconIntervalMs) / kNodeCount;
    scheduledJobs[2].lastRunMs -= beaconPhaseMs;
    for (;;) {
        runScheduledJobs(millis());
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

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
        logPrintf("event=ack_send_failed node=%u peer=%u bundle=%llu\n",
                      kNodeId, destinationId, static_cast<unsigned long long>(bundleId));
    }
}

void sendBeacon(uint32_t nowMs) {
    Packet beacon;
    beacon.type = FrameType::Beacon;
    beacon.originId = kNodeId;
    beacon.lastHopId = kNodeId;
    beacon.destinationId = beaconDestinationId;
    beaconDestinationId = beaconDestinationId >= kNodeCount
                              ? 1
                              : static_cast<uint8_t>(beaconDestinationId + 1U);
    beacon.sequence = ++sequence;
    beacon.payloadLength = 3;
    beacon.payload[0] = routeCost(beacon.destinationId, nowMs);
    beacon.payload[1] = bundleQueue.bufferPercent();
    beacon.payload[2] = static_cast<uint8_t>(kQueueCapacity - bundleQueue.size());
    if (!transmit(beacon, true)) {
        logPrintf("event=beacon_send_failed node=%u\n", kNodeId);
    }
}

void processBeacon(const Packet& packet, const ReceivedFrame& frame, uint32_t nowMs) {
    if (packet.payloadLength != 3 || packet.originId != packet.lastHopId) {
        ++counters.malformed;
        return;
    }
    if (!transport.registerPeer(packet.lastHopId, frame.mac)) {
        logPrintf("event=peer_register_failed node=%u peer=%u\n", kNodeId, packet.lastHopId);
        return;
    }
    const int8_t effectiveRssi = simulatedRssi[packet.lastHopId] == -128
                                     ? frame.rssi
                                     : simulatedRssi[packet.lastHopId];
    const RouteMessage update{RouteOperation::Update, packet.lastHopId, packet.destinationId,
                              packet.payload[0], packet.payload[1], effectiveRssi, nowMs, 0, 0,
                              false};
    if (routeQueue == nullptr || xQueueSend(routeQueue, &update, 0) != pdTRUE) {
        logLine("event=route_update_dropped");
        return;
    }
    logPrintf("event=neighbor node=%u peer=%u destination=%u rssi=%d cost=%u buffer_pct=%u\n",
              kNodeId, packet.lastHopId, packet.destinationId,
              static_cast<int>(effectiveRssi), packet.payload[0], packet.payload[1]);
}

void processAck(const Packet& packet) {
    if (packet.destinationId != kNodeId || packet.payloadLength != 0) {
        ++counters.malformed;
        return;
    }
    if (bundleQueue.acknowledge(packet.bundleId)) {
        logPrintf("event=bundle_acked node=%u bundle=%llu by=%u queue=%u\n",
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
        logPrintf("event=bundle_delivered node=%u origin=%u destination=%u bundle=%llu priority=%u payload=",
                      kNodeId, packet.originId, packet.destinationId,
                      static_cast<unsigned long long>(packet.bundleId),
                      static_cast<unsigned>(packet.priority));
        logBytes(packet.payload, packet.payloadLength);
        logLine("");
        sendAck(packet.lastHopId, packet.bundleId);
        return;
    }

    if (packet.hopLimit == 0 || (packet.visitedMask & nodeMask(kNodeId)) != 0U) {
        ++counters.dropped;
        logPrintf("event=bundle_route_rejected node=%u bundle=%llu reason=loop_or_hop_limit\n",
                      kNodeId, static_cast<unsigned long long>(packet.bundleId));
        return;
    }

    bool evictedLowerPriority = false;
    uint64_t evictedBundleId = 0;
    if (!bundleQueue.enqueue(packet, nowMs, evictedLowerPriority, evictedBundleId)) {
        logPrintf("event=bundle_queue_full node=%u bundle=%llu priority=%u\n",
                      kNodeId, static_cast<unsigned long long>(packet.bundleId),
                      static_cast<unsigned>(packet.priority));
        return;
    }
    if (evictedLowerPriority) {
        ++counters.dropped;
        forget(evictedBundleId);
        logPrintf("event=low_priority_evicted node=%u queue=%u\n",
                      kNodeId, static_cast<unsigned>(bundleQueue.size()));
    }
    remember(packet.bundleId);
    sendAck(packet.lastHopId, packet.bundleId);
    logPrintf("event=bundle_buffered node=%u destination=%u bundle=%llu queue=%u\n",
                  kNodeId, packet.destinationId, static_cast<unsigned long long>(packet.bundleId),
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
        logPrintf("event=bundle_expired node=%u bundle=%llu\n",
                      kNodeId, static_cast<unsigned long long>(queued.packet.bundleId));
        return;
    }

    const bool requireRelay = queued.packet.originId == kNodeId;
    const uint8_t nextHop = routeNextHop(queued.packet.destinationId,
                                         queued.packet.visitedMask | nodeMask(kNodeId), nowMs,
                                         requireRelay);
    if (nextHop == 0) {
        bundleQueue.defer(slotIndex, nowMs, 1000);
        logPrintf("event=bundle_waiting_route node=%u destination=%u bundle=%llu model=%s\n",
                  kNodeId, queued.packet.destinationId,
                  static_cast<unsigned long long>(queued.packet.bundleId),
                  tinyMlEnabled ? "tiny_rssi_first" : "rssi_first");
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
        logPrintf("event=bundle_forwarded node=%u next=%u destination=%u bundle=%llu queue=%u\n",
                      kNodeId, nextHop, outgoing.destinationId,
                      static_cast<unsigned long long>(outgoing.bundleId),
                      static_cast<unsigned>(bundleQueue.size()));
    } else {
        bundleQueue.defer(slotIndex, nowMs, 500);
        logPrintf("event=bundle_send_failed node=%u next=%u destination=%u bundle=%llu\n",
                  kNodeId, nextHop, outgoing.destinationId,
                  static_cast<unsigned long long>(outgoing.bundleId));
    }
}

void printStatus(uint32_t nowMs) {
    char routes[40]{};
    size_t routeLength = 0;
    for (uint8_t destination = 1; destination <= kNodeCount; ++destination) {
        const int written = std::snprintf(routes + routeLength, sizeof(routes) - routeLength,
                                          "%s%u:%u", destination == 1 ? "" : ",",
                                          static_cast<unsigned>(destination),
                                          static_cast<unsigned>(routeCost(destination, nowMs)));
        if (written < 0 || static_cast<size_t>(written) >= sizeof(routes) - routeLength) {
            break;
        }
        routeLength += static_cast<size_t>(written);
    }
    logPrintf("event=status node=%u default_target=%u queue=%u buffer_pct=%u default_cost=%u routes=%s model=%s rx=%lu tx=%lu malformed=%lu retries=%lu delivered=%lu dropped=%lu free_heap=%lu\n",
                  kNodeId, kDefaultDestinationId, static_cast<unsigned>(bundleQueue.size()),
                  bundleQueue.bufferPercent(), routeCost(kDefaultDestinationId, nowMs),
                  routes,
                  tinyMlEnabled ? "tiny_rssi_first" : "rssi_first",
                  static_cast<unsigned long>(counters.received),
                  static_cast<unsigned long>(counters.transmitted),
                  static_cast<unsigned long>(counters.malformed),
                  static_cast<unsigned long>(counters.retries),
                  static_cast<unsigned long>(counters.delivered),
                  static_cast<unsigned long>(counters.dropped),
                  static_cast<unsigned long>(ESP.getFreeHeap()));
}

void runStatus(uint32_t nowMs) {
    printStatus(nowMs);
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
        logLine("usage: send <node 1..5> <critical|normal|bulk> <message>");
        return;
    }
    *priorityText++ = '\0';
    char* payload = std::strchr(priorityText, ' ');
    if (payload == nullptr) {
        logLine("usage: send <node 1..5> <critical|normal|bulk> <message>");
        return;
    }
    *payload++ = '\0';

    char* end = nullptr;
    const long destination = std::strtol(destinationText, &end, 10);
    Priority priority;
    if (end == destinationText || *end != '\0' || destination < 1 || destination > kNodeCount ||
        destination == kNodeId || !parsePriority(priorityText, priority) || *payload == '\0') {
        logLine("invalid command; use: send <other node 1..5> <critical|normal|bulk> <message>");
        return;
    }

    const size_t payloadLength = std::strlen(payload);
    if (payloadLength > kMaximumPayloadBytes) {
        logPrintf("error=payload_too_large max=%u\n", static_cast<unsigned>(kMaximumPayloadBytes));
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
        logLine("error=queue_full; critical bundles may replace lower priority bundles");
        return;
    }
    if (evictedLowerPriority) {
        ++counters.dropped;
        forget(evictedBundleId);
        logPrintf("event=low_priority_evicted node=%u queue=%u\n",
                      kNodeId, static_cast<unsigned>(bundleQueue.size()));
    }
    logPrintf("event=bundle_created node=%u destination=%u bundle=%llu priority=%u\n",
                  kNodeId, bundle.destinationId, static_cast<unsigned long long>(bundle.bundleId),
                  static_cast<unsigned>(priority));
}

void handleSimulationCommand(char* command) {
    char* peerText = command + 9;
    char* valueText = std::strchr(peerText, ' ');
    if (valueText == nullptr) {
        logLine("usage: sim rssi <peer 1..5> <-110..-20|auto>");
        return;
    }
    *valueText++ = '\0';
    char* end = nullptr;
    const long peer = std::strtol(peerText, &end, 10);
    if (end == peerText || *end != '\0' || peer < 1 || peer > kNodeCount || peer == kNodeId) {
        logLine("error=invalid_peer usage=sim_rssi");
        return;
    }
    if (std::strcmp(valueText, "auto") == 0) {
        simulatedRssi[peer] = -128;
        logPrintf("event=sim_rssi node=%u peer=%ld mode=auto\n", kNodeId, peer);
        return;
    }
    end = nullptr;
    const long value = std::strtol(valueText, &end, 10);
    if (end == valueText || *end != '\0' || value < -110 || value > -20) {
        logLine("error=invalid_rssi range=-110..-20");
        return;
    }
    simulatedRssi[peer] = static_cast<int8_t>(value);
    logPrintf("event=sim_rssi node=%u peer=%ld value=%ld\n", kNodeId, peer, value);
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
            } else if (std::strcmp(serialLine, "model on") == 0) {
                tinyMlEnabled = true;
                logLine("event=model mode=tiny_rssi_first");
            } else if (std::strcmp(serialLine, "model off") == 0) {
                tinyMlEnabled = false;
                logLine("event=model mode=baseline");
            } else if (std::strncmp(serialLine, "sim rssi ", 9) == 0) {
                handleSimulationCommand(serialLine);
            } else if (std::strcmp(serialLine, "help") == 0) {
                logLine("commands: status | model on|off | sim rssi <peer 1..5> <-110..-20|auto> | send <node 1..5> <critical|normal|bulk> <message>");
            } else if (serialLineLength > 0) {
                logLine("unknown command; type help");
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

    Serial.printf("event=boot project=Aegis node=%u default_destination=%u channel=%u framework=Arduino\n",
                  aegis::kNodeId, aegis::kDefaultDestinationId, aegis::kWifiChannel);
    if (!transport.begin(aegis::kWifiChannel)) {
        Serial.println(F("event=boot_error reason=espnow_initialization_failed"));
        return;
    }
    Serial.printf("event=ready node=%u mac=%s channel=%u\n", aegis::kNodeId,
                  WiFi.macAddress().c_str(), aegis::kWifiChannel);
    Serial.println(F("commands: status | model on|off | sim rssi <peer 1..5> <-110..-20|auto> | send <node 1..5> <critical|normal|bulk> <message>"));

    logQueue = xQueueCreate(24, sizeof(LogRecord));
    routeQueue = xQueueCreate(16, sizeof(RouteMessage));
    routeResultQueue = xQueueCreate(4, sizeof(RouteResult));
    if (logQueue == nullptr || routeQueue == nullptr || routeResultQueue == nullptr) {
        Serial.println(F("event=boot_error reason=freertos_queue_setup_failed"));
        return;
    }

    const BaseType_t loggerCreated = xTaskCreatePinnedToCore(
        loggerTask, "aegis_host_logger", 3072, nullptr, 1, nullptr, 1);
    const BaseType_t routerCreated = xTaskCreatePinnedToCore(
        routingTask, "aegis_router", 4096, nullptr, 2, nullptr, 1);
    const BaseType_t radioCreated = xTaskCreatePinnedToCore(
        radioBundleTask, "aegis_radio_bundle", 6144, nullptr, 3, nullptr, 0);
    if (loggerCreated != pdPASS || routerCreated != pdPASS || radioCreated != pdPASS) {
        Serial.println(F("event=boot_error reason=freertos_task_setup_failed"));
        return;
    }
}

void loop() {
    vTaskDelay(portMAX_DELAY);
}
