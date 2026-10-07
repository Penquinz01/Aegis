#include "aegis/EspNowTransport.hpp"

#include <cstring>

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace aegis {

void* EspNowTransport::receiveQueue_ = nullptr;

bool EspNowTransport::begin(uint8_t channel) {
    QueueHandle_t queue = xQueueCreate(8, sizeof(ReceivedFrame));
    if (queue == nullptr) {
        return false;
    }
    receiveQueue_ = queue;

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    const uint32_t startMs = millis();
    while (!WiFi.STA.started() && static_cast<uint32_t>(millis() - startMs) < 2000U) {
        delay(1);
    }
    if (!WiFi.STA.started() || esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE) != ESP_OK ||
        esp_now_init() != ESP_OK) {
        vQueueDelete(queue);
        receiveQueue_ = nullptr;
        return false;
    }

    esp_now_peer_info_t broadcastPeer{};
    std::memcpy(broadcastPeer.peer_addr, broadcastMac_, sizeof(broadcastMac_));
    broadcastPeer.channel = channel;
    broadcastPeer.ifidx = WIFI_IF_STA;
    broadcastPeer.encrypt = false;
    if (esp_now_add_peer(&broadcastPeer) != ESP_OK ||
        esp_now_register_recv_cb(&EspNowTransport::onReceive) != ESP_OK) {
        esp_now_deinit();
        vQueueDelete(queue);
        receiveQueue_ = nullptr;
        return false;
    }
    return true;
}

bool EspNowTransport::receive(ReceivedFrame& frame) {
    return receiveQueue_ != nullptr &&
           xQueueReceive(static_cast<QueueHandle_t>(receiveQueue_), &frame, 0) == pdTRUE;
}

bool EspNowTransport::sendBroadcast(const uint8_t* data, size_t length) {
    if (data == nullptr || length == 0 || length > kMaximumFrameBytes) {
        return false;
    }
    return esp_now_send(broadcastMac_, data, length) == ESP_OK;
}

bool EspNowTransport::sendToNode(uint8_t nodeId, const uint8_t* data, size_t length) {
    uint8_t mac[6];
    if (data == nullptr || length == 0 || length > kMaximumFrameBytes ||
        !macForNode(nodeId, mac)) {
        return false;
    }
    return esp_now_send(mac, data, length) == ESP_OK;
}

bool EspNowTransport::registerPeer(uint8_t nodeId, const uint8_t mac[6]) {
    if (nodeId == 0 || nodeId > kNodeCount || nodeId == kNodeId || mac == nullptr) {
        return false;
    }

    size_t slot = kNeighborCapacity;
    for (size_t i = 0; i < kNeighborCapacity; ++i) {
        if (peers_[i].occupied && peers_[i].nodeId == nodeId) {
            slot = i;
            break;
        }
        if (!peers_[i].occupied && slot == kNeighborCapacity) {
            slot = i;
        }
    }
    if (slot == kNeighborCapacity) {
        return false;
    }

    if (!esp_now_is_peer_exist(mac) && !addMacAsPeer(mac)) {
        return false;
    }
    peers_[slot].nodeId = nodeId;
    std::memcpy(peers_[slot].mac, mac, 6);
    peers_[slot].occupied = true;
    return true;
}

bool EspNowTransport::macForNode(uint8_t nodeId, uint8_t mac[6]) const {
    if (mac == nullptr) {
        return false;
    }
    for (const PeerEntry& peer : peers_) {
        if (peer.occupied && peer.nodeId == nodeId) {
            std::memcpy(mac, peer.mac, 6);
            return true;
        }
    }
    return false;
}

bool EspNowTransport::addMacAsPeer(const uint8_t mac[6]) {
    esp_now_peer_info_t peer{};
    std::memcpy(peer.peer_addr, mac, 6);
    peer.channel = kWifiChannel;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    const esp_err_t result = esp_now_add_peer(&peer);
    return result == ESP_OK || result == ESP_ERR_ESPNOW_EXIST;
}

void EspNowTransport::onReceive(const esp_now_recv_info_t* info, const uint8_t* data, int length) {
    if (info == nullptr || info->src_addr == nullptr || data == nullptr || length <= 0 ||
        length > static_cast<int>(kMaximumFrameBytes) || receiveQueue_ == nullptr) {
        return;
    }

    ReceivedFrame frame;
    std::memcpy(frame.mac, info->src_addr, sizeof(frame.mac));
    frame.length = static_cast<uint16_t>(length);
    std::memcpy(frame.data, data, static_cast<size_t>(length));
    if (info->rx_ctrl != nullptr) {
        frame.rssi = info->rx_ctrl->rssi;
    }
    xQueueSend(static_cast<QueueHandle_t>(receiveQueue_), &frame, 0);
}

}  // namespace aegis
