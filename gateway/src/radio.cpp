#include "radio.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
namespace garage::radio {
void start() {
  static bool started = false;
  if (started)
    return;
  started = true;
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();
  wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&init));
  // Test credentials stay in RAM; do not overwrite saved Wi-Fi config in NVS.
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());
  // Default STA power save sleeps the radio between router beacons, so it can miss ESP-NOW
  // packets from the node. The gateway is USB/mains powered, so keep the radio always on.
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
}
} // namespace garage::radio
