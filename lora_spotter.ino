/**
 * ==============================================================================
 * Project: ESP32-LoRa Emergency Communication "Spotter" System
 * Authors: Yashas & Team
 * Target Hardware: ESP32 DevKit V1 + SX1278 (433MHz) + NEO-6M GPS + SSD1306 OLED
 * Description: 100% off-grid standalone handheld emergency communicator.
 * ==============================================================================
 */

#include <Arduino.h>
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

// Beep helper functions
void beepChirp(uint16_t durationMs = 30) {
    #if defined(BUZZER_PIN)
    digitalWrite(BUZZER_PIN, HIGH);
    delay(durationMs);
    digitalWrite(BUZZER_PIN, LOW);
    #endif
}

void soundAlarmPattern() {
    #if defined(BUZZER_PIN)
    for (int i = 0; i < 4; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(120);
        digitalWrite(BUZZER_PIN, LOW);
        delay(80);
    }
    #endif
}

void soundAckPattern() {
    #if defined(BUZZER_PIN)
    digitalWrite(BUZZER_PIN, HIGH);
    delay(40);
    digitalWrite(BUZZER_PIN, LOW);
    delay(40);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(80);
    digitalWrite(BUZZER_PIN, LOW);
    #endif
}

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

            // Trigger visual & audible alarm
            displayMgr.triggerAlertPopup(
                rxMsg.packet.src_node,
                rxMsg.packet.preset_code,
                targetLat,
                targetLon,
                rxMsg.rssi
            );

            // Beep the emergency siren
            soundAlarmPattern();
        }

        // 4. Check if a delivery receipt (ACK) arrived for our last sent message
        uint8_t ackNode;
        float ackRssi;
        if (radioMgr.checkAckReceived(ackNode, ackRssi)) {
            Serial.printf("[LORA ACK] Delivery confirmed by Node 0x%02X (RSSI: %.1f dBm)\n", ackNode, ackRssi);
            displayMgr.triggerAckPopup(ackNode, ackRssi);
            soundAckPattern();
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
        // 1. Poll the 2 tactile push buttons
        ButtonEvent btnEvent = buttonMgr.update();

        if (btnEvent != EVENT_NONE) {
            displayMgr.wakeScreen();
            beepChirp(20);
        }

        // 2. Handle Button Actions
        if (btnEvent == EVENT_NAV_CLICK) {
            // Black button: Advance through screens / messages
            displayMgr.nextScreen();
        } 
        else if (btnEvent == EVENT_SOS_CLICK) {
            // Red button single click: If on Menu, transmit highlighted message
            if (displayMgr.getScreenMode() == SCREEN_MENU) {
                uint8_t activeCode = displayMgr.getActivePresetIndex();
                displayMgr.triggerSendingNotice();

                GPSLocation loc;
                if (xSemaphoreTake(gpsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                    loc = sharedGpsLocation;
                    xSemaphoreGive(gpsMutex);
                }

                int32_t latScaled = (int32_t)(loc.latitude * 1000000.0);
                int32_t lonScaled = (int32_t)(loc.longitude * 1000000.0);

                Serial.printf("[TX] Broadcasting Preset #%d to ALL nodes\n", activeCode);
                radioMgr.sendPresetMessage(activeCode, latScaled, lonScaled, loc.altitude, loc.satellites, 95);
            }
        } 
        else if (btnEvent == EVENT_PANIC_HOLD) {
            // Red button held for 3 seconds: INSTANT EMERGENCY SOS OVERRIDE!
            Serial.println("[TX] !!! PANIC OVERRIDE TRIGGERED !!!");
            displayMgr.triggerSendingNotice();
            soundAlarmPattern();

            GPSLocation loc;
            if (xSemaphoreTake(gpsMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                loc = sharedGpsLocation;
                xSemaphoreGive(gpsMutex);
            }

            int32_t latScaled = (int32_t)(loc.latitude * 1000000.0);
            int32_t lonScaled = (int32_t)(loc.longitude * 1000000.0);

            // Broadcast priority SOS at maximum +20 dBm LoRa power
            radioMgr.sendBroadcastSOS(latScaled, lonScaled, loc.altitude, loc.satellites, 95);
        }

        // 3. Render OLED Display at ~15 FPS
        GPSLocation currentLoc;
        if (xSemaphoreTake(gpsMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            currentLoc = sharedGpsLocation;
            xSemaphoreGive(gpsMutex);
        }

        displayMgr.update(currentLoc, 95); // Estimated battery 95%

        vTaskDelay(pdMS_TO_TICKS(65)); // ~15 FPS refresh rate
    }
}

// ==============================================================================
// ARDUINO SETUP
// ==============================================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n==============================================");
    Serial.printf("  ESP32-LoRa Emergency Spotter (Node 0x%02X)\n", CURRENT_NODE_ID);
    Serial.println("==============================================");

    // Initialize optional buzzer
    #if defined(BUZZER_PIN)
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    #endif

    // Initialize Mutex
    gpsMutex = xSemaphoreCreateMutex();

    // Initialize all hardware peripherals
    buttonMgr.begin();
    displayMgr.begin();
    gpsMgr.begin();

    if (!radioMgr.begin()) {
        Serial.println("[FATAL] LoRa radio failed to initialize. Check wiring!");
    } else {
        Serial.println("[OK] LoRa radio active on 433 MHz.");
    }

    beepChirp(100);

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
