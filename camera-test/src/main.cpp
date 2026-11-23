#include <Arduino.h>
#include <esp_camera.h>

#if __has_include("config.local.h")
#include "config.local.h"
#else
#include "config.example.h"
#endif

namespace {
using namespace cameraPins;
constexpr int RequiredPins = 14;
constexpr int AssignedPins[] = {
    Xclk, Pclk, Vsync, Href, Sda, Scl, D0, D1, D2, D3, D4, D5, D6, D7, PowerDown, Reset,
};

bool validPins() {
  uint64_t used = 0;
  for (size_t i = 0; i < sizeof(AssignedPins) / sizeof(AssignedPins[0]); ++i) {
    const int pin = AssignedPins[i];
    if (i >= RequiredPins && pin == -1) continue;
    // Reserve Gateway UART, USB, flash/PSRAM, UART0 and strapping pins.
    if (!((pin >= 1 && pin <= 16 && pin != 3) ||
          pin == 21 || (pin >= 38 && pin <= 42) ||
          (pin >= 47 && pin <= 48))) {
      return false;
    }
    const uint64_t bit = UINT64_C(1) << pin;
    if (used & bit) return false;
    used |= bit;
  }
  return true;
}

const char* startupError = nullptr;
}

void setup() {
  Serial.begin(115200);
  if (!validPins()) {
    startupError = "invalid camera pins";
    return;
  }
  if (!psramFound()) {
    startupError = "PSRAM unavailable";
    return;
  }

  camera_config_t config{};
  config.pin_xclk = Xclk;
  config.pin_pclk = Pclk;
  config.pin_vsync = Vsync;
  config.pin_href = Href;
  config.pin_sccb_sda = Sda;
  config.pin_sccb_scl = Scl;
  config.pin_d0 = D0;
  config.pin_d1 = D1;
  config.pin_d2 = D2;
  config.pin_d3 = D3;
  config.pin_d4 = D4;
  config.pin_d5 = D5;
  config.pin_d6 = D6;
  config.pin_d7 = D7;
  config.pin_pwdn = PowerDown;
  config.pin_reset = Reset;
  config.ledc_timer = LEDC_TIMER_0;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  const esp_err_t result = esp_camera_init(&config);
  if (result != ESP_OK) {
    startupError = esp_err_to_name(result);
  }
}

void loop() {
  if (Serial.read() != 'c') {
    delay(10);
    return;
  }
  if (startupError) {
    Serial.printf("ERR %s\n", startupError);
    return;
  }

  camera_fb_t* frame = esp_camera_fb_get();
  if (frame && frame->format == PIXFORMAT_JPEG && frame->len > 0) {
    Serial.printf("JPEG %u\n", static_cast<unsigned>(frame->len));
    Serial.write(frame->buf, frame->len);
  } else {
    Serial.println("ERR frame capture failed");
  }
  if (frame) esp_camera_fb_return(frame);
}
