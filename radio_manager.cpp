#include "radio_manager.h"

// Hardware interrupt flag for DIO0
static volatile bool s_dio0Triggered = false;

#if defined(ESP32) || defined(ESP8266)
static void IRAM_ATTR onDio0Interrupt() {
    s_dio0Triggered = true;
}
#else
static void onDio0Interrupt() {
    s_dio0Triggered = true;
}
#endif

RadioManager::RadioManager()
    : _radio(new Module(LORA_NSS, LORA_DIO0, LORA_RST)),
      _seqNum(1),
      _waitingForAck(false),
      _pendingSeqNum(0),
      _txTimestamp(0),
      _ackReceived(false),
      _ackFromNode(0),
      _ackRssi(0.0) {}

bool RadioManager::begin() {
    // Initialize standard ESP32 SPI pins for LoRa
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);

    // Initialize RadioLib SX1278
    int state = _radio.begin(
        LORA_FREQUENCY,
        LORA_BANDWIDTH,
        LORA_SPREAD_FACTOR,
        LORA_CODING_RATE,
        LORA_SYNC_WORD,
        LORA_OUTPUT_POWER,
        LORA_PREAMBLE_LEN
    );

    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LORA] Init failed with error code: %d\n", state);
        return false;
    }

    // Attach DIO0 interrupt and start non-blocking continuous reception
    _radio.setPacketReceivedAction(onDio0Interrupt);
    _radio.startReceive();
    Serial.println("[LORA] SX1278 initialized successfully on 433 MHz");
    return true;
}

void RadioManager::startListening() {
    _radio.startReceive();
}

uint8_t RadioManager::getNextSequenceNum() {
    if (++_seqNum == 0) _seqNum = 1;
    return _seqNum;
}

bool RadioManager::transmitRaw(SpotterPacket& pkt) {
    sealPacket(pkt);

    // Temporarily disable interrupt during transmission
    int state = _radio.transmit((uint8_t*)&pkt, SPOTTER_PACKET_SIZE);

    // Immediately resume listening for incoming packets & ACKs
    _radio.startReceive();

    return (state == RADIOLIB_ERR_NONE);
}

bool RadioManager::sendBroadcastSOS(int32_t lat, int32_t lon, int16_t alt, uint8_t sats, uint8_t bat) {
    SpotterPacket pkt;
    pkt.src_node    = CURRENT_NODE_ID;
    pkt.dst_node    = BROADCAST_NODE_ID; // Broadcast to ALL nodes
    pkt.seq_num     = getNextSequenceNum();
    pkt.msg_type    = MSG_EMERGENCY_SOS;
    pkt.satellites  = sats;
    pkt.battery_pct = bat;
    pkt.lat_scaled  = lat;
    pkt.lon_scaled  = lon;
    pkt.alt_meters  = alt;
    pkt.preset_code = 0x01; // Critical SOS code

    _waitingForAck = true;
    _pendingSeqNum = pkt.seq_num;
    _txTimestamp   = millis();
    _ackReceived   = false;

    return transmitRaw(pkt);
}

bool RadioManager::sendPresetMessage(uint8_t presetIndex, int32_t lat, int32_t lon, int16_t alt, uint8_t sats, uint8_t bat) {
    SpotterPacket pkt;
    pkt.src_node    = CURRENT_NODE_ID;
    pkt.dst_node    = BROADCAST_NODE_ID;
    pkt.seq_num     = getNextSequenceNum();
    pkt.msg_type    = MSG_STATUS_UPDATE;
    pkt.satellites  = sats;
    pkt.battery_pct = bat;
    pkt.lat_scaled  = lat;
    pkt.lon_scaled  = lon;
    pkt.alt_meters  = alt;
    pkt.preset_code = presetIndex;

    _waitingForAck = true;
    _pendingSeqNum = pkt.seq_num;
    _txTimestamp   = millis();
    _ackReceived   = false;

    return transmitRaw(pkt);
}

bool RadioManager::sendAck(uint8_t targetNode, uint8_t seqNum) {
    SpotterPacket ack;
    ack.src_node    = CURRENT_NODE_ID;
    ack.dst_node    = targetNode;
    ack.seq_num     = seqNum;
    ack.msg_type    = MSG_ACK_RECEIPT;
    ack.satellites  = 0;
    ack.battery_pct = 100;
    ack.lat_scaled  = 0;
    ack.lon_scaled  = 0;
    ack.alt_meters  = 0;
    ack.preset_code = 0;

    return transmitRaw(ack);
}

bool RadioManager::checkReceived(ReceivedMessage& outMsg) {
    if (!s_dio0Triggered) {
        return false;
    }
    s_dio0Triggered = false;

    SpotterPacket tempPkt;
    size_t len = _radio.getPacketLength();
    if (len != SPOTTER_PACKET_SIZE) {
        // Not our packet size, discard and resume listening
        _radio.startReceive();
        return false;
    }

    int state = _radio.readData((uint8_t*)&tempPkt, SPOTTER_PACKET_SIZE);
    _radio.startReceive(); // Return to listening mode immediately

    if (state != RADIOLIB_ERR_NONE || !validatePacket(tempPkt)) {
        return false; // Checksum or magic failed
    }

    // Ignore packets transmitted by ourselves
    if (tempPkt.src_node == CURRENT_NODE_ID) {
        return false;
    }

    float rssi = _radio.getRSSI();
    float snr  = _radio.getSNR();

    // Check if this incoming packet is an ACK meant for our pending message
    if (tempPkt.msg_type == MSG_ACK_RECEIPT &&
        (tempPkt.dst_node == CURRENT_NODE_ID || tempPkt.dst_node == BROADCAST_NODE_ID)) {
        if (_waitingForAck && tempPkt.seq_num == _pendingSeqNum) {
            _ackReceived   = true;
            _ackFromNode   = tempPkt.src_node;
            _ackRssi       = rssi;
            _waitingForAck = false;
        }
        return false; // Handled internally, no need to trigger full UI popup
    }

    // If we received an SOS or status update addressed to us or broadcast, auto-reply with ACK!
    if (tempPkt.msg_type == MSG_EMERGENCY_SOS || tempPkt.msg_type == MSG_STATUS_UPDATE) {
        // Brief random jitter (20-60ms) before ACK to prevent collision if multiple nodes reply
        delay(20 + (CURRENT_NODE_ID * 25));
        sendAck(tempPkt.src_node, tempPkt.seq_num);
    }

    outMsg.packet = tempPkt;
    outMsg.rssi = rssi;
    outMsg.snr = snr;
    outMsg.receivedTime = millis();
    return true;
}

bool RadioManager::isWaitingForAck() {
    if (_waitingForAck && (millis() - _txTimestamp > ACK_TIMEOUT_MS)) {
        _waitingForAck = false; // Timed out
    }
    return _waitingForAck;
}

bool RadioManager::checkAckReceived(uint8_t& outAckNode, float& outRssi) {
    if (_ackReceived) {
        outAckNode = _ackFromNode;
        outRssi    = _ackRssi;
        _ackReceived = false;
        return true;
    }
    return false;
}

void RadioManager::clearAckState() {
    _waitingForAck = false;
    _ackReceived = false;
}
