
# Dual-Zone IoT Environmental Logger 
> A distributed ESP32-Wrover environmental monitoring system featuring an indoor "master" node with local I2C display and remote MQTT data logging to Google Sheets paired with an Outdoor node.

---

## Overview
This project implements a wireless dual-zone environmental logging system designed to track temperature and humidity across indoor and outdoor environments. The setup uses two ESP32-Wrover microcontrollers communicating via MQTT: an outdoor sensor node that transmits remote telemetry, and an indoor "master" node that aggregates both datasets, displays live readings on an I2C LCD, and pushes consolidated records to Google Sheets every 15 minutes.

---

## Hardware & Tech Stack
* **Microcontrollers:** 2x ESP32-Wrover modules
* **Sensors:** SHT31 High-Precision Temperature & Humidity Sensors
* **Display:** I2C LCD Display
* **Protocols & Cloud:** Wi-Fi, MQTT (Broker Pub/Sub), Google Apps Script Webhooks, Google Sheets

---

## 📊 Live Dashboard
You can view the real-time environmental data logged by the ESP32 nodes here:
[View Live Google Sheets Dashboard](https://docs.google.com/spreadsheets/d/1bzZhhgqcaNZR67QA8EV1ULNdBV76X2SWmEw9hCDUZkA/edit?gid=0#gid=0)

---

## System Architecture & Node Roles

```text
[ Outdoor SHT31 Sensor ] ---> [ Outdoor ESP32 ] --(MQTT Publish)---> [ MQTT Broker ]
                                                                          |
[ Indoor SHT31 Sensor  ] ---> [ Indoor Master ESP32 ] <-----------------+
                                      |
                             [ I2C LCD Display ]
                                      |
                             [ HTTPS / Webhook ]
                                      v
                             [ Google Sheets (15-min intervals) ]
=======
# esp_environment_logger
A dual-zone ESP32 environmental logger with local I2C display and a synced MQTT Google Sheet.

