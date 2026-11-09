#include "display_manager.h"

DisplayManager::DisplayManager()
    : _display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1),
      _currentScreen(SCREEN_HOME),
      _previousScreen(SCREEN_HOME),
      _activePreset(0),
      _popupStartTime(0),
      _lastUserActivity(0),
      _isAsleep(false),
      _lastAlertNode(0),
      _lastAlertPreset(0),
      _lastAlertLat(0.0),
      _lastAlertLon(0.0),
      _lastAlertRssi(0.0),
      _lastAckNode(0),
      _lastAckRssi(0.0),
      _hasRadarTarget(false),
      _radarTargetNode(0),
      _radarTargetLat(0.0),
      _radarTargetLon(0.0),
      _txNoticeTitle(""),
      _txNoticeMsg("") {}

bool DisplayManager::begin() {
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    if (!_display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
        Serial.println("[OLED] SSD1306 allocation failed");
        return false;
    }

    // Lower contrast to save current and eliminate brownout risk
    _display.ssd1306_command(SSD1306_SETCONTRAST);
    _display.ssd1306_command(OLED_CONTRAST_LOW);

    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);
    _display.setTextSize(1);
    
    // Minimal Splash screen
    _display.setCursor(20, 18);
    _display.setTextSize(2);
    _display.println("SPOTTER");
    _display.setTextSize(1);
    _display.setCursor(20, 38);
    _display.printf("NODE ID: 0x%02X\n", CURRENT_NODE_ID);
    _display.setCursor(20, 50);
    _display.println("Emergency LoRa");
    _display.display();
    delay(1000);

    _lastUserActivity = millis();
    return true;
}

void DisplayManager::wakeScreen() {
    _lastUserActivity = millis();
    if (_isAsleep) {
        _display.ssd1306_command(SSD1306_DISPLAYON);
        _isAsleep = false;
    }
}

bool DisplayManager::isScreenAsleep() {
    return _isAsleep;
}

void DisplayManager::nextScreen() {
    wakeScreen();
    if (_currentScreen == SCREEN_ALERT_POPUP || _currentScreen == SCREEN_ACK_POPUP || _currentScreen == SCREEN_TX_POPUP) {
        dismissPopup();
        return;
    }

    if (_currentScreen == SCREEN_HOME) {
        _currentScreen = SCREEN_MENU;
    } else if (_currentScreen == SCREEN_MENU) {
        // If on menu, pressing NAV cycles to next message preset first
        nextPreset();
    } else if (_currentScreen == SCREEN_RADAR) {
        _currentScreen = SCREEN_HOME;
    }
}

void DisplayManager::nextPreset() {
    wakeScreen();
    _activePreset = (_activePreset + 1) % (PRESET_COUNT + 1);
    if (_activePreset == PRESET_COUNT) {
        // Wrapped past last message, advance to Radar screen
        _currentScreen = SCREEN_RADAR;
        _activePreset = 0;
    }
}

uint8_t DisplayManager::getActivePresetIndex() {
    return _activePreset + 1; // 1-indexed preset code
}

ScreenMode DisplayManager::getScreenMode() {
    return _currentScreen;
}

void DisplayManager::triggerAlertPopup(uint8_t srcNode, uint8_t presetCode, double lat, double lon, float rssi) {
    wakeScreen();
    _previousScreen  = _currentScreen;
    _currentScreen   = SCREEN_ALERT_POPUP;
    _lastAlertNode   = srcNode;
    _lastAlertPreset = presetCode;
    _lastAlertLat    = lat;
    _lastAlertLon    = lon;
    _lastAlertRssi   = rssi;
    _popupStartTime  = millis();

    // Automatically set as active radar tracking target
    setRadarTarget(srcNode, lat, lon);
}

void DisplayManager::triggerAckPopup(uint8_t ackNode, float rssi) {
    wakeScreen();
    _previousScreen = _currentScreen;
    _currentScreen  = SCREEN_ACK_POPUP;
    _lastAckNode    = ackNode;
    _lastAckRssi    = rssi;
    _popupStartTime = millis();
}

void DisplayManager::triggerSendingNotice() {
    triggerTxNotice("TRANSMITTING...", "Sending Beacon...");
}

void DisplayManager::triggerTxNotice(const char* title, const char* message) {
    wakeScreen();
    _previousScreen  = _currentScreen;
    _currentScreen   = SCREEN_TX_POPUP;
    _txNoticeTitle   = title;
    _txNoticeMsg     = message;
    _popupStartTime  = millis();
}

