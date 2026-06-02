# Liberi – Vital Sign Baby Monitor

This project was developed for the "Interface Technologies" course unit of the Master's in Design and Multimedia of the Faculty of Sciences and Technology of the University of Coimbra. This project was made by Estêvão Abreu, Mariana Silva and Silas Sequeira.

Liberi is a real-time baby vital sign monitoring system designed to track a baby's wrist temperature, heart rate, and oxygen level (SpO2). The system streams this biometric data directly to an interactive web dashboard via the **Web Serial API**, completely eliminating the need for complex Wi-Fi setups. The dashboard features a personalized onboarding experience, **Web Push Notifications** for emergency alerts, and dynamic status indicators to alert caregivers if readings deviate from standard age-specific health thresholds.

This project supports two execution setups:

1. **Physical Setup (ESP32)**: A device utilizing an ESP32 microcontroller and physical I2C sensors. It streams raw data simultaneously over a wired USB connection or wirelessly via a virtual Bluetooth Classic COM port.
2. **Simulation Setup (Wokwi)**: A virtual prototype utilizing an Arduino Uno running in the Wokwi simulator with custom WebAssembly-simulated I2C sensors.

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

This setup deploys a C++ firmware onto a physical ESP32 microcontroller that streams sensor readings to your browser dashboard directly via **USB-C** or wirelessly via **Bluetooth Classic**.

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
   - **Adafruit MLX90614 Library**
   - **SparkFun MAX3010x Pulse and Proximity Sensor Library**

### Installation & Uploading

1. Open the [arduino_ide/LiberiMonitor](./arduino_ide/LiberiMonitor) directory.
2. Open `LiberiMonitor.ino` in Arduino IDE.
3. Select your ESP32 board model (e.g., `ESP32 Dev Module`) and the appropriate COM port.
4. Click **Upload** to flash the firmware.

### Running & Dashboard Modes

Once uploaded, press the push button on GPIO 17 to start monitoring. The firmware streams data simultaneously over physical USB and virtual Bluetooth Serial.

To view the dashboard, you simply need to open `index.html` in a supported browser (Chrome or Edge) or serve it locally using Node (`node server.js` -> `http://localhost:3000`).

#### Mode A: Wireless Bluetooth Connection (Recommended)

1. On your PC/Mac, open your OS **Bluetooth Settings** and pair with the device named **`Liberi_Monitor`**.
2. Once paired, your OS will assign it a virtual serial COM port.
3. Open the dashboard in Chrome/Edge.
4. Click the **"Connect to ESP32"** button.
5. Select the newly created Bluetooth COM port from the browser's pairing dialog to start the wireless stream.

#### Mode B: Wired USB Connection

1. Keep the ESP32 connected to your PC using the USB-C cable.
2. Open the dashboard in Chrome/Edge.
3. Click the **"Connect to ESP32"** button.
4. Select the physical USB COM port assigned to your ESP32 to start the wired stream.
