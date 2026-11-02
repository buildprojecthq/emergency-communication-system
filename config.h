#pragma once

#include <Arduino.h>

// ==============================================================================
// DEVICE IDENTITY CONFIGURATION
// Change CURRENT_NODE_ID to 0x02 before uploading to the second Spotter unit!
// ==============================================================================
#define CURRENT_NODE_ID       0x01      // Spotter 1 = 0x01 | Spotter 2 = 0x02
#define BROADCAST_NODE_ID     0xFF      // Reaches all listening Spotter nodes

// ==============================================================================
// HARDWARE PIN DEFINITIONS (ESP32 DevKit V1)
// ==============================================================================

// LoRa SX1278 (SPI Bus + Control Pins)
#define LORA_NSS              5         // Chip Select (NSS/CS)
#define LORA_SCK              18        // SPI Clock
#define LORA_MISO             19        // SPI Master In Slave Out
#define LORA_MOSI             23        // SPI Master Out Slave In
#define LORA_DIO0             2         // Packet Received Interrupt
#define LORA_RST              14        // Module Reset

// NEO-6M GPS (HardwareSerial 2)
#define GPS_RX_PIN            4         // ESP32 RX2 <- GPS TX
#define GPS_TX_PIN            15        // ESP32 TX2 -> GPS RX
#define GPS_BAUD_RATE         9600      // Default u-blox NEO-6M baud rate

// 0.96" I2C SSD1306 OLED (128x64)
#define OLED_SCL_PIN          22        // I2C Clock
#define OLED_SDA_PIN          21        // I2C Data
#define OLED_I2C_ADDR         0x3C      // Standard I2C address for SSD1306
#define SCREEN_WIDTH          128
#define SCREEN_HEIGHT         64

// Tactile Push Buttons (Active LOW with internal INPUT_PULLUP)
#define BTN_SOS_PIN           32        // Red Button: Action / Instant SOS
#define BTN_NAV_PIN           33        // Black Button: Navigation / Carousel

// Audible Alert (Optional Buzzer / LED)
#define BUZZER_PIN            27        // Alarm buzzer output

// ==============================================================================
// RADIO CONFIGURATION (SX1278 / 433 MHz ISM Band)
// ==============================================================================
#define LORA_FREQUENCY        433.0     // Frequency in MHz (Asia/EU ISM band)
#define LORA_BANDWIDTH        125.0     // Bandwidth in kHz (125 kHz standard)
#define LORA_SPREAD_FACTOR    9         // Spreading Factor (SF9 = optimal range/speed)
#define LORA_CODING_RATE      7         // Coding Rate (4/7 = good error tolerance)
#define LORA_SYNC_WORD        0x12      // Private network sync word (filters out random packets)
#define LORA_OUTPUT_POWER     20        // Output Power in dBm (+20 dBm = 100 mW max)
#define LORA_PREAMBLE_LEN     8         // Preamble symbols

// ==============================================================================
// TIMING CONSTANTS & THRESHOLDS
// ==============================================================================
#define BTN_DEBOUNCE_MS       25        // Button debounce interval
#define BTN_LONG_PRESS_MS     3000      // 3 seconds hold to trigger PANIC SOS
#define SCREEN_SLEEP_MS       20000     // 20 seconds screen sleep timeout
#define ACK_TIMEOUT_MS        2000      // 2 seconds wait for delivery confirmation
#define MAX_RETRY_COUNT       3         // Number of retransmissions if no ACK arrives

// ==============================================================================
// PRESET EMERGENCY MESSAGES
// ==============================================================================
struct EmergencyPreset {
    uint8_t code;
    const char* label;
    const char* description;
};

const EmergencyPreset EMERGENCY_PRESETS[] = {
    { 0x01, "CRITICAL SOS",  "Medical / Trapped" },
    { 0x02, "NEED SUPPLIES", "Food & Clean Water" },
    { 0x03, "EVACUATING",    "Moving to Safe Zone" },
    { 0x04, "ALL SAFE",      "Team OK / Standing By" }
};
const uint8_t PRESET_COUNT = sizeof(EMERGENCY_PRESETS) / sizeof(EmergencyPreset);
