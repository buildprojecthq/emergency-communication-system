#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "packet_protocol.h"
#include "gps_manager.h"

enum ScreenMode {
    SCREEN_HOME = 0,
    SCREEN_MENU,
    SCREEN_RADAR,
    SCREEN_ALERT_POPUP,
    SCREEN_ACK_POPUP
};

class DisplayManager {
public:
    DisplayManager();
    bool begin();
    
    void update(const GPSLocation& localGps, uint8_t batteryPct);
    
    // Screen navigation
    void nextScreen();
    void nextPreset();
    uint8_t getActivePresetIndex();
    ScreenMode getScreenMode();

    // Alert & ACK Triggers
    void triggerAlertPopup(uint8_t srcNode, uint8_t presetCode, double lat, double lon, float rssi);
    void triggerAckPopup(uint8_t ackNode, float rssi);
    void triggerSendingNotice();
    void dismissPopup();

    // Power management
    void wakeScreen();
    bool isScreenAsleep();

    // Set remote target for Rescue Radar
    void setRadarTarget(uint8_t targetNode, double targetLat, double targetLon);

private:
    Adafruit_SSD1306 _display;
    ScreenMode       _currentScreen;
    ScreenMode       _previousScreen;
    uint8_t          _activePreset;
    
    // Popup state
    uint32_t         _popupStartTime;
    uint32_t         _lastUserActivity;
    bool             _isAsleep;

    // Last alert details
    uint8_t          _lastAlertNode;
    uint8_t          _lastAlertPreset;
    double           _lastAlertLat;
    double           _lastAlertLon;
    float            _lastAlertRssi;

    // Last ACK details
    uint8_t          _lastAckNode;
    float            _lastAckRssi;

    // Rescue Radar target
    bool             _hasRadarTarget;
    uint8_t          _radarTargetNode;
    double           _radarTargetLat;
    double           _radarTargetLon;

    void renderHomeScreen(const GPSLocation& localGps, uint8_t batteryPct);
    void renderMenuScreen();
    void renderRadarScreen(const GPSLocation& localGps);
    void renderAlertPopup();
    void renderAckPopup();
};
