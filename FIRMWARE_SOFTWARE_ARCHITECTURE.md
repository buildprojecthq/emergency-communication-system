# Firmware & Software Architecture Plan
## ESP32-LoRa Emergency Communication "Spotter" System (Standalone Multi-Node)

---

## 1. System Overview & Philosophy

The system consists of two or more identical, autonomous handheld field communicators (**Spotter 1, Spotter 2, Spotter 3...**), running the same codebase but distinguished by a unique hardware Node ID (`0x01`, `0x02`, `0x03`...).

### Key Principles
1. **100% Offline & Standalone:** Zero internet, zero cellular network, zero laptop dashboard needed. Everything operates directly between the pocket devices via direct LoRa radio waves.
2. **Emergency Broadcast (All Units Alerted):** An SOS distress beacon is received by **every Spotter in radio range**.
3. **Radio-Only Delivery Receipt (Zero-Internet ACK):** When any Spotter receives an SOS, it automatically bounces back a radio acknowledgment. The victim's screen confirms: `✔ SOS HEARD BY RESCUER (Node X)`.
4. **Rescue Radar (Hardware-Only Compass):** Calculates real-time distance in meters and draws a direction arrow (`↗`, `→`, etc.) pointing rescuers directly to the victim.
5. **Ultra-Simple 2-Button UI:** 
   * **Black Button (NAV):** Cycles through Home, Emergency Presets, and Rescue Radar.
   * **Red Button (ACTION/SOS):** Sends selected message. **Holding for 3 seconds blasts an instant PANIC SOS to all units.**

---

## 2. Hardware Pinout & Bus Mapping

| Peripheral | Chip / Module | ESP32 GPIO | Bus / Protocol | Configuration |
| :--- | :--- | :--- | :--- | :--- |
| **LoRa Transceiver** | SX1278 (Ra-02) | **GPIO 5** | SPI (NSS / CS) | Active Low |
| | | **GPIO 18** | SPI (SCK) | Clock |
| | | **GPIO 19** | SPI (MISO) | Master In |
| | | **GPIO 23** | SPI (MOSI) | Master Out |
| | | **GPIO 2** | Interrupt (DIO0) | Rising Edge (Packet Ready) |
| | | **GPIO 14** | Reset (RST) | Active Low |
| **GPS Receiver** | u-blox NEO-6M | **GPIO 4** | UART2 (RX) | ESP32 RX <- GPS TX (9600 baud) |
| | | **GPIO 15** | UART2 (TX) | ESP32 TX -> GPS RX |
| **OLED Display** | 0.96" SSD1306 | **GPIO 22** | I2C (SCL) | 400 kHz Fast I2C |
| | | **GPIO 21** | I2C (SDA) | |
| **SOS / Action Button** | Tactile Switch | **GPIO 32** | Digital Input | Internal `INPUT_PULLUP` to GND |
| **NAV / Menu Button** | Tactile Switch | **GPIO 33** | Digital Input | Internal `INPUT_PULLUP` to GND |
| **Alert Buzzer / LED** | Active Buzzer | **GPIO 27** | Digital Output | Audio alarm on SOS reception |

---

## 3. Dual-Core Concurrency Model (FreeRTOS)

To ensure the device never freezes or drops radio signals while redrawing the screen:

* **Core 0 (Radio & GPS Core):**
  * `Task_LoRa`: Interrupt-driven (DIO0) listening for incoming radio waves. Handles broadcasting, parsing, and auto-replying with ACK receipts.
  * `Task_GPS`: Reads serial stream from the NEO-6M module, decodes NMEA coordinates, and keeps current position updated.
* **Core 1 (User Interface Core):**
  * `Task_Buttons`: 20 ms polling with non-blocking click, double-click, and 3-second long-press detection.
  * `Task_Display`: 15 FPS OLED screen renderer. Manages screen dimming/sleep timeout after 20 seconds.

---

## 4. Multi-Node Radio Protocol (Binary Format)

Every transmission is packed into a compact **28-byte binary packet** (takes ~85 ms on air):

```
+---------------+---------------+---------------+---------------+
| Magic (0xAA)  | Network (0x42)| Source Node ID| Dest Node ID  |
+---------------+---------------+---------------+---------------+
|  Seq Number   | Message Type  | Fix / Sats    | Battery %     |
+---------------+---------------+---------------+---------------+
|                    Latitude (int32_t * 1e6)                   |
+---------------+---------------+---------------+---------------+
|                   Longitude (int32_t * 1e6)                   |
+---------------+---------------+---------------+---------------+
|       Altitude (int16_t)      |     Preset Message Code       |
+---------------+---------------+---------------+---------------+
|                       CRC16 Checksum                          |
+---------------------------------------------------------------+
```

* **Destination `0xFF`:** Broadcasts to **ALL** Spotters in range.
* **Message Types:**
  * `0x01` = **EMERGENCY_SOS** (Medical, Trapped, Critical)
  * `0x02` = **STATUS_UPDATE** (Supplies, Safe, Evacuating)
  * `0x03` = **ACK_RECEIPT** (Invisible return receipt confirming message delivery)

---

## 5. Screen Navigation (3 Screens Only)

```
                    ┌─────────────────────────┐
                    │     1. HOME SCREEN      │
                    │ Lat: 12.9234 Lon: 77.501│
                    │ Sats: 8  |  Bat: 90%    │
                    └────────────┬────────────┘
                                 │
             Single Click [NAV]  │  ▲ Single Click [NAV] (wraps around)
                                 ▼  │
                    ┌─────────────────────────┐
                    │   2. EMERGENCY MENU     │
                    │ > [1] MEDICAL CRISIS    │
                    │   [2] NEED FOOD/WATER   │
                    │   [3] SAFE / STANDBY    │
                    └────────────┬────────────┘
                                 │
             Single Click [NAV]  │
                                 ▼
                    ┌─────────────────────────┐
                    │   3. RESCUE RADAR MODE  │
                    │ Target: SPOTTER 1       │
                    │ Distance: 350 METERS    │
                    │ Direction: NORTHEAST ↗  │
                    └─────────────────────────┘

========================= GLOBAL EMERGENCY =========================
- HOLD RED BUTTON 3 SECONDS:
    --> Instant Emergency Override from ANY screen!
    --> Screen flashes, buzzer blares.
    --> Transmits EMERGENCY_SOS broadcast to ALL Spotters.
- SINGLE CLICK RED BUTTON in Menu:
    --> Transmits highlighted message to ALL Spotters.
```

---

## 6. Rescue Radar Calculation (Onboard Mathematics)

When Spotter 2 receives an alert from Spotter 1:
1. **Distance (Haversine Formula):** The ESP32 compares Spotter 1's coordinates (`lat1, lon1`) with its own GPS (`lat2, lon2`) to calculate straight-line distance in meters:
   $$d = 2R \cdot \arcsin\left(\sqrt{\sin^2\left(\frac{\Delta\phi}{2}\right) + \cos(\phi_1)\cos(\phi_2)\sin^2\left(\frac{\Delta\lambda}{2}\right)}\right)$$
2. **Direction (Azimuth Formula):** Calculates the angle $\theta$ (0° to 360°) from Spotter 2 to Spotter 1.
3. **Display:** Converts $\theta$ into a directional arrow (`↑`, `↗`, `→`, `↘`, `↓`, `↙`, `←`, `↖`) pointing the rescuer directly towards the victim.