void DisplayManager::dismissPopup() {
    _currentScreen = _previousScreen;
    if (_currentScreen == SCREEN_ALERT_POPUP || _currentScreen == SCREEN_ACK_POPUP || _currentScreen == SCREEN_TX_POPUP) {
        _currentScreen = SCREEN_HOME;
    }
}

void DisplayManager::setRadarTarget(uint8_t targetNode, double targetLat, double targetLon) {
    _hasRadarTarget  = true;
    _radarTargetNode = targetNode;
    _radarTargetLat  = targetLat;
    _radarTargetLon  = targetLon;
}

void DisplayManager::update(const GPSLocation& localGps) {
    // Check auto-sleep timeout
    if (!_isAsleep && (millis() - _lastUserActivity > SCREEN_SLEEP_MS)) {
        _display.ssd1306_command(SSD1306_DISPLAYOFF);
        _isAsleep = true;
        return;
    }

    if (_isAsleep) return;

    // Auto-dismiss popups: alert/ack after 6s, tx notice after 3s
    if ((_currentScreen == SCREEN_ALERT_POPUP || _currentScreen == SCREEN_ACK_POPUP) &&
        (millis() - _popupStartTime > 6000)) {
        dismissPopup();
    } else if (_currentScreen == SCREEN_TX_POPUP &&
        (millis() - _popupStartTime > 3000)) {
        dismissPopup();
    }

    _display.clearDisplay();

    switch (_currentScreen) {
        case SCREEN_HOME:
            renderHomeScreen(localGps);
            break;
        case SCREEN_MENU:
            renderMenuScreen();
            break;
        case SCREEN_RADAR:
            renderRadarScreen(localGps);
            break;
        case SCREEN_ALERT_POPUP:
            renderAlertPopup();
            break;
        case SCREEN_ACK_POPUP:
            renderAckPopup();
            break;
        case SCREEN_TX_POPUP:
            renderTxPopup();
            break;
    }

    _display.display();
}

void DisplayManager::renderHomeScreen(const GPSLocation& localGps) {
    _display.setTextColor(SSD1306_WHITE);
    _display.setTextSize(1);

    // Minimal Header (No solid fill bar!)
    _display.setCursor(0, 2);
    _display.printf("SPOTTER %d", CURRENT_NODE_ID);
    _display.setCursor(76, 2);
    _display.printf("SATS: %d", localGps.satellites);
    _display.drawLine(0, 11, 128, 11, SSD1306_WHITE);

    // Body: GPS Telemetry
    if (localGps.valid) {
        _display.setCursor(0, 18);
        _display.printf("LAT: %.5f\n", localGps.latitude);
        _display.setCursor(0, 29);
        _display.printf("LON: %.5f\n", localGps.longitude);
        _display.setCursor(0, 40);
        _display.printf("ALT: %dm\n", localGps.altitude);
    } else {
        _display.setCursor(14, 23);
        _display.println("GPS SEARCHING...");
        _display.setCursor(14, 35);
        _display.println("(Under open sky)");
    }

    // Bottom Footer Tip
    _display.drawLine(0, 52, 128, 52, SSD1306_WHITE);
    _display.setCursor(0, 55);
    _display.println("[NAV] Menu  [SOS 3s] Panic");
}

void DisplayManager::renderMenuScreen() {
    _display.setTextColor(SSD1306_WHITE);
    _display.setTextSize(1);

    // Minimal Header (No fillRect)
    _display.setCursor(0, 2);
    _display.println("SELECT EMERGENCY MSG");
    _display.drawLine(0, 11, 128, 11, SSD1306_WHITE);

    // Carousel Items with clean arrow cursor
    for (uint8_t i = 0; i < PRESET_COUNT; i++) {
        int y = 15 + (i * 9);
        _display.setCursor(0, y);
        if (i == _activePreset) {
            _display.printf("> [%d] %s", i + 1, EMERGENCY_PRESETS[i].label);
        } else {
            _display.printf("  [%d] %s", i + 1, EMERGENCY_PRESETS[i].label);
        }
    }

    // Footer
    _display.drawLine(0, 52, 128, 52, SSD1306_WHITE);
    _display.setCursor(0, 55);
    _display.println("[NAV] Next   [RED] Send");
}

