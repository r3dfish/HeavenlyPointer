# Stack-chan — Kawaii Co-created Open-source AI Desktop Robot

> A pocket-sized, expressive AI desktop companion built on the M5Stack **CoreS3** (ESP32-S3).
> Stack-chan talks, looks around, sees, listens, and reacts with lively facial animations —
> and it's fully open-source and hackable across UiFlow2, Arduino, PlatformIO, and ESP-IDF.

**Product page:** [M5Stack Shop — Stack-chan](https://shop.m5stack.com/products/stackchan-kawaii-co-created-open-source-ai-desktop-robot)
**Official docs:** [docs.m5stack.com/en/StackChan](https://docs.m5stack.com/en/StackChan)
**Price:** $99.00 USD
**Powered by:** M5Stack CoreS3 (pre-installed)

---

> 🛰️ **This is a hardware reference** for the device that **HeavenlyPointer** runs
> on. For the firmware itself — features, build, flashing, and usage — see the
> main **[project README](../README.md)**.

---

## Table of Contents

- [What Stack-chan Can Do](#what-stack-chan-can-do)
- [Hardware & Chipsets](#hardware--chipsets)
- [Technical Specifications](#technical-specifications)
- [Connectivity & I/O](#connectivity--io)
- [Software & Development](#software--development)
- [Package Contents](#package-contents)
- [Notes & Cautions](#notes--cautions)

---

## What Stack-chan Can Do

### 🤖 AI & Conversation
- **AI Agent** for natural, conversational interaction
- **Voice question & answer** — ask it things and hear it respond
- **Cloud LLM connectivity** — connects to large language models in the cloud
- **StackFlow AI** integration for building AI-driven behaviors

### 😄 Expression & Personality
- **Lively, expressive facial animations** rendered on the 2.0" display
- **12 RGB LEDs** (two rows) for ambient visual feedback and mood
- Character expression stickers to customize its look

### 🔄 Movement
- **360° continuous rotation** on the horizontal (X) axis via feedback servo
- **90° tilt movement** on the vertical (Y) axis via feedback servo
- Recommended Y-axis range of **5°–85°** to avoid servo strain

### 👀 Vision & 🎙️ Audio
- **Onboard camera** for video viewing and seeing its surroundings
- **Dual microphones** for voice capture and audio interaction
- **1W speaker** for speech and sound output

### 📡 Interaction & Control
- **ESP-NOW wireless remote control**
- **Mobile app integration** — video viewing, remote avatar control, and app downloads
- **Three-zone touch panel** interface
- **NFC** wireless interaction
- **Infrared** transmit & receive (control IR devices / receive remote signals)

### 🏠 Applications
- Desktop companion / virtual pet
- Smart-home and IoT control platform
- Storytelling and entertainment

### 🔧 Maintenance
- **Over-the-air (OTA) online updates**
- WiFi & Bluetooth connectivity

---

## Hardware & Chipsets

Stack-chan is driven by the **M5Stack CoreS3**. Below is the full chipset breakdown.

### Core Processor & Memory
| Component | Part / Chip | Details |
|---|---|---|
| **Main SoC** | **ESP32-S3** | Xtensa dual-core 32-bit LX7 @ 240 MHz |
| **Flash** | 16 MB | |
| **PSRAM** | 8 MB Quad PSRAM | |

### Display & Touch
| Component | Part / Chip | Details |
|---|---|---|
| **Display** | 2.0" IPS LCD | 320 × 240, 65,536 colors |
| **Display driver** | **ILI9342C** | |
| **Touch controller** | **FT6336U** | Capacitive multi-touch |

### Camera
| Component | Part / Chip | Details |
|---|---|---|
| **Camera sensor** | **GC0308** | 640 × 480, 0.3 MP |

### Audio
| Component | Part / Chip | Details |
|---|---|---|
| **Audio codec** | **ES7210** | Dual-microphone input codec |
| **Speaker amplifier** | **AW88298** | 16-bit I²S power amplifier |
| **Speaker** | 1 W | |
| **Microphones** | Dual mic array | |

### Sensors
| Component | Part / Chip | Details |
|---|---|---|
| **Proximity & ambient light** | **LTR-553ALS-WA** | |
| **9-axis IMU** | **BMI270 + BMM150** | Accelerometer, gyroscope, magnetometer |
| **Three-zone touch panel** | **Si12T** driver | |

### Wireless, NFC & Infrared
| Component | Part / Chip | Details |
|---|---|---|
| **Wi-Fi** | 2.4 GHz, IEEE 802.11 b/g/n | |
| **Bluetooth** | Bluetooth® 5 LE | |
| **NFC** | **ST25R3916** | Full-featured NFC |
| **IR receiver** | **IRM56384** | |
| **IR transmitter** | Onboard IR LED | |

### Power Management
| Component | Part / Chip | Details |
|---|---|---|
| **Power management IC** | **AXP2101** | |
| **Real-time clock** | **BM8563** | |
| **Battery** | 550 mAh internal | |

### RGB Lighting
| Component | Part / Chip | Details |
|---|---|---|
| **RGB LEDs** | **WS2812C** ×12 | Two rows, individually addressable |

### Servos / Motors
| Component | Details |
|---|---|
| **Horizontal (X) servo** | 360° continuous rotation, with position feedback |
| **Vertical (Y) servo** | 90° movement, with position feedback |

---

## Technical Specifications

| Specification | Details |
|---|---|
| **CPU** | ESP32-S3, dual-core Xtensa LX7 @ 240 MHz |
| **Memory** | 16 MB Flash, 8 MB Quad PSRAM |
| **Display** | 2.0" IPS LCD, 320 × 240, capacitive multi-touch |
| **Camera** | 0.3 MP (640 × 480), GC0308 |
| **Wireless** | Wi-Fi 802.11 b/g/n, Bluetooth 5 LE |
| **Audio** | 1 W speaker, dual microphones |
| **LEDs** | 12 × WS2812C RGB |
| **Battery** | 550 mAh (internal) |
| **Power input** | USB-C |
| **Dimensions** | 54.0 × 70.5 × 61.5 mm |
| **Weight** | 187.0 g (Stack-chan unit) |

---

## Connectivity & I/O

- **USB-C** (CDC & OTG)
- **GPIO**, **UART**, **I²C**
- **microSD card slot**
- **3 × Grove interfaces**
- **LEGO-compatible mounting holes**
- **Wi-Fi** 2.4 GHz (802.11 b/g/n)
- **Bluetooth 5 LE**
- **ESP-NOW** wireless protocol
- **NFC**
- **Infrared** TX/RX

---

## Software & Development

Stack-chan is fully open-source and supports multiple development environments:

- **UiFlow2** — block & Python visual programming
- **Arduino IDE**
- **PlatformIO**
- **ESP-IDF** — Espressif's native framework
- **StackFlow AI** — integration for AI agent workflows
- **Mobile app** — remote control, video viewing, and app downloads
- **OTA** online updates
- Online documentation available from M5Stack

---

## Package Contents

- 1 × Carrying bag or box
- 1 × Stack-chan unit (with **CoreS3 pre-installed**)
- 1 × 50 cm USB-A to USB-C cable
- 1 × Character expression sticker
- 1 × Printed user manual

---

## Notes & Cautions

- ⚠️ **Servo limits:** Keep the Y-axis (vertical tilt) within **5°–85°** to prevent damaging the servo.
- 🔌 Charge and program over **USB-C**.
- 🔋 The internal **550 mAh** battery supports untethered desktop use.

---

*This README documents the Stack-chan as described on the official M5Stack product page. Part numbers and specifications are quoted from that listing.*
