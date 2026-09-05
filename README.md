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


# 👨‍💻 About Me

Hi! I'm **Soumyadeep Samanta**, a first-year **B.Tech student in Computer Science and Engineering**.

I am interested in **programming, embedded systems, IoT, and developing practical technology-based projects**. Through this project, I am exploring the integration of hardware, sensors, embedded programming, and web technologies using the ESP32.

This project is part of my learning journey, where I aim to strengthen my skills in:
- 💻 C/C++ programming
- 🔌 Embedded systems
- 🌐 Web development
- 📡 IoT and wireless communication
- 🧩 Hardware-software integration
- 🌱 Smart monitoring systems

I enjoy learning by building projects and experimenting with different technologies.