void DisplayManager::renderRadarScreen(const GPSLocation& localGps) {
    _display.setTextColor(SSD1306_WHITE);
    _display.setTextSize(1);

    // Minimal Header (No fillRect)
    _display.setCursor(0, 2);
    _display.println("RESCUE RADAR MODE");
    _display.drawLine(0, 11, 128, 11, SSD1306_WHITE);

    if (!_hasRadarTarget) {
        _display.setCursor(6, 24);
        _display.println("NO ACTIVE TARGET");
        _display.setCursor(6, 36);
        _display.println("Waiting for SOS...");
        _display.drawLine(0, 52, 128, 52, SSD1306_WHITE);
        _display.setCursor(0, 55);
        _display.println("[NAV]=Home");
        return;
    }

    // Target Info
    _display.setCursor(0, 15);
    _display.printf("Target: SPOTTER %d\n", _radarTargetNode);

    if (localGps.valid) {
        double dist = GPSManager::calculateDistanceMeters(
            localGps.latitude, localGps.longitude,
            _radarTargetLat, _radarTargetLon
        );
        double bearing = GPSManager::calculateBearingDegrees(
            localGps.latitude, localGps.longitude,
            _radarTargetLat, _radarTargetLon
        );

        // Large Distance Display
        _display.setCursor(0, 28);
        _display.setTextSize(2);
        if (dist >= 1000.0) {
            _display.printf("%.1fkm", dist / 1000.0);
        } else {
            _display.printf("%dm", (int)dist);
        }

        // Bearing & Direction Arrow
        _display.setTextSize(1);
        _display.setCursor(76, 26);
        _display.printf("HEAD: %s", GPSManager::getBearingCardinal(bearing));
        _display.setCursor(76, 38);
        _display.setTextSize(2);
        _display.printf(" %s", GPSManager::getBearingArrow(bearing));
    } else {
        _display.setCursor(0, 28);
        _display.println("Acquiring Local GPS...");
    }

    _display.setTextSize(1);
    _display.drawLine(0, 52, 128, 52, SSD1306_WHITE);
    _display.setCursor(0, 55);
    _display.println("[NAV]=Home");
}

void DisplayManager::renderAlertPopup() {
    // Double border box (No full-screen fillRect power surge!)
    _display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
    _display.drawRect(2, 2, 124, 60, SSD1306_WHITE);
    _display.setTextColor(SSD1306_WHITE);
    _display.setTextSize(1);

    _display.setCursor(8, 6);
    _display.println("! INCOMING ALERT !");
    _display.drawLine(4, 15, 124, 15, SSD1306_WHITE);

    _display.setCursor(8, 19);
    _display.printf("FROM: SPOTTER %d\n", _lastAlertNode);

    _display.setCursor(8, 30);
    const char* desc = "CRITICAL SOS";
    if (_lastAlertPreset >= 1 && _lastAlertPreset <= PRESET_COUNT) {
        desc = EMERGENCY_PRESETS[_lastAlertPreset - 1].label;
    }
    _display.printf("TYPE: %s\n", desc);

    _display.setCursor(8, 41);
    _display.printf("RSSI: %.0f dBm\n", _lastAlertRssi);

    _display.setCursor(8, 51);
    _display.println("Radar Target Updated!");
}

void DisplayManager::renderAckPopup() {
    _display.drawRect(2, 2, 124, 60, SSD1306_WHITE);
    _display.setTextColor(SSD1306_WHITE);
    _display.setTextSize(1);
    
    _display.setCursor(14, 10);
    _display.println("v MESSAGE DELIVERED");
    _display.drawLine(4, 20, 124, 20, SSD1306_WHITE);

    _display.setCursor(12, 26);
    _display.printf("Receipt: SPOTTER %d\n", _lastAckNode);

    _display.setCursor(12, 38);
    _display.printf("Signal: %.0f dBm\n", _lastAckRssi);

    _display.setCursor(16, 49);
    _display.println("[Ack Received OK]");
}

void DisplayManager::renderTxPopup() {
    _display.drawRect(2, 2, 124, 60, SSD1306_WHITE);
    _display.setTextColor(SSD1306_WHITE);
    _display.setTextSize(1);

    _display.setCursor(8, 8);
    _display.print(_txNoticeTitle);
    _display.drawLine(4, 18, 124, 18, SSD1306_WHITE);

    _display.setCursor(8, 24);
    _display.print(_txNoticeMsg);

    _display.setCursor(8, 37);
    _display.print("Broadcasted to ALL");

    _display.setCursor(8, 49);
    _display.print("Waiting for ACK...");
}
