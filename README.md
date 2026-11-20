
# Dual-Zone IoT Environmental Logger

> A distributed ESP32-Wrover environmental monitoring system featuring an indoor "master" node with a local I2C display and remote MQTT data logging to Google Sheets, paired with an outdoor node.

---

## Overview

This project implements a wireless dual-zone environmental logging system designed to track temperature and humidity across indoor and outdoor environments.

The setup uses two ESP32-Wrover microcontrollers communicating via MQTT: an outdoor sensor node that transmits remote telemetry, and an indoor "master" node that aggregates both datasets, displays live readings on an I2C LCD, and pushes consolidated records to Google Sheets every 15 minutes.

---

## Hardware & Tech Stack

- **Microcontrollers:** 2x ESP32-Wrover modules
- **Sensors:** SHT31 High-Precision Temperature & Humidity Sensors
- **Display:** I2C LCD Display
- **Protocols & Cloud:** Wi-Fi, MQTT (Broker Pub/Sub), Google Apps Script Webhooks, Google Sheets

---

## 📊 Live Dashboard

You can view the environmental data logged by the ESP32 nodes here:

[View Live Google Sheets Dashboard](https://docs.google.com/spreadsheets/d/1bzZhhgqcaNZR67QA8EV1ULNdBV76X2SWmEw9hCDUZkA/edit?gid=0#gid=0)

---

## System Architecture & Node Roles

```text
[ Outdoor SHT31 Sensor ] ---> [ Outdoor ESP32 ] --(MQTT Publish)---> [ MQTT Broker ]
                                                                          |
[ Indoor SHT31 Sensor  ] ---> [ Indoor Master ESP32 ] <--------------------+
                                      |
                             [ I2C LCD Display ]
                                      |
                             [ HTTPS / Webhook ]
                                      |
                                      v
                             [ Google Sheets (15-min intervals) ]
```

---

## System Testing and Verification

To verify the functionality of the dual-zone environmental monitoring system, the following components and processes should be checked:

### 1. Sensor Data Collection

- Confirm that the indoor and outdoor SHT31 sensors provide temperature and humidity readings.
- Check that sensor measurements are displayed correctly and consistently.

### 2. MQTT Communication

- Verify that the outdoor ESP32 successfully publishes environmental data to the MQTT broker.
- Ensure that the indoor master ESP32 receives the transmitted data.

### 3. Local LCD Display

- Confirm that the I2C LCD displays the environmental readings.
- Check that the displayed values update as new measurements become available.

### 4. Google Sheets Data Logging

- Verify that the indoor master ESP32 sends consolidated environmental records through the configured webhook.
- Confirm that new records appear in Google Sheets at the intended 15-minute intervals.
- Check that temperature and humidity values are recorded correctly for both monitoring zones.

### 5. End-to-End System Verification

- Monitor the complete data flow from both sensor nodes to the indoor master ESP32.
- Verify that MQTT communication, local display updates, and cloud logging operate as expected.
- Compare the displayed sensor readings with the corresponding records stored in Google Sheets.

**Expected Outcome:** A functioning system should collect indoor and outdoor environmental measurements, display current readings locally, and maintain periodic cloud-based records for monitoring and analysis.
