# Software Implementation Plan
## ESP32-LoRa Emergency Spotter (Firmware Modules & Rollout)

---

## 1. Code Architecture & Folder Structure

To keep the code clean, professional, and easy to debug, we will structure it into **modular header & source files**. This structure works seamlessly with both **Arduino IDE** and **PlatformIO**:

```
lora-spotter/
├── config.h               # Hardware pinouts, radio frequency, Node ID, timing constants
├── packet_protocol.h      # 28-byte binary packet structure & message type definitions
├── button_manager.h/.cpp  # Non-blocking button debouncing, single click, 3s long-press
├── gps_manager.h/.cpp     # GPS UART2 parsing, satellite lock, Haversine distance & bearing
├── radio_manager.h/.cpp   # LoRa (SX1278) transmit/receive, interrupt handling, ACK logic
├── display_manager.h/.cpp # OLED 128x64 screens (Home, Menu Carousel, Radar arrow, Alarms)
└── lora_spotter.ino       # Main entry point, FreeRTOS tasks & queue orchestration
```

---

## 2. The 6 Core Software Modules to Implement

### Module 1: System Configuration (`config.h`)
* Defines hardware GPIO pins (LoRa SPI, GPS UART2, OLED I2C, 2 Buttons, Buzzer).
* Hardware Node ID setting (`NODE_ID_1` vs `NODE_ID_2`) so flashing either board is as simple as toggling a single line (`#define CURRENT_NODE_ID 0x01`).
* RF parameters (433 MHz, Spreading Factor SF9 for maximum range, 125 kHz Bandwidth, Coding Rate 4/7, +20 dBm transmit power).

### Module 2: Over-The-Air Packet Protocol (`packet_protocol.h`)
* Defines the compact **28-byte packed binary struct** (`__attribute__((packed))`).
* Eliminates heavy string parsing; packs GPS coordinates as `int32_t` (`lat * 1e6`).
* Computes and verifies **CRC16** on every incoming radio wave to reject atmospheric radio noise.

### Module 3: Button & User Event Manager (`button_manager.h/.cpp`)
* Implements a non-blocking state machine polled every 20 ms.
* **Black Button (NAV):**
  * `SINGLE CLICK`: Cycles to the next screen (Home ➔ Menu ➔ Radar ➔ Home).
* **Red Button (ACTION/SOS):**
  * `SINGLE CLICK`: Selects and broadcasts the currently highlighted menu message.
  * `HOLD (3 SECONDS)`: Fires an immediate **EMERGENCY PANIC OVERRIDE** from any screen, silencing menus and blasting priority distress beacons.

### Module 4: GPS & Navigational Mathematics (`gps_manager.h/.cpp`)
* Interfaces with the NEO-6M on `HardwareSerial2` (GPIO 4 RX, GPIO 15 TX) at 9600 baud using `TinyGPS++`.
* **Haversine Distance Algorithm:** Calculates real-time distance in meters between current GPS and target's received coordinates.
* **Compass Bearing & Azimuth Algorithm:** Computes the relative angle (0° to 360°) and determines the cardinal direction & arrow symbol (`↑`, `↗`, `→`, `↘`, `↓`, `↙`, `←`, `↖`).

### Module 5: LoRa Radio & ACK Handshake (`radio_manager.h/.cpp`)
* Utilizes **RadioLib** (or Arduino-LoRa fallback) for low-level register control on SX1278.
* Uses the **DIO0 hardware interrupt** (GPIO 2) so the ESP32 wakes up the radio task the microsecond a packet arrives.
* **Auto-ACK Engine:** If an incoming message is an SOS, the radio automatically bounces back an acknowledgment packet with its own Node ID.
* **Retry Loop:** If the sender doesn't receive an ACK within 2,000 ms, it automatically retransmits up to 3 times.

### Module 6: Display & Screen Renderer (`display_manager.h/.cpp`)
* Controls the SSD1306 128×64 OLED display using `Adafruit_SSD1306` or `U8g2`.
* **Screen 1 (Home):** Battery icon, satellite counter, current lat/lon coordinates.
* **Screen 2 (Emergency Carousel):** Scrollable preset triage cards (*Medical*, *Supplies*, *Safe*, *Evacuating*).
* **Screen 3 (Rescue Radar):** Large distance readout (`350 METERS`) and a bold directional arrow pointing to the victim.
* **Screen 4 (Alert Popup):** Inverted black/white flashing screen with buzzer beeps when an incoming SOS arrives.
* **Power Saver:** Turns the display off after 20 seconds of idle time.

---

## 3. FreeRTOS Concurrency Architecture

We will set up two dedicated RTOS tasks pinned to the ESP32's dual cores:

```
           CORE 0                                CORE 1
┌───────────────────────────┐         ┌───────────────────────────┐
│       Task_RadioGPS       │         │          Task_UI          │
├───────────────────────────┤         ├───────────────────────────┤
│ • Listens on DIO0 IRQ     │         │ • 20ms button debounce    │
│ • Feeds TinyGPS++ parser  │         │ • 15 FPS OLED refresh     │
│ • Handles ACK replies     │         │ • Screen sleep timer      │
│ • Distance/Bearing math   │         │ • Buzzer melody generator │
└─────────────┬─────────────┘         └─────────────▲─────────────┘
              │                                     │
              └────── FreeRTOS Event Queue ─────────┘
                      (Posts "Incoming SOS"
                       or "ACK Received" to UI)
```

---

## 4. Required Libraries

All libraries are standard and easily installed via the Arduino IDE Library Manager or PlatformIO:

1. **`RadioLib`** (by Jan Gromes) — High-reliability LoRa driver for SX1278.
2. **`TinyGPS++`** (by Mikal Hart) — Efficient NMEA GPS parser.
3. **`Adafruit_SSD1306`** & **`Adafruit_GFX`** — Core graphics driver for 128x64 I2C OLED.

---

## 5. Step-by-Step Implementation Steps

We will build and deliver the code in 4 orderly steps:

* **Step 1:** Create `config.h` and `packet_protocol.h` (pin assignments, binary packet struct, CRC).
* **Step 2:** Build `button_manager` and `gps_manager` (user input debouncing and distance/bearing navigation math).
* **Step 3:** Build `radio_manager` (LoRa packet handling, hardware interrupts, two-way ACK handshake).
* **Step 4:** Build `display_manager` and the main `lora_spotter.ino` orchestration sketch with the FreeRTOS tasks.
