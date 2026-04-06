# Firmware

This directory contains the embedded firmware for the **GuardianBell MK1** system.

The firmware is designed for the ESP32-CAM environment using the Arduino framework with PlatformIO. It integrates camera capture, motion detection, networking, cloud services,
and over-the-air (OTA) updates into a cohesive system.

## Architecture Overview

The firmware is organised into two primary directories: [**`/include`**](#include) and [**`/src`**](#src). Each module in the system typically follows this pattern:

- `include/module.h` → *interface definition*
- `src/module.cpp` → *implementation*

The project configuration is defined in [**`platformio.ini`**](./platformio.ini)

### `./include`

This [directory](./include/) contains all project header files (`.h`).

Headers define the interfaces used across the firmware,
including function declarations, constants, and shared types.

#### Purpose

Header files enable modular development by allowing components to interact without exposing implementation details. This separation improves maintainability, readability, and scalability.

### `./src`

This [directory](./src/) contains the project source code.

Source files (`.cpp`) implement the application logic and reference project headers located in the include directory.

#### Directory Structrure

The codebase is organised by responsibility to keep concerns separated
and the firmware easy to scale and maintain:

- [**`/config`**](./src/config/) → Runtime and build-time configuration sources (e.g. settings, secret credentials).

- [**`/hardware`**](./src/hardware/) → Hardware abstraction and device drivers (e.g. camera, sensors, storage).

- [**`/network`**](./src/network/) → Networking and communication layers (e.g. WiFi, MQTT, protocols).

- [**`/services`**](./src/services/) → High-level features and external service integrations
(e.g. cloud services, messaging, APIs).

- [**`/util`**](/src/util/) → Common utilities shared across the project (e.g. error handling, logging, time helpers).

- [**`main.cpp`**](./src/main.cpp) → Program entry point. Responsible for initialisation and coordinating
the main application flow.

## Design Principles

- **Modularity** → Each subsystem is isolated and reusable  
- **Reliability** → Emphasis on defensive error handling and safe OTA updates  
- **Efficiency** → Optimised for low power consumption and minimal blocking operations
- **Scalability** → Designed to accommodate additional hardware and services with minimal refactoring 

## Notes
- Secrets and credentials are stored separately (`secrets.h`)
- All headers use `#pragma once` for include guards
- Changes in headers propagate across all dependent modules
- Debugging can be toggled at compile time  