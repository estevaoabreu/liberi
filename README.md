# Liberi – Vital Sign Baby Monitor

This project was developed for the "Interface Technologies" course unit of the Master's in Design and Multimedia of the Faculty of Sciences and Technology of the University of Coimbra. This project was made by Estêvão Abreu, Mariana Silva and Silas Sequeira.

Liberi is a real-time baby vital sign monitoring system designed to track a baby's temperature, heart rate, and oxygen level (SpO2). The system streams this biometric data to an interactive web dashboard that features a personalized onboarding experience and dynamic status indicators (Normal, Abnormal, concerning) to alert caregivers if readings deviate from standard health thresholds.

This project supports two execution setups:

1. **Physical Setup (ESP32)**: A fully wireless device utilizing an ESP32 microcontroller, physical I2C sensors, a local web server (LittleFS), and optional Node.js data forwarding.
2. **Simulation Setup (Wokwi)**: A virtual prototype utilizing an Arduino Uno running in the Wokwi simulator with custom WebAssembly-simulated I2C sensors and a serial-to-dashboard Node.js bridge.

For details on the power system, microcontroller, breadboard, and sensor connections, please refer to [circuit_diagram.md](./circuit_diagram.md).

---

## Setup Variation 1: Wokwi Simulation (Arduino Uno)

This setup runs a simulated Arduino Uno inside the Wokwi simulator interface. Sensor data is simulated by custom WebAssembly logic.

### Hardware Emulated

- **Arduino Uno**
- **RGB LED** (Pins: Red=9, Green=10, Blue=11)
- **Push Button** (Pin 2, active LOW with internal pull-up)
- **MLX90614** (I2C address `0x5A`)
- **MAX30102** (I2C address `0x57`)

### Prerequisites

1. **VS Code** with the **PlatformIO IDE** extension installed.
2. The **Wokwi Simulator** VS Code extension.
3. **Node.js** (v16+) installed on your PC.

### Configuration & Build

1. In the project root, open your terminal and install Node dependencies:
   ```bash
   npm install
   ```
2. Build the Arduino Uno firmware using PlatformIO:
   ```bash
   pio run
   ```
   This will compile the project defined in [platformio.ini](./platformio.ini) and write the firmware outputs to `.pio/build/uno/firmware.hex` (loaded automatically by Wokwi).

### Running the Simulation

1. Press `F1` in VS Code and select **Wokwi: Start Simulator**. (The simulator will open and load [diagram.json](./diagram.json) and [wokwi.toml](./wokwi.toml)).
2. Wokwi starts a virtual RFC2217 serial server on TCP port `4000`.
3. To bridge this virtual serial data to your local web browser, open a new terminal in the project root and run:
   ```bash
   node server_arduino.js
   ```
   _(Note: By default, `server_arduino.js` connects to serial port `COM16`. If your system utilizes a different virtual COM port bridged to Wokwi, update line 8 in [server_arduino.js](./server_arduino.js))._
4. Open your web browser and navigate to `http://localhost:3000`.
5. Press the simulated button on the Wokwi schematic to turn the system "ON". Data will start streaming to your dashboard.

---

## Setup Variation 2: Physical Device (ESP32)

This setup deploys a multi-threaded C++ firmware onto a physical ESP32 microcontroller that streams sensor readings wirelessly over Wi-Fi or directly via USB.

### Hardware Architecture & Wiring

The physical device uses an ESP-32D, real sensors, a push button, and an RGB LED. It also includes a Li-Po battery charging circuit:

- **ESP-32D Microcontroller**
- **TP4056 USB-C Charger** + **Li-Po Battery** (3.7V) + **Step-Up Boost Converter** (to supply constant 5V to the ESP32's `VIN` pin)
- **Adafruit MLX90614** (I2C: SDA=GPIO 21, SCL=GPIO 22)
- **SparkFun MAX30102** (I2C: SDA=GPIO 21, SCL=GPIO 22)
- **RGB LED** (Red=GPIO 27, Green=GPIO 26, Blue=GPIO 25)
- **Push Button** (GPIO 17, active LOW with internal pull-up)

### Prerequisites

1. **Arduino IDE** (v2.0+) or VS Code with the PlatformIO extension.
2. **ESP32 Board Package** installed in Arduino IDE (`https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`).
3. Installed Arduino Libraries:
   - **ESPAsyncWebServer** and **AsyncTCP** (for hosting the web server and SSE streams)
   - **Adafruit MLX90614 Library**
   - **SparkFun MAX3010x Pulse and Proximity Sensor Library**

### Installation & Configuration

1. Open the [arduino_ide/LiberiMonitor](./arduino_ide/LiberiMonitor) directory.
2. Copy `secrets.h.example` to `secrets.h` inside the `LiberiMonitor` directory:
   ```bash
   cp secrets.h.example secrets.h
   ```
3. Open `secrets.h` and configure:
   ```cpp
   #define WIFI_SSID "YOUR_WIFI_SSID"
   #define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
   #define LOCAL_SERVER_HOST "YOUR_PC_LOCAL_IP_ADDRESS" // e.g., "192.168.1.15"
   ```

### Uploading Code and Assets

1. **Upload Sketch**: Open [LiberiMonitor.ino](./arduino_ide/LiberiMonitor/LiberiMonitor.ino) in Arduino IDE, select your ESP32 board model (e.g., `ESP32 Dev Module`) and port, and click **Upload**.
2. **Upload LittleFS Filesystem Data**:
   - The ESP32 serves the dashboard locally from its internal storage using LittleFS.
   - Place the compiled static web files (or copy the files from `arduino_ide/LiberiMonitor/data/`) into the `data` folder.
   - Use the **ESP32 Sketch Data Upload** tool in Arduino IDE (or equivalent CLI command) to compile the LittleFS partition and flash it to the ESP32.

### Running & Dashboard Modes

Once uploaded, press the push button on GPIO 17 to start monitoring. You have three ways to view the dashboard:

#### Mode A: Standalone Wireless Web Server (No PC Server Required)

- The ESP32 hosts its own web server on port 80.
- When connected to your local Wi-Fi, it prints its assigned IP address to the Serial monitor. Open that IP (e.g., `http://192.168.1.50/`) in your browser.
- **AP Fallback Mode**: If Wi-Fi fails to connect, the ESP32 hosts its own Access Point named `LiberiMonitor` (password: `12345678`). Connect your phone or PC to this Wi-Fi network and open `http://192.168.4.1/`.

#### Mode B: Local Node.js Hub (Recommended)

1. On your PC, navigate to the project root and start the Node.js server:
   ```bash
   node server.js
   ```
2. Open `http://localhost:3000` in your web browser.
3. The ESP32 will parse and process vitals, then make asynchronous POST requests to `http://<your-pc-ip>:3000/api/data`, which is forwarded to the browser dashboard via Server-Sent Events (SSE).

#### Mode C: Web Serial Direct Connection (Browser Serial)

1. Connect the ESP32 to your PC using a USB cable.
2. Open the dashboard (from your local Node.js server, ESP32 local server, or simply opening `index.html` in Chrome/Edge).
3. Click the **"Connect to ESP32"** button on the dashboard interface.
4. Select the serial port assigned to your ESP32. The dashboard will process raw serial telemetry lines directly.
