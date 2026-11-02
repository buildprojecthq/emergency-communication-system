#pragma once

#include <Arduino.h>

// Protocol constants
#define PROTOCOL_MAGIC        0xAA
#define PROTOCOL_NETWORK_ID   0x42

// Message Types
enum MessageType : uint8_t {
    MSG_EMERGENCY_SOS = 0x01,  // Priority distress alert (Panic or Medical)
    MSG_STATUS_UPDATE = 0x02,  // Preset message (Supplies, Evac, Safe)
    MSG_ACK_RECEIPT   = 0x03   // Invisible return confirmation of delivery
};

// Compact 24-byte Over-The-Air Binary Packet
// Keeps LoRa on-air transmission time under 85ms!
struct __attribute__((packed)) SpotterPacket {
    uint8_t  magic;          // Must be 0xAA
    uint8_t  network_id;     // Must be 0x42 (filters out noise/other networks)
    uint8_t  src_node;       // Sending Spotter ID (e.g., 0x01)
    uint8_t  dst_node;       // Destination ID (0xFF = Broadcast to ALL)
    uint8_t  seq_num;        // Rolling sequence counter (0 - 255)
    uint8_t  msg_type;       // MessageType enum
    uint8_t  satellites;     // GPS satellites locked
    uint8_t  battery_pct;    // Estimated battery % (0 - 100)
    int32_t  lat_scaled;     // Latitude * 1,000,000 (sub-meter accuracy)
    int32_t  lon_scaled;     // Longitude * 1,000,000
    int16_t  alt_meters;     // Altitude in meters
    uint8_t  preset_code;    // Index of the preset message
    uint16_t crc16;          // Checksum for radio transmission integrity
};

#define SPOTTER_PACKET_SIZE sizeof(SpotterPacket)

// ==============================================================================
// CRC16 Checksum Implementation (CCITT-FALSE: Polynomial 0x1021)
// ==============================================================================
inline uint16_t calculateCRC16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

// Helper to seal packet with CRC before transmitting
inline void sealPacket(SpotterPacket& pkt) {
    pkt.magic = PROTOCOL_MAGIC;
    pkt.network_id = PROTOCOL_NETWORK_ID;
    // Calculate CRC over all bytes except the trailing 2-byte crc16 field itself
    pkt.crc16 = calculateCRC16((const uint8_t*)&pkt, sizeof(SpotterPacket) - sizeof(uint16_t));
}

// Helper to validate packet integrity upon reception
inline bool validatePacket(const SpotterPacket& pkt) {
    if (pkt.magic != PROTOCOL_MAGIC || pkt.network_id != PROTOCOL_NETWORK_ID) {
        return false;
    }
    uint16_t expectedCRC = calculateCRC16((const uint8_t*)&pkt, sizeof(SpotterPacket) - sizeof(uint16_t));
    return (pkt.crc16 == expectedCRC);
}
