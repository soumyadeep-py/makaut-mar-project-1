# 🌱 Smart Plant Monitoring System using ESP32

A smart indoor plant monitoring system based on **ESP32** that monitors soil moisture, temperature, humidity, and ambient light. The system displays the sensor information on an OLED display and provides a web-based dashboard hosted directly by the ESP32.

The system is designed to reduce unnecessary sensor polling by operating the sensors in a **time-multiplexed sensing cycle** during normal operation. If an abnormal condition is detected, the system switches to **Alert Mode** and monitors the sensors more frequently.


## 📌 Features

- 🌱 Soil moisture monitoring
- 🌡️ Temperature monitoring
- 💧 Humidity monitoring
- ☀️ Ambient light monitoring
- 📺 0.96" OLED status display
- 🌐 Web dashboard hosted directly by ESP32
- 📡 ESP32 operates as a Wi-Fi Access Point
- 🔄 Time-multiplexed sensor monitoring
- 🚨 Automatic Alert Mode when threshold conditions are detected
- 💡 RGB LED system-status indication
- 🔊 Buzzer notification
- 🔘 Push-button to wake the Wi-Fi access point
- 💾 Web files stored using ESP32 LittleFS
- 📱 Dashboard can be accessed from a phone or computer connected to the ESP32 Wi-Fi network

---

# 🧰 Hardware Required

| Component | Quantity |
|---|---:|
| ESP32 WROOM-32 Development Board | 1 |
| Capacitive Soil Moisture Sensor | 1 |
| AHT21B Temperature & Humidity Sensor | 1 |
| BH1750 Ambient Light Sensor | 1 |
| 0.96" OLED Display | 1 |
| Common-Cathode RGB LED | 1 |
| Passive Buzzer | 1 |
| Push Button | 1 |
| Connecting Wires | As required |

---

# 🔌 Pin Configuration

| Component | ESP32 Pin |
|---|---|
| Soil Moisture Sensor | GPIO 34 |
| I2C SDA | GPIO 21 |
| I2C SCL | GPIO 22 |
| Push Button | GPIO 27 |
| RGB LED - Red | GPIO 25 |
| RGB LED - Green | GPIO 26 |
| RGB LED - Blue | GPIO 33 |
| Passive Buzzer | GPIO 14 |
| OLED I2C Address | 0x3C |

The AHT21B, BH1750 and OLED share the same I2C bus:

```text
ESP32 GPIO 21 ───── SDA ───── AHT21B
                           ├── BH1750
                           └── OLED

ESP32 GPIO 22 ───── SCL ───── AHT21B
                           ├── BH1750
                           └── OLEDz
```

--- 

# 📂 File Structure
```text
D:.
└───code
    │   code.ino
    │   
    └───data
            index.html
            script.js
            style.css
```


# Hi, I'm Tamaghna Basu
**First Year B.Tech Student | Electronics & Communication Engineering (ECE)**

I am a first-year **B.Tech ECE student** with a strong interest in **electronics, embedded systems, microcontrollers, sensors, and hardware-based technology**. I enjoy understanding how electronic circuits work and turning ideas into practical projects by combining hardware and programming.

🔧 I am particularly interested in exploring **Arduino, ESP32, sensors, digital electronics, analog circuits, communication systems, and embedded programming**.

## ⚡ Areas I'm Exploring

* 🔌 Electronic Circuits & Circuit Design
* 🤖 Microcontrollers & Embedded Systems
* 📟 Arduino & ESP32 Development
* 🌡️ Sensors & Instrumentation
* 💻 C/C++ Programming
* 📡 Communication & IoT Systems
* 🔋 Power Electronics & Basic Power Systems
* 🧠 Digital & Analog Electronics
* 🔧 Hardware Debugging & Prototyping
* 🌱 Smart Electronics & Automation

## 🛠️ Skills I'm Building

Through academic work and personal projects, I am working on strengthening my skills in:

* **C / C++**
* **Arduino & ESP32**
* **Sensor Interfacing**
* **GPIO, ADC & PWM**
* **I2C & Serial Communication**
* **Circuit Design & Prototyping**
* **Embedded Programming**
* **PCB & Electronics Fundamentals**
* **Hardware–Software Integration**

## 🚀 My Learning Journey

I believe the best way to learn electronics is by **building, testing, debugging, and experimenting**.

I use my projects to understand how components such as **microcontrollers, sensors, LEDs, displays, buzzers, and communication modules** work together to create useful electronic systems.

Currently, I am focusing on developing a strong foundation in **electronics and embedded systems** while gradually exploring more advanced areas of ECE.

## 📚 Future Goals

I aim to explore areas such as:

* Embedded Systems
* Robotics
* IoT
* VLSI & Digital Electronics
* Wireless Communication
* PCB Design
* Automation & Control Systems
* Advanced Microcontroller Applications

> **"Learn the fundamentals. Build something. Break it. Debug it. Build it better."**

Thanks for visiting my GitHub! ⚡

