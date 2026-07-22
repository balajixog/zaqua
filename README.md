# ZaquaFlow
### Smart Irrigation & Pump Health Monitoring System using ESP32

## Overview

ZaquaFlow is an IoT-based smart irrigation system that automatically controls irrigation based on soil moisture while continuously monitoring pump health. The system uses multiple sensors to detect irrigation faults, compute performance indicators, and generate real-time reports.

---

## Features

- Automatic irrigation control
- Soil moisture monitoring
- Water flow measurement
- Pump current monitoring
- Relay-based pump control
- Dry-run detection
- Overload detection
- Blockage detection
- Leakage detection
- KPI calculation
- Dynamic Irrigation Intelligence Score (DIIS)
- CSV telemetry export
- Irrigation cycle summary

---

## Hardware Used

| Component | Quantity |
|-----------|---------:|
| ESP32 Dev Board | 1 |
| Capacitive Soil Moisture Sensor | 1 |
| YF-S201 Flow Sensor | 1 |
| ACS712 Current Sensor | 1 |
| Relay Module | 1 |
| 12V DC Water Pump | 1 |
| 12V Adapter | 1 |
| Connecting Wires | As required |

---

## Software

- Arduino IDE
- ESP32 Board Package
- C++

---

## System Architecture

```
           Soil Sensor
                |
Flow Sensor --> ESP32 <-- Current Sensor
                |
             Relay Module
                |
             Water Pump
```

---

## Project Workflow

```
Read Sensors
      │
      ▼
Automatic Irrigation
      │
      ▼
Fault Detection
      │
      ▼
KPI Calculation
      │
      ▼
DIIS Calculation
      │
      ▼
CSV Export
      │
      ▼
Cycle Summary
```

---

## Fault Detection

| Fault | Condition |
|--------|-----------|
| Dry Run | Current present but little/no water flow |
| Overload | Current exceeds safe limit |
| Blockage | High current with low flow |
| Leakage | Large water usage with little soil improvement |

---

## KPIs

- Electrical Conversion Indicator (ECI)
- Hydraulic Consistency Indicator (HCI)
- Soil Recovery Indicator (SRIt)
- Energy Utilization Indicator (EUI)

---

## DIIS

Dynamic Irrigation Intelligence Score (DIIS) combines normalized KPIs to evaluate the overall performance of each irrigation cycle.

---

## Folder Structure

```
ZaquaFlow/
│
├── firmware/
│   └── firmware.ino
│
├── datasets/
│
├── docs/
│
├── hardware/
│
├── README.md
└── LICENSE
```

---

## Future Enhancements

- MQTT Integration
- ThingSpeak Dashboard
- Mobile Application
- AI-based Fault Prediction
- Cloud Database Integration

---

## Authors

**Balaji G , AADHITHYAN S , ABIKESH A , KABIVALAVAN K**
