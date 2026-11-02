#pragma once

#include <Arduino.h>
#include <TinyGPS++.h>
#include "config.h"

struct GPSLocation {
    double   latitude;
    double   longitude;
    int16_t  altitude;
    uint8_t  satellites;
    bool     valid;
};

class GPSManager {
public:
    GPSManager();
    void begin();
    void update(); // Call frequently to process UART NMEA sentences

    GPSLocation getLocation();
    bool hasFix();
    double getLatitude();
    double getLongitude();
    int32_t getScaledLatitude();
    int32_t getScaledLongitude();
    int16_t getAltitude();
    uint8_t getSatellites();

    // Rescue Radar Mathematics
    static double calculateDistanceMeters(double lat1, double lon1, double lat2, double lon2);
    static double calculateBearingDegrees(double lat1, double lon1, double lat2, double lon2);
    static const char* getBearingCardinal(double bearingDeg);
    static const char* getBearingArrow(double bearingDeg);

private:
    TinyGPSPlus _gps;
    HardwareSerial _gpsSerial;
};
