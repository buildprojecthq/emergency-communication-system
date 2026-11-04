#include "gps_manager.h"
#include <math.h>

#ifndef DEG_TO_RAD
#define DEG_TO_RAD (PI / 180.0)
#endif
#ifndef RAD_TO_DEG
#define RAD_TO_DEG (180.0 / PI)
#endif

GPSManager::GPSManager() : _gpsSerial(2) {}

void GPSManager::begin() {
    // Initialize HardwareSerial2 on ESP32 with specified RX/TX pins
    _gpsSerial.begin(GPS_BAUD_RATE, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
}

void GPSManager::update() {
    while (_gpsSerial.available() > 0) {
        _gps.encode(_gpsSerial.read());
    }
}

GPSLocation GPSManager::getLocation() {
    GPSLocation loc;
    loc.valid = _gps.location.isValid();
    loc.latitude = loc.valid ? _gps.location.lat() : 0.0;
    loc.longitude = loc.valid ? _gps.location.lng() : 0.0;
    loc.altitude = _gps.altitude.isValid() ? (int16_t)_gps.altitude.meters() : 0;
    loc.satellites = _gps.satellites.isValid() ? (uint8_t)_gps.satellites.value() : 0;
    return loc;
}

bool GPSManager::hasFix() {
    return _gps.location.isValid() && _gps.location.age() < 5000;
}

double GPSManager::getLatitude() {
    return _gps.location.isValid() ? _gps.location.lat() : 0.0;
}

double GPSManager::getLongitude() {
    return _gps.location.isValid() ? _gps.location.lng() : 0.0;
}

int32_t GPSManager::getScaledLatitude() {
    return (int32_t)(getLatitude() * 1000000.0);
}

int32_t GPSManager::getScaledLongitude() {
    return (int32_t)(getLongitude() * 1000000.0);
}

int16_t GPSManager::getAltitude() {
    return _gps.altitude.isValid() ? (int16_t)_gps.altitude.meters() : 0;
}

uint8_t GPSManager::getSatellites() {
    return _gps.satellites.isValid() ? (uint8_t)_gps.satellites.value() : 0;
}

// ==============================================================================
// HAVERSINE DISTANCE FORMULA (Earth Radius = 6,371,000 meters)
// Returns distance in meters between two coordinates
// ==============================================================================
double GPSManager::calculateDistanceMeters(double lat1, double lon1, double lat2, double lon2) {
    double dLat = (lat2 - lat1) * DEG_TO_RAD;
    double dLon = (lon2 - lon1) * DEG_TO_RAD;

    double rLat1 = lat1 * DEG_TO_RAD;
    double rLat2 = lat2 * DEG_TO_RAD;

    double a = sin(dLat / 2.0) * sin(dLat / 2.0) +
               cos(rLat1) * cos(rLat2) *
               sin(dLon / 2.0) * sin(dLon / 2.0);

    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    return 6371000.0 * c;
}

// ==============================================================================
// FORWARD AZIMUTH BEARING FORMULA
// Returns initial compass bearing (0° to 360°) from point 1 to point 2
// ==============================================================================
double GPSManager::calculateBearingDegrees(double lat1, double lon1, double lat2, double lon2) {
    double rLat1 = lat1 * DEG_TO_RAD;
    double rLat2 = lat2 * DEG_TO_RAD;
    double dLon  = (lon2 - lon1) * DEG_TO_RAD;

    double y = sin(dLon) * cos(rLat2);
    double x = cos(rLat1) * sin(rLat2) - sin(rLat1) * cos(rLat2) * cos(dLon);

    double bearing = atan2(y, x) * RAD_TO_DEG;
    bearing = fmod((bearing + 360.0), 360.0);
    return bearing;
}

const char* GPSManager::getBearingCardinal(double bearingDeg) {
    const char* cardinals[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
    int index = (int)((bearingDeg + 22.5) / 45.0) % 8;
    return cardinals[index];
}

const char* GPSManager::getBearingArrow(double bearingDeg) {
    // Arrow symbols representing 8 cardinal directions
    const char* arrows[] = { "^", "^>", ">", "v>", "v", "<v", "<", "<^" };
    int index = (int)((bearingDeg + 22.5) / 45.0) % 8;
    return arrows[index];
}
