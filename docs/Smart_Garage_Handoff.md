# Smart Garage project handoff

Use this brief as context for continuing the CEDT Embedded Systems miniature Smart Garage project. Use the current design below when older files disagree with it. Reply in Thai when asked, and keep familiar technical terms such as Gateway, UART and Face Recognition in English.

## Status update — 11 Nov 2026

- The proposal PDF has been submitted. No instructor feedback has come back, and none is expected.
- Parts are still being bought. Nothing has been tested and no code has been written yet.
- Toolchain: PlatformIO in VS Code, with Arduino as an option where it's more convenient. Note: ESP-WHO is an ESP-IDF project, and current Arduino cores no longer include face recognition. The likely setup is the Gateway on PlatformIO with `framework = espidf` (or Arduino as an IDF component), and the Sensor Node on PlatformIO with Arduino.
- **IR is back in the design as an IR obstacle (reflective) sensor**, not a break-beam pair. It sits on the Gateway as an extra sensor type (protecting M1 if the camera slips) and is planned for door-path obstruction detection and/or the E2 ISR input. Reflective sensors have short range and react to ambient light and dark surfaces, so tune the potentiometer on the actual model.
- Next milestone: checkpoint video, Fri 20 Nov 2026 (sensor node → gateway → cloud → dashboard). Prioritise UART + Google Sheets + a basic dashboard first, and bench-test the camera alongside.
- Recommended E items: E1, E2, E4, E5, E7 (E7 counts on the Gateway per FAQ #4), with E6 as a spare. E3 is a poor fit for an always-on garage. Explain any change from the proposal in the report.
- Notes still open: on the N8R8, GPIO35–37 are used by the PSRAM, so map all pins on paper first. A browser dashboard needs MQTT over WSS. Scalability: plan either a second physical node on UART2 or 2 simulated nodes, with the node ID in every message.

## Goal and current status

Build a tabletop garage that opens a sliding door for an enrolled person, shows whether the bay is occupied, turns on lighting in the dark, logs environmental readings and door events, and supports remote status and control. The proposal draft, architecture diagram and animated Blender concept exist. The gateway ESP32-S3 is already owned. Hardware integration, the face recognition demo, and cloud deployment are still unverified.

## Current design

| Part | Design |
|---|---|
| Gateway | ESP32-S3-DevKitC-1-N8R8. Receives UART data, reads both ultrasonic sensors and the IR obstacle sensor, runs local Face Recognition, controls the door servo, and connects to cloud services. |
| Sensor Node | Separate ESP32 DOIT DevKit. Reads LDR, DHT22 and ENS160. Controls the LED based on ambient light and sends readings to the Gateway over wired UART. Confirm actual sensor availability before finalizing the BOM. |
| Entrance and parking | One ultrasonic sensor at the entrance detects approach and triggers image capture. A second ultrasonic sensor inside checks bay occupancy. An IR obstacle sensor is planned for door-path obstruction detection (see status update). Occupancy is a separate measurement from doorway clearance. |
| Camera and AI | OV2640 DVP camera on the ESP32-S3. Planned ESP-WHO pipeline detects a face, extracts an embedding, compares it with enrolled users, and gives the result to door control. No cloud inference. Plate recognition was superseded. Liveness detection is outside the current prototype scope. |
| Door | Sideways sliding panel in upper and lower guides, with a 180° positional servo, crank and connecting rod. Current material choice is 2 mm Plaswood. Use hot glue and triangular offcut braces to support wall-to-base joints. Reinforce the linkage attachment and the servo mounting area as needed. |
| Cloud | Proposed Vercel Web Dashboard shows readings, bay and door status, event history, and Open, Close and Stop controls. MQTT carries status and commands. Google Sheets receives timestamped readings and events through an HTTPS endpoint. LINE Messaging API sends selected alerts. Camera streaming is undecided. |

## Decisions and verification still needed

1. **Camera module and pins.** The DevKitC has GPIO headers, so a bare OV2640 ribbon needs a matching FPC breakout. Prefer a DVP module with a documented breakout, or a camera and adapter sold as a confirmed pair. Check the ribbon pin count, pitch, contact orientation, pinout, and required supply rails. The earlier Shopee listing was inaccessible. A 24-pin, 0.5 mm connector was only an example. Map camera signals alongside UART, ultrasonics, IR and servo before wiring. First capture a test image, then adapt ESP-WHO to the custom board. The official S3-EYE example is a reference. Compatibility with this DevKitC and camera combination has not been demonstrated.
2. **Mechanism and power.** Start with an SG90 positional servo if the lightweight panel slides freely. MG996R is an option if measured force requires it. MG996SR specs were not confirmed. The Blender model assumes a 300 × 230 mm base, 2 mm panels and a 98 mm simulated door stroke. Verify physical clearance, force, usable servo sweep and crank dimensions before fabrication. For a portable demo, 5 V / 3 A total output from a power bank was discussed as a starting point for two boards plus SG90. Confirm combined output and test for resets and automatic cutoff. Feed servo power directly from its supply and share ground with the Gateway. Use an adapter for extended bench testing if available.
3. **Door behavior.** Use Closed, Opening, Open, Closing and Fault states. Define how Stop and movement timeout act, how actual door state is measured, and what prevents closing on an obstruction. Confirm the closing condition before enabling automatic closing. An enrolled face is required for automatic opening. Manual dashboard commands need authorization and must honor local interlocks.
4. **Communications.** Planned UART payload includes node ID, sequence number, readings and checksum, with ACK/retry. Define framing, duplicate handling, and timeouts. Gateway adds a timestamp and buffers readings and events during cloud outages, then forwards them on reconnect. Define MQTT topics, authentication and TLS, Sheets endpoint, and LINE alert events. Keep credentials out of Git.

## Course rubric

The submitted proposal listed E1, E2, E3, E4 and E6. An earlier handoff targeted E1, E2, E4, E5 and E6. E7 is a strong candidate once Face Recognition runs on the MCU. Pick the final five based on working demos.

- **E1** FreeRTOS with at least three tasks, justified priorities and inter-task communication.
- **E2** ISR with deferred processing. Candidates: IR obstacle sensor output, or ultrasonic Echo edge capture.
- **E3** Sleep mode, measured current and estimated battery life.
- **E4** Node-to-gateway ACK/retry and cloud outage buffering with replay.
- **E5** Encrypted, authenticated cloud connection and secrets kept out of Git.
- **E6** Demonstrated OTA firmware update. OTA was discussed for the DOIT DevKit using ArduinoOTA. The ESP-IDF Gateway needs its own OTA approach if selected.
- **E7** On-device AI inference demonstrated on the MCU.
- **E8** A driver written at register or bus level, or a mesh of at least three nodes. The basic point-to-point UART link does not count.

## Existing files

- Smart_Garage_Proposal_TH (PDF in this project) is the submitted Thai proposal. It predates the 2 mm material, the removal and later return of IR, and the move to an obstacle sensor.
- Smart_Garage_Architecture.drawio is the editable diagram. It still shows a provisional DOIT Gateway, IR trigger and AI TBD. Revise those blocks to the current design.
- Smart_Garage.blend is the editable cutaway concept. Its timeline shows closed at frame 1, open at 52, and closed again at 113.
- ioT Sensor List (27 Aug 2025).xlsx is the course lending inventory. Check live availability with the team or lab before treating a listed sensor as available.

## Where to resume

For the checkpoint (20 Nov): get UART between the two boards, Sheets logging and a basic dashboard working first. For hardware, identify the exact OV2640 module and verify a camera-only capture on the owned ESP32-S3 before combining pins and firmware. In parallel, bench-test the sliding panel and servo. Then demonstrate local face enrollment and matching, and add MQTT control and LINE alerts. Treat each integration as pending until it has been demonstrated.
