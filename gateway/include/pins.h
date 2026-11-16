#pragma once
// Gateway pins (ESP32-S3 N16R8). Unassigned = -1. Keep in sync with docs/pin-map.md.
// Reserved: GPIO35-37 (octal PSRAM), 19/20 (USB), 43/44 (UART0 console), 0/3/45/46 (strapping).
namespace pins {
constexpr int Unassigned = -1;
constexpr int UartTx = 17, UartRx = 18; // UART1 to sensor node: TX -> node GPIO16, RX <- node GPIO17
constexpr int IrInput = Unassigned, UltrasonicTrigger = Unassigned, UltrasonicEcho = Unassigned;
constexpr int ServoPwm = Unassigned, HardwareStop = Unassigned;
constexpr int OpenFeedback = Unassigned, ClosedFeedback = Unassigned;
// Camera requires a complete OV2640 map, not a guessed board preset.
constexpr bool CameraMapConfirmed = false;
} // namespace pins
