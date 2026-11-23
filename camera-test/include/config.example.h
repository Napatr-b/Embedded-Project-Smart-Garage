#pragma once

// Populate from the confirmed camera-to-Gateway wiring.
// -1 is allowed only for a PWDN or RESET signal handled by the adapter.
namespace cameraPins {
constexpr int Xclk = -1;
constexpr int Pclk = -1;
constexpr int Vsync = -1;
constexpr int Href = -1;
constexpr int Sda = -1;
constexpr int Scl = -1;
constexpr int D0 = -1, D1 = -1, D2 = -1, D3 = -1;
constexpr int D4 = -1, D5 = -1, D6 = -1, D7 = -1;
constexpr int PowerDown = -1;
constexpr int Reset = -1;
}
