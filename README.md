# ESP32-SMART-HOME-AUTOMATION-BLYNK
Smart Home Automation System using ESP32 and Blynk IoT Cloud

## Overview

An IoT-based smart home automation system built using the ESP32 microcontroller and Blynk Cloud platform. The system automatically detects rainfall and closes simulated doors and windows using servo motors, while monitoring temperature and humidity and displaying live status on an LCD screen. Developed as a group coursework project for CC5068NI - Cloud Computing and the Internet of Things at Islington College.

## Problem Statement

Open doors and windows during sudden rainfall can cause property damage, electronic equipment damage, and safety hazards, especially when homes are left unattended. Existing smart home solutions are often expensive and complex. This project provides an affordable, automated alternative.

## Features

* Automatic rain detection using a raindrop sensor
* Automatic closing of doors/windows via servo motors when rain is detected
* Real-time temperature and humidity monitoring using DHT11 sensor
* Audible buzzer alert during rainfall
* Live status display on 16x2 I2C LCD
* Remote monitoring and manual override via Blynk IoT Cloud app
* Power supply via Li-ion battery and buck converter

## Technologies Used

* ESP32 Development Board
* Arduino IDE (C/C++)
* Blynk IoT Cloud
* DHT11 Temperature & Humidity Sensor
* Raindrop Sensor
* Servo Motors (SG90)
* 16x2 I2C LCD Display
* Draw.io and Tinkercad (for circuit design)

## Testing

Six tests were conducted to verify system functionality, including code upload, rain detection triggering buzzer/servo response, servo return to open position, DHT11 sensor readings, and LCD display output. All tests were completed successfully.

## Learning Outcomes

* IoT system design and integration
* Embedded systems programming
* Sensor data acquisition and processing
* Cloud platform integration (Blynk)
* Hardware-software integration
* Team collaboration on a technical project

## Future Improvements

* Solar power integration for eco-friendly, off-grid operation
* Cloud-based data analytics using Firebase or AWS IoT Core
* Voice control integration with Alexa or Google Assistant
* Facial recognition/biometric security features
* SMS/email notification alerts via Twilio or SendGrid

## Group Members

* Jiwosh Shrestha
* Shubham Subedi
* Saiman GC
* Sunaimi Dhakal
* Sristi Shahi
* Elle Shrestha

## Repository Maintained By

Elle Shrestha
