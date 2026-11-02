#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include "config.h"
#include "packet_protocol.h"

struct ReceivedMessage {
    SpotterPacket packet;
    float rssi;
    float snr;
    uint32_t receivedTime;
};

class RadioManager {
public:
    RadioManager();
    bool begin();
    
    // Transmission
    bool sendBroadcastSOS(int32_t lat, int32_t lon, int16_t alt, uint8_t sats, uint8_t bat);
    bool sendPresetMessage(uint8_t presetIndex, int32_t lat, int32_t lon, int16_t alt, uint8_t sats, uint8_t bat);
    bool sendAck(uint8_t targetNode, uint8_t seqNum);
    
    // Reception & Polling
    bool checkReceived(ReceivedMessage& outMsg);
    void startListening();

    // Delivery Confirmation State
    bool isWaitingForAck();
    bool checkAckReceived(uint8_t& outAckNode, float& outRssi);
    void clearAckState();

    uint8_t getNextSequenceNum();

private:
    SX1278 _radio;
    uint8_t _seqNum;

    // Delivery tracking
    bool     _waitingForAck;
    uint8_t  _pendingSeqNum;
    uint32_t _txTimestamp;
    bool     _ackReceived;
    uint8_t  _ackFromNode;
    float    _ackRssi;

    bool transmitRaw(SpotterPacket& pkt);
};
