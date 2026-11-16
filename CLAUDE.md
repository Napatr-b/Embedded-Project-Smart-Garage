# Smart Garage — CEDT Embedded Systems Final Project 2026

Miniature tabletop smart garage: a sliding door opens for an enrolled face, the system shows whether the bay is occupied, turns on lighting in the dark, logs environmental readings and door events to the cloud, and supports remote status and control.

**Full context:** read `docs/Smart_Garage_Handoff.md` before starting any non-trivial work. It's the source of truth for the current design. Where older files (proposal PDF, drawio diagram) disagree with it, the handoff wins.
The submitted Thai proposal is `docs/Smart_Garage_Proposal_TH.pdf` (outdated in places; see the handoff).

## Working with me

- I'm Patr. Call me "bro" in English and "พี่ชาย" in Thai.
- Reply in Thai when I write in Thai. Keep technical terms (Gateway, UART, Face Recognition, FreeRTOS, MQTT…) in English.
- Every team member must be able to explain their own code during Q&A, so explain non-obvious design choices briefly in comments or in your reply.
- Code from other people or projects must be cited (in the README and report). AI tool use gets declared in the report.

## Hardware (current design)

| Part | Board / parts |
|---|---|
| Gateway | ESP32-S3-DevKitC-1-N8R8. UART RX from the node, 2× ultrasonic (entrance and bay occupancy), IR obstacle sensor (door path / E2 ISR), OV2640 DVP camera, door servo, Wi-Fi → cloud |
| Sensor Node | ESP32 DOIT DevKit. LDR, DHT22, ENS160, LED lighting. Sends readings over wired UART |
| Door | Sliding panel driven by a 180° positional servo via crank + rod (SG90 first, MG996R if needed) |

Pin notes: on the N8R8, **GPIO35–37 are used by PSRAM**, so don't assign them. Map camera + UART + ultrasonics + IR + servo pins on paper before wiring.

## Toolchain

- PlatformIO in VS Code.
- Gateway: `framework = espidf` (ESP-WHO face recognition is an ESP-IDF project; current Arduino cores dropped face recognition). Arduino-as-IDF-component is an option.
- Sensor Node: PlatformIO + Arduino.
- Cloud: Google Sheets via an HTTPS endpoint (logging), MQTT for status and commands (browser dashboard needs MQTT over WSS), Vercel web dashboard, LINE Messaging API for alerts.

## Rules that must hold

- **Never commit secrets** (Wi-Fi passwords, API keys, MQTT credentials, LINE tokens, Apps Script URLs with keys). Put them in a git-ignored file such as `secrets.h` / `.env` and commit a `*.example` template instead. This is also rubric item E5.
- Every UART/MQTT message carries a **node ID** (scalability score) and a sequence number. The UART frame also has a checksum, with ACK/retry (E4).
- Moving parts need a safe stop: movement timeout + Stop command + obstruction interlock. Door states: Closed, Opening, Open, Closing, Fault. Manual dashboard commands must still honor local interlocks.
- The Gateway buffers readings and events while the cloud is unreachable and replays them on reconnect (E4).
- Every member needs commits in this repo (M8 + peer factor), so keep commits attributable.

## Course requirements (summary)

- M1 ≥5 sensor types (≥2 on the node, ≥2 on the gateway) · M2 ≥1 actuator (servo) · M3 node→gateway link (UART) · M4 AI in a closed loop (face recognition → door) · M5 cloud storage + dashboard · M6 notifications (LINE) · M7 safety · M8 public Git with commits from everyone. Each missing item is −2.
- Planned depth items: **E1** FreeRTOS ≥3 tasks with queues/semaphores/event groups, **E2** ISR + deferred processing, **E4** ACK/retry + store-and-forward, **E5** TLS + auth + no secrets in Git, **E7** on-device AI on the Gateway. E6 (OTA) is the spare.
- Metrics to measure later: end-to-end latency, node→gateway packet delivery rate, AI accuracy, uptime, and so on, with repeated runs (mean, min/max).

## Deadlines

- **Fri 20 Nov 2026**: checkpoint video (≤2 min) showing sensor node → gateway → cloud → dashboard. Priority: UART between the boards, then Sheets logging, then a basic dashboard. Bench-test the camera in parallel.
- **Tue 8 Dec 2026**: final demo, report (≤10 pages), video, source + repo link.

## Status (as of 11 Nov 2026)

Proposal submitted. Parts still being bought. **No code yet, nothing tested.** Treat every integration as unverified until it's demonstrated.

## Repo layout

```
gateway/        PlatformIO, ESP-IDF (ESP32-S3). src/main.c has the FreeRTOS task skeleton
sensor-node/    PlatformIO, Arduino (ESP32 DOIT). src/main.cpp sends READINGS with ACK/retry
shared/protocol garage_protocol.h: UART frame, CRC16, parser (used by both firmwares)
cloud/          apps-script/Code.gs (Sheets logger), MQTT/LINE notes
dashboard/      Vercel web app (not started)
docs/           handoff, proposal, protocol.md, pin-map.md (PROVISIONAL), test-plan.md
```

The templates have not been built or flashed yet. Pin assignments are provisional until the camera breakout is chosen.
