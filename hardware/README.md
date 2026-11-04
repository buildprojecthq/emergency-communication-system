# Hardware Documentation

This folder is reserved for hardware schematics, wiring pinout diagrams, KiCad/EasyEDA files, and prototype build photos.

### Key Connections Summary:
- **LoRa (SX1278 Ra-02):** SPI (NSS: GPIO 5, SCK: GPIO 18, MISO: GPIO 19, MOSI: GPIO 23, DIO0: GPIO 2, RST: GPIO 14) — **Power: 3.3V ONLY**
- **GPS (NEO-6M):** UART2 (TX to GPIO 4, RX to GPIO 15) — **Power: VIN (5V) or 3.3V**
- **OLED (SSD1306):** I2C (SCL: GPIO 22, SDA: GPIO 21) — **Power: 3.3V**
- **Buttons:** Red SOS to GPIO 32, Black NAV to GPIO 33 (Active LOW to GND with internal PULLUP)

