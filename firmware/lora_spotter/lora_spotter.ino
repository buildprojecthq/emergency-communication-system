/**
 * ==============================================================================
 * Project: ESP32-LoRa Emergency Communication "Spotter" System
 * Authors: Yashas & Team
 * Target Hardware: ESP32 DevKit V1 + SX1278 (433MHz) + NEO-6M GPS + SSD1306 OLED
 * Description: 100% off-grid standalone handheld emergency communicator.
 * ==============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_bt.h>
#include <esp_sleep.h>
#include "config.h"
#include "packet_protocol.h"
#include "button_manager.h"
#include "gps_manager.h"
#include "radio_manager.h"
#include "display_manager.h"

// Hardware instances
ButtonManager  buttonMgr;
GPSManager     gpsMgr;
RadioManager   radioMgr;
DisplayManager displayMgr;

// Mutex for thread-safe access to GPS position between cores
SemaphoreHandle_t gpsMutex;
GPSLocation sharedGpsLocation;

// ==============================================================================
// CORE 0 TASK: Radio Packet Handling & GPS NMEA Decoding
// ==============================================================================
void Task_RadioGPS(void* pvParameters) {
    Serial.println("[CORE 0] Radio & GPS Task Started");
    ReceivedMessage rxMsg;

    for (;;) {
        // 1. Process incoming GPS serial data continuously
        gpsMgr.update();

        // 2. Safely copy latest GPS fix to shared struct
        if (xSemaphoreTake(gpsMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            sharedGpsLocation = gpsMgr.getLocation();
            xSemaphoreGive(gpsMutex);
        }

        // 3. Check for incoming radio packets (DIO0 interrupt)
        if (radioMgr.checkReceived(rxMsg)) {
            Serial.printf("[LORA RX] Packet from Node 0x%02X | Type: 0x%02X | RSSI: %.1f dBm\n",
                          rxMsg.packet.src_node, rxMsg.packet.msg_type, rxMsg.rssi);

            double targetLat = (double)rxMsg.packet.lat_scaled / 1000000.0;
            double targetLon = (double)rxMsg.packet.lon_scaled / 1000000.0;

            // Trigger visual alarm popup
            displayMgr.triggerAlertPopup(
                rxMsg.packet.src_node,
                rxMsg.packet.preset_code,
                targetLat,
                targetLon,
                rxMsg.rssi
            );
        }

        // 4. Check if a delivery receipt (ACK) arrived for our last sent message
        uint8_t ackNode;
        float ackRssi;
        if (radioMgr.checkAckReceived(ackNode, ackRssi)) {
            Serial.printf("[LORA ACK] Delivery confirmed by Node 0x%02X (RSSI: %.1f dBm)\n", ackNode, ackRssi);
            displayMgr.triggerAckPopup(ackNode, ackRssi);
        }

        // 5. Periodic GPS Diagnostics to Serial Monitor (every 3 seconds)
        static uint32_t lastGpsLog = 0;
        if (millis() - lastGpsLog >= 3000) {
            lastGpsLog = millis();
            Serial.printf("[GPS DIAG] Chars: %u | Fix: %s | Sats: %u | Lat: %.5f | Lon: %.5f | Alt: %dm | CksumErr: %u\n",
                          gpsMgr.getCharsProcessed(),
                          gpsMgr.hasFix() ? "YES" : "NO",
                          gpsMgr.getSatellites(),
                          gpsMgr.getLatitude(),
                          gpsMgr.getLongitude(),
                          gpsMgr.getAltitude(),
                          gpsMgr.getFailedChecksum());
        }

        // Yield CPU briefly to prevent watchdog trip
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

// ==============================================================================
// CORE 1 TASK: Button State Machine & OLED Display Rendering
// ==============================================================================
void Task_UI(void* pvParameters) {
    Serial.println("[CORE 1] UI & Button Task Started");

    for (;;) {
        // Raw pin transition check (immediate hardware sanity check)
        static bool lastRawNav = HIGH;
        static bool lastRawSos = HIGH;
        bool rawNav = digitalRead(BTN_NAV_PIN);
        bool rawSos = digitalRead(BTN_SOS_PIN);
        if (rawNav != lastRawNav) {
            lastRawNav = rawNav;
            Serial.printf("[RAW PIN] NAV (GPIO %d) -> %s\n",
                          BTN_NAV_PIN, rawNav == LOW ? "LOW (Pressed)" : "HIGH (Released)");
        }
        if (rawSos != lastRawSos) {
            lastRawSos = rawSos;
            Serial.printf("[RAW PIN] SOS (GPIO %d) -> %s\n",
                          BTN_SOS_PIN, rawSos == LOW ? "LOW (Pressed)" : "HIGH (Released)");
        }

        // 1. Poll the 2 tactile push buttons via debouncer
        ButtonEvent btnEvent = buttonMgr.update();

        if (btnEvent != EVENT_NONE) {
            displayMgr.wakeScreen();
        }

        // 2. Handle Button Actions
        if (btnEvent == EVENT_NAV_CLICK) {
            Serial.println("[BUTTON] >>> NAV CLICK DETECTED! Advancing screen / menu item <<<");
            displayMgr.nextScreen();
        } 
        else if (btnEvent == EVENT_SOS_CLICK) {
            Serial.println("[BUTTON] >>> SOS CLICK DETECTED! <<<");
            if (displayMgr.getScreenMode() == SCREEN_MENU) {
                uint8_t activeCode = displayMgr.getActivePresetIndex();
                displayMgr.triggerTxNotice("TRANSMITTING...", EMERGENCY_PRESETS[activeCode - 1].label);

                GPSLocation loc;
                if (xSemaphoreTake(gpsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                    loc = sharedGpsLocation;
                    xSemaphoreGive(gpsMutex);
                }

                int32_t latScaled = (int32_t)(loc.latitude * 1000000.0);
                int32_t lonScaled = (int32_t)(loc.longitude * 1000000.0);

                Serial.printf("[TX] Broadcasting Preset #%d to ALL nodes\n", activeCode);
                radioMgr.sendPresetMessage(activeCode, latScaled, lonScaled, loc.altitude, loc.satellites, 0);
            }
        } 
        else if (btnEvent == EVENT_PANIC_HOLD) {
            Serial.println("[BUTTON] >>> !!! PANIC HOLD (3s) DETECTED! Broadcasting SOS! !!! <<<");
            displayMgr.triggerTxNotice("! EMERGENCY SOS !", "Distress Beacon Sent");

            GPSLocation loc;
            if (xSemaphoreTake(gpsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                loc = sharedGpsLocation;
                xSemaphoreGive(gpsMutex);
            }

            int32_t latScaled = (int32_t)(loc.latitude * 1000000.0);
            int32_t lonScaled = (int32_t)(loc.longitude * 1000000.0);

            // Broadcast priority SOS at maximum LoRa power
            radioMgr.sendBroadcastSOS(latScaled, lonScaled, loc.altitude, loc.satellites, 0);
        }

        // 3. Render OLED Display (Minimalist, No Battery %)
        GPSLocation currentLoc;
        if (xSemaphoreTake(gpsMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            currentLoc = sharedGpsLocation;
            xSemaphoreGive(gpsMutex);
        }

        displayMgr.update(currentLoc);

        // Dynamic task rate: slower polling when display is sleeping to save power
        if (displayMgr.isScreenAsleep()) {
            vTaskDelay(pdMS_TO_TICKS(150));
        } else {
            vTaskDelay(pdMS_TO_TICKS(65)); // ~15 FPS refresh rate when screen active
        }
    }
}

// ==============================================================================
// ARDUINO SETUP
// ==============================================================================
void setup() {
    // --------------------------------------------------------------------------
    // POWER OPTIMIZATION: Downclock CPU & Disable Unused Radios
    // --------------------------------------------------------------------------
    // 1. Lower CPU from 240 MHz to 80 MHz (Saves ~50 mA, eliminates brownouts)
    setCpuFrequencyMhz(80);

    // 2. Shut down Wi-Fi and Bluetooth baseband radios completely (Saves ~20 mA)
    WiFi.mode(WIFI_OFF);
    btStop();

    // 3. Configure Wakeup Pins for Light Sleep
    // Wake on LoRa DIO0 (GPIO 2 HIGH = incoming packet ready)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)LORA_DIO0, 1);
    // Wake on Buttons (Active LOW)
    esp_sleep_enable_ext1_wakeup(((1ULL << BTN_NAV_PIN) | (1ULL << BTN_SOS_PIN)), ESP_EXT1_WAKEUP_ALL_LOW);

    Serial.begin(115200);
    delay(300);
    Serial.println("\n==============================================");
    Serial.printf("  ESP32-LoRa Emergency Spotter (Node 0x%02X) [80MHz Low-Power]\n", CURRENT_NODE_ID);
    Serial.println("==============================================");

    // Initialize Mutex
    gpsMutex = xSemaphoreCreateMutex();

    // Initialize all hardware peripherals
    buttonMgr.begin();
    Serial.printf("[BUTTON PIN CHECK] NAV (GPIO %d) = %s | SOS (GPIO %d) = %s\n",
                  BTN_NAV_PIN, digitalRead(BTN_NAV_PIN) == LOW ? "LOW (Pressed/Shorted!)" : "HIGH (Normal Released)",
                  BTN_SOS_PIN, digitalRead(BTN_SOS_PIN) == LOW ? "LOW (Pressed/Shorted!)" : "HIGH (Normal Released)");

    displayMgr.begin();
    gpsMgr.begin();

    if (!radioMgr.begin()) {
        Serial.println("[FATAL] LoRa radio failed to initialize. Check wiring!");
    } else {
        Serial.println("[OK] LoRa radio active on 433 MHz.");
    }

    // Spawn FreeRTOS Tasks pinned to specific cores
    xTaskCreatePinnedToCore(
        Task_RadioGPS,    // Task function
        "RadioGPSTask",   // Task name
        4096,             // Stack size in words
        NULL,             // Parameter
        2,                // Priority (High priority for RF interrupts)
        NULL,             // Task handle
        0                 // Core 0
    );

    xTaskCreatePinnedToCore(
        Task_UI,          // Task function
        "UITask",         // Task name
        4096,             // Stack size in words
        NULL,             // Parameter
        1,                // Priority (Medium priority for display)
        NULL,             // Task handle
        1                 // Core 1
    );
}

// Main Arduino loop remains idle since FreeRTOS manages both tasks
void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
