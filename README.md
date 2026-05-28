# Robot-Dog-for-Bomb-Detection-using-ESP32-FreeRTOS

## 📌 Project Overview

This project presents a **Robot Dog for Bomb Detection** using an **ESP32 dual-core microcontroller** and **FreeRTOS**. The system is designed as a low-cost embedded prototype for detecting possible hazardous conditions using gas sensing, metal detection, GPS geo-tagging, and live wireless monitoring.

The robot integrates an **MQ-2 gas sensor**, an **inductive metal detector**, and a **Neo-6M GPS module**. Sensor data is processed using multiple FreeRTOS tasks and transmitted in real time to a web dashboard hosted directly by the ESP32 through its built-in Wi-Fi Access Point.

---

## 🎯 Objectives

- Detect carbon monoxide and methane/LPG gas concentration using the MQ-2 sensor.
- Detect metallic objects using an inductive metal detector module.
- Activate a buzzer or LED alarm when metal is detected.
- Capture GPS location data using the Neo-6M GPS module.
- Display real-time sensor data on a WebSocket-based dashboard.
- Demonstrate multitasking using FreeRTOS on ESP32.
- Use queue-based inter-task communication for reliable real-time performance.

---

## 🧠 Key Features

- ESP32-based real-time embedded system
- FreeRTOS multitasking architecture
- Dual-core task separation
- MQ-2 gas detection for CO and methane/LPG
- Inductive metal detection
- GPS-based threat geo-tagging
- Local alarm output using buzzer/LED
- ESP32 Wi-Fi Access Point mode
- Live WebSocket dashboard
- Real-time telemetry visualization
- Queue-based sensor-to-web data transfer

---

## 🧩 Hardware Components

| Component | Description |
|---|---|
| ESP32 | Main controller with dual-core FreeRTOS support |
| MQ-2 Gas Sensor | Detects CO, methane, LPG and smoke-related gases |
| Inductive Metal Detector | Detects nearby metallic objects |
| Neo-6M GPS Module | Provides latitude, longitude and satellite count |
| Buzzer / LED | Alarm output for metal detection |
| Power Supply | Battery or regulated DC supply |
| Robot Dog Frame | 12-DOF quadruped structure |

---

## 🔌 Pin Configuration

| Module | ESP32 Pin |
|---|---|
| MQ-2 Analog Output | GPIO34 |
| Metal Detector Output | GPIO25 |
| Alarm Buzzer / LED | GPIO26 |
| GPS TX | GPIO16 |
| GPS RX | GPIO17 |

---

## ⚙️ Software and Libraries Used

- Arduino IDE / PlatformIO
- ESP32 Board Package
- FreeRTOS
- TinyGPS++
- ESPAsyncWebServer
- AsyncTCP
- SPIFFS
- WebSocket communication

---

## 🏗️ System Architecture

The system uses four main FreeRTOS tasks:

| Task | Priority | Core | Function |
|---|---:|---:|---|
| Telemetry Task | 5 | Core 0 | Serial visualization of system status |
| Web Task | 4 | Core 0 | Sends JSON data to WebSocket dashboard |
| Sensor Task | 3 | Core 1 | Reads MQ-2, metal detector and alarm status |
| GPS Task | 2 | Core 1 | Reads GPS UART data and decodes NMEA messages |

The Sensor Task collects gas, metal and GPS data, stores it in a `TacticalData` structure, and sends it to the Web Task using a FreeRTOS queue.

---

## 📡 Web Dashboard

The ESP32 creates its own Wi-Fi Access Point.

```text
SSID: DOG
Password: 12345678
Dashboard IP: 192.168.4.1
