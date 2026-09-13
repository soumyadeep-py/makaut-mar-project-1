# 🔌 Smart Indoor Plant Monitoring System — Schematics

This folder contains the electrical schematic and wiring information for the **Smart Indoor Plant Monitoring System** based on the **ESP32 WROOM-32**.

## 📐 System Schematic

The complete wiring diagram is available below:

![Smart Plant Monitoring System Schematic](smart_plant_monitoring_schematic.png)

## 🔧 Main Connections

| Component                            | ESP32 Connection |
| ------------------------------------ | ---------------- |
| Capacitive Soil Moisture Sensor (AO) | GPIO 34          |
| AHT21B SDA                           | GPIO 21          |
| AHT21B SCL                           | GPIO 22          |
| BH1750 SDA                           | GPIO 21          |
| BH1750 SCL                           | GPIO 22          |
| OLED SDA                             | GPIO 21          |
| OLED SCL                             | GPIO 22          |
| Push Button                          | GPIO 27          |
| RGB LED Red                          | GPIO 25          |
| RGB LED Green                        | GPIO 26          |
| RGB LED Blue                         | GPIO 33          |
| Passive Buzzer                       | GPIO 14          |
| OLED I²C Address                     | `0x3C`           |

## 🔗 I²C Bus

The **AHT21B, BH1750, and OLED display** share the same I²C bus.

```text
ESP32 GPIO 21 (SDA)
        │
        ├── AHT21B SDA
        ├── BH1750 SDA
        └── OLED SDA

ESP32 GPIO 22 (SCL)
        │
        ├── AHT21B SCL
        ├── BH1750 SCL
        └── OLED SCL
```

## ⚡ Power

The sensor modules and OLED should be powered according to their module specifications. For typical ESP32-compatible breakout boards:

```text
ESP32 3.3V ─── VCC
ESP32 GND ──── GND
```

The capacitive soil moisture sensor's **analog output (AO)** is connected to **GPIO 34**, which is an ESP32 ADC input.

## 💡 RGB LED

The RGB LED is used to indicate the current system status.

```text
GPIO 25 ─── Red
GPIO 26 ─── Green
GPIO 33 ─── Blue
```

> ⚠️ Use appropriate current-limiting resistors for the RGB LED.

## 🔊 Buzzer

```text
GPIO 14 ─── Buzzer signal
GND      ─── Buzzer GND
```

If your buzzer module requires more current than an ESP32 GPIO can safely provide, use an appropriate transistor driver.

## 🔘 Push Button

The push button is connected to:

```text
GPIO 27
```

It can be used to wake or activate the Wi-Fi access point according to the system firmware.

## 📝 Notes

* GPIO 34 is an **input-only ADC pin** on the ESP32.
* AHT21B, BH1750, and OLED use the shared I²C bus.
* The OLED uses I²C address `0x3C` in this project.
* Avoid connecting 5V signals directly to ESP32 GPIO pins.
* Use proper resistors with discrete/common-cathode RGB LEDs.
* Verify the pinout of your specific sensor modules before powering the circuit.

## 📁 Files

```text
schematics/
├── README.md
└── smart_plant_monitoring_schematic.png
```

---

**Project:** Smart Indoor Plant Monitoring System
**Platform:** ESP32 WROOM-32
**Author:** Tamaghna Basu
**Field:** Electronics & Communication Engineering (ECE)
