# Water Quality Monitor

**ESP32-based IoT system for real-time water quality monitoring with cloud connectivity and solar power management**

![Build Status](https://img.shields.io/badge/build-passing-brightgreen)
![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)
![Platform](https://img.shields.io/badge/platform-ESP32-blue)

## Table of Contents

- [Features](#features)
- [Hardware Components](#hardware-components)
- [System Architecture](#system-architecture)
- [Installation](#installation)
- [Configuration](#configuration)
- [Usage](#usage)
- [API Reference](#api-reference)
- [Power Management](#power-management)
- [Contributing](#contributing)
- [License](#license)

## Features

- **Real-time water quality monitoring**: TDS, turbidity, and pH measurement
- **Local alerting system**: Buzzer, LED indicators, and LCD display for immediate feedback
- **Cloud connectivity**: MQTT over TLS for remote monitoring and data logging
- **Solar-powered operation**: Battery backup with solar panel charging for remote locations
- **Data persistence**: Local SD card logging with offline data synchronization
- **Over-the-air updates**: Remote firmware updates via secure HTTPS
- **Low power design**: Deep sleep cycles with intelligent power management

## Hardware Components

### Sensors
- **TDS Sensor**: Total Dissolved Solids measurement (ppm)
- **Turbidity Sensor**: Water clarity measurement (NTU)
- **pH Sensor**: Acidity/alkalinity measurement
- **Battery Monitor**: Voltage divider for battery level monitoring

### Actuators & Display
- **16x2 LCD (I2C)**: Local data display for field readings
- **Buzzer**: Audio alerts for threshold violations
- **RGB LEDs**: Visual status indicators (red/yellow/green)

### Power & Storage
- **Solar Panel**: 5-10W for sustainable power
- **18650 Li-ion Batteries**: Dual battery configuration for extended operation
- **SD Card Module**: Local data logging and backup
- **RTC DS3231**: Real-time clock for accurate timestamps

### Connectivity
- **ESP32 DevKit V1**: Main controller with built-in WiFi
- **MOSFET Power Switch**: Sensor power management for energy efficiency

## System Architecture

```
┌─────────────────────────┐     WiFi/MQTT/TLS     ┌──────────────────────────┐
│      DEVICE LAYER       │ ──────────────────────▶│    CLOUD BACKEND         │
│                         │                        │                          │
│  ESP32 Controller       │                        │  MQTT Broker (TLS)       │
│  ├── Sensor Array       │◀────────────────────── │  Backend Processor       │
│  ├── Local Display      │   Commands/OTA         │  Time-series Database    │
│  ├── Alert System       │                        │  Web Dashboard           │
│  ├── Data Storage       │                        │  Alert Service           │
│  └── Power Management   │                        │                          │
└─────────────────────────┘                        └──────────────────────────┘
```

## Installation

### Prerequisites

- [PlatformIO](https://platformio.org/) IDE or extension for VS Code
- ESP32 development board
- Required libraries (automatically installed via platformio.ini):
  - ArduinoJson
  - PubSubClient
  - RTClib
  - LiquidCrystal_I2C

### Setup Steps

1. **Clone the repository**
   ```bash
   git clone https://github.com/hamin-baek/water-quality-monitor.git
   cd water-quality-monitor
   ```

2. **Configure credentials**
   ```bash
   cp src/secrets.example.h src/secrets.h
   ```
   Edit `src/secrets.h` with your WiFi and MQTT broker credentials.

3. **Build and upload**
   ```bash
   pio run --target upload
   ```

4. **Monitor serial output**
   ```bash
   pio device monitor
   ```

## Configuration

### WiFi and MQTT Settings

Edit `src/secrets.h`:

```cpp
// WiFi Configuration
#define WIFI_SSID "your_network_name"
#define WIFI_PASSWORD "your_wifi_password"

// MQTT Broker Configuration
#define MQTT_BROKER "your.mqtt.broker.com"
#define MQTT_PORT 8883
#define MQTT_USERNAME "device_username"
#define MQTT_PASSWORD "device_password"
#define DEVICE_ID "water_monitor_001"
```

### Sensor Calibration

Threshold values can be adjusted in the respective sensor modules:

- **TDS**: Default threshold 500 ppm
- **Turbidity**: Default threshold 5 NTU
- **pH**: Safe range 6.5-8.5

### Power Management

Sleep cycle interval can be modified in `main.cpp`:

```cpp
#define SLEEP_TIME_SECONDS  600ULL  // 10 minutes between readings
```

## Usage

### Normal Operation

The device operates in 10-minute cycles:

1. **Wake up** from deep sleep
2. **Power on** sensors via MOSFET switch
3. **Read** water quality parameters
4. **Evaluate** thresholds and trigger local alerts if needed
5. **Display** current readings on LCD
6. **Connect** to WiFi and publish data to cloud
7. **Log** data to SD card
8. **Return** to deep sleep

### Local Alerts

- **Green LED**: All parameters within safe limits
- **Yellow LED**: Warning levels detected
- **Red LED + Buzzer**: Critical thresholds exceeded
- **LCD Display**: Current sensor readings and status

### Cloud Integration

Data is published to MQTT topics:
- `desa/{village}/waterquality/{device_id}/telemetry` - Regular readings
- `desa/{village}/waterquality/{device_id}/alert` - Threshold violations
- `desa/{village}/waterquality/{device_id}/status` - Device online/offline status

## API Reference

### MQTT Data Format

**Telemetry Payload** (JSON):
```json
{
  "ts": 1735459200,
  "tds": 245.3,
  "turb": 3.2,
  "ph": 7.1,
  "batt_v": 3.87,
  "alert": false
}
```

**Alert Payload** (JSON):
```json
{
  "ts": 1735459200,
  "reason": "tds_high",
  "value": 620.5,
  "threshold": 500
}
```

### Local Data Format

SD card logs are stored in CSV format:
```
timestamp,tds_ppm,turbidity_ntu,ph,battery_v,alert_flag,sent_to_cloud
2026-08-29T14:10:00,245.3,3.2,7.1,3.87,0,1
```

## Power Management

### Energy Efficiency Features

- **Deep sleep cycles**: Device sleeps 99% of the time
- **Sensor power switching**: MOSFET-controlled sensor rail
- **Optimized WiFi usage**: Connect-publish-disconnect pattern
- **Solar charging**: Automatic battery management with MPPT controller

### Power Budget

- **Active cycle**: ~180mA for 15-20 seconds
- **Deep sleep**: ~0.15mA between cycles
- **Daily consumption**: ~200mAh (including safety margin)
- **Battery capacity**: 4000-6000mAh (dual 18650 configuration)
- **Solar generation**: 1000-2000mAh/day (5-10W panel)

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/enhancement`)
3. Commit your changes (`git commit -am 'Add new feature'`)
4. Push to the branch (`git push origin feature/enhancement`)
5. Create a Pull Request

### Development Guidelines

- Follow existing code structure with modular design
- Add comments for complex logic
- Test thoroughly with hardware before submitting
- Update documentation for new features

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- ESP32 community for excellent documentation and examples
- PlatformIO team for the robust development environment
- Open source sensor libraries that made this project possible

---

**Tags**: `iot` `esp32` `water-quality`

**Project Status**: Active development - suitable for deployment in pilot locations with proper field testing.