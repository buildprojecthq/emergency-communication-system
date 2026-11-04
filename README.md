# ESP32-LoRa Emergency Communication Spotter System

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Framework: Arduino](https://img.shields.io/badge/Framework-Arduino-blue.svg)](https://www.arduino.cc/)
[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-red.svg)](https://www.espressif.com/)
[![LoRa: SX1278](https://img.shields.io/badge/LoRa-SX1278%20433MHz-brightgreen.svg)](https://www.semtech.com/)

A **100% off-grid, standalone handheld emergency communicator** designed for natural disaster zones, remote search-and-rescue, and cellular infrastructure blackouts. 

Operating entirely without the internet, cellular towers, or mobile apps, two or more **Spotter** devices communicate over multi-kilometer LoRa radio waves to transmit distress alerts, share GPS coordinates, and guide rescuers directly to victims using an on-screen **Rescue Radar**.

---

## Key Features

* **100% Offline & Infrastructure-Independent:** Zero SIM cards, zero Wi-Fi, zero cellular networks, and zero mobile apps required.
* **Multi-Node Emergency Broadcast:** An SOS triggered by one unit is received simultaneously by **every Spotter in radio range**.
* **Radio-Only Delivery Confirmation (Zero-Internet ACK):** Automatic return radio receipt confirms when a distress beacon reaches rescue personnel (`✔ SOS HEARD BY RESCUER`).
* **Built-in Rescue "Radar" (Digital Direction Finder):**
  * Computes real-time distance in meters using the **Haversine formula**.
  * Displays a directional compass arrow (`↑`, `↗`, `→`, `↘`, `↓`, `↙`, `←`, `↖`) pointing rescuers straight to the survivor.
* **One-Touch 3-Second Panic SOS:** Holding the Red Button for 3 seconds overrides all menus and blasts a priority distress beacon at maximum (+20 dBm) radio power.
* **Minimalist 2-Button Interface:**
  * **Black Button (NAV):** Cycles through Home, Emergency Menu, and Rescue Radar.
  * **Red Button (ACTION):** Sends the highlighted message or triggers the panic override.
* **Dual-Core FreeRTOS Concurrency:** Core 0 handles real-time RF interrupts and GPS parsing, while Core 1 renders the 15 FPS OLED interface without screen lag.
* **Power-Saving OLED Sleep:** The display automatically dims and sleeps after 20 seconds of idle time to conserve battery.

---

## Hardware Bill of Materials (BOM)

| Component | Specification | Quantity (per Spotter) |
| :--- | :--- | :--- |
| **Microcontroller** | ESP32 DevKit V1 (30-pin) | 1 |
| **LoRa Transceiver** | Ai-Thinker Ra-02 (SX1278, 433 MHz) | 1 |
| **GPS Module** | u-blox NEO-6M with ceramic patch antenna | 1 |
| **Display** | 0.96" SSD1306 I2C OLED (128×64) | 1 |
| **Input Buttons** | 2× Tactile Push Buttons (Red = SOS, Black = NAV) | 2 |
| **Substrate** | Single-sided Zero-PCB (Perfboard) + Female headers | 1 |
| **Power** | 3.7V 18650 Li-ion battery or 5V Powerbank | 1 |

---

## Hardware Pinout Table

| Module | Pin | ESP32 GPIO | Bus / Protocol | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **LoRa (SX1278)** | NSS / CS | **GPIO 5** | SPI | Active Low Chip Select |
| | SCK | **GPIO 18** | SPI | Clock |
| | MISO | **GPIO 19** | SPI | Master In |
| | MOSI | **GPIO 23** | SPI | Master Out |
| | DIO0 | **GPIO 2** | Interrupt | Packet Received IRQ |
| | RST | **GPIO 14** | Reset | Active Low Reset |
| **GPS (NEO-6M)** | TX | **GPIO 4** | UART2 (RX) | ESP32 RX <- GPS TX (9600 baud) |
| | RX | **GPIO 15** | UART2 (TX) | ESP32 TX -> GPS RX |
| **OLED (SSD1306)** | SCL | **GPIO 22** | I2C | Clock (400 kHz) |
| | SDA | **GPIO 21** | I2C | Data |
| **Red Button (SOS)** | Pin 1 -> Pin 2 | **GPIO 32** -> GND | GPIO Input | Internal `INPUT_PULLUP` |
| **Black Button (NAV)**| Pin 1 -> Pin 2 | **GPIO 33** -> GND | GPIO Input | Internal `INPUT_PULLUP` |

---

## Power Supply & Voltage Distribution Guide

| Module / Component | Supply Voltage | Connect to ESP32 Pin | Current Draw | Critical Warning |
| :--- | :--- | :--- | :--- | :--- |
| **SX1278 LoRa (Ra-02)** | **STRICTLY 3.3V** | **`3.3V`** | ~120 mA (Peak TX) | ⚠️ **DO NOT connect to 5V!** Voltage >3.7V will permanently destroy the SX1278 chip. |
| **NEO-6M GPS** | **3.3V or 5V** | **`VIN` (5V)** or **`3.3V`** | ~50 mA | Safe on 5V because of onboard 3.3V regulator. Using `VIN` unloads the 3.3V rail. |
| **0.96" SSD1306 OLED** | **3.3V or 5V** | **`3.3V`** | ~20 mA | Safe on 3.3V or 5V. |
| **Tactile Push Buttons** | **No Power Rail** | **`GPIO` to `GND`** | < 1 mA | Uses internal `INPUT_PULLUP`. Do NOT connect external voltage to buttons. |

> **Pro-Tip:** Solder a **100 µF capacitor** across the LoRa module's `3.3V` and `GND` pins to buffer the 120 mA current spikes during +20 dBm RF transmission.

---

## Project File Structure

```
emergency-communication-system/
├── firmware/                      # All embedded C++ source & header files
│   ├── lora_spotter.ino           # Main FreeRTOS dual-core task orchestration sketch
│   ├── config.h                   # Pinouts, RF settings, Node ID toggle, timings
│   ├── packet_protocol.h          # 24-byte compact binary packet & CRC16 checksum
│   ├── button_manager.h/.cpp      # 2-button debouncer (single click vs 3s panic hold)
│   ├── gps_manager.h/.cpp         # GPS NMEA parsing & Haversine distance/azimuth math
│   ├── radio_manager.h/.cpp       # SX1278 LoRa driver, DIO0 interrupt & ACK handling
│   └── display_manager.h/.cpp     # 3-screen OLED renderer (Home, Menu, Radar, Popups)
├── docs/                          # Architecture & design documentation
│   ├── FIRMWARE_SOFTWARE_ARCHITECTURE.md
│   └── IMPLEMENTATION_PLAN.md
├── hardware/                      # Schematics and circuit design notes
├── platformio.ini                 # PlatformIO project configuration (src_dir = firmware)
├── .gitignore                     # Git exclusions
├── LICENSE                        # MIT License
└── README.md                      # Project documentation
```

---

## Flashing Instructions

### 1. Setting Node IDs
Before uploading, set the unique identity for each device in `firmware/config.h`:
* **For Spotter 1:**
  ```cpp
  #define CURRENT_NODE_ID 0x01
  ```
* **For Spotter 2:**
  ```cpp
  #define CURRENT_NODE_ID 0x02
  ```

---

### 2. Uploading via Arduino IDE
1. Open the Arduino IDE.
2. Go to **Sketch** -> **Include Library** -> **Manage Libraries...** and install:
   * **`RadioLib`** (by Jan Gromes)
   * **`TinyGPSPlus`** (by Mikal Hart)
   * **`Adafruit SSD1306`** (by Adafruit)
   * **`Adafruit GFX Library`** (by Adafruit)
3. Select Board: **Tools** -> **Board** -> **ESP32 Arduino** -> **DOIT ESP32 DEVKIT V1**.
4. Select your COM / Serial port.
5. Open `firmware/lora_spotter.ino` and click **Upload**.

---

### 3. Uploading via PlatformIO (VS Code)
1. Open the repository root folder in VS Code with the PlatformIO extension.
2. PlatformIO will automatically download all required libraries listed in `platformio.ini`.
3. Connect your ESP32 via USB and click **PlatformIO: Upload**.

---

## Operation Guide

```
+───────────────────────+   [BLACK BUTTON]   +───────────────────────+
|     1. HOME SCREEN    | -----------------> |  2. EMERGENCY MENU    |
| Lat: 12.9234 Lon: 77.5|                    | > [1] MEDICAL CRISIS  |
| Sats: 8   |  Bat: 95% | <----------------- |   [2] NEED SUPPLIES   |
+───────────────────────+    (wraps around)  +───────────────────────+
           ^                                             |
           |              [BLACK BUTTON]                 |
           +---------------------------------------------+
                                   |
                                   v
                      +───────────────────────+
                      |   3. RESCUE RADAR     |
                      | Target: SPOTTER 1     |
                      | Distance: 350 METERS  |
                      | Direction: NORTHEAST ↗|
                      +───────────────────────+
```

* **Browse Menu:** Tap the **Black Button** to cycle through screens and messages.
* **Send Selected Alert:** Highlight a message on the menu and tap the **Red Button**.
* **Instant Emergency Panic:** **Hold the Red Button for 3 seconds** from any screen to blast an immediate distress beacon to all units.

---

## License

This project is licensed under the [MIT License](LICENSE).
