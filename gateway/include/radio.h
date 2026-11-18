#pragma once
// Wi-Fi radio bring-up shared by ESP-NOW (node_link) and MQTT (network).
// Both need the Wi-Fi driver running, but it may only be initialised once, so whichever
// module starts first calls start() and the second call does nothing.
namespace garage::radio {
// Idempotent: netif + default event loop + STA netif + Wi-Fi driver (RAM storage), STA mode, started.
// Does not join a network; network.cpp sets the credentials and connects.
void start();
} // namespace garage::radio
