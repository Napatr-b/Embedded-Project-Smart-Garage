#pragma once
// All GPIOs are unassigned. Do not replace these without the team's reviewed wiring map.
namespace pins {
constexpr int Unassigned = -1;
constexpr int UartTx = Unassigned, UartRx = Unassigned;
constexpr int IrInput = Unassigned, UltrasonicTrigger = Unassigned, UltrasonicEcho = Unassigned;
constexpr int ServoPwm = Unassigned, HardwareStop = Unassigned;
constexpr int OpenFeedback = Unassigned, ClosedFeedback = Unassigned;
// Camera requires a complete OV2640 map, not a guessed board preset.
constexpr bool CameraMapConfirmed = false;
} // namespace pins
