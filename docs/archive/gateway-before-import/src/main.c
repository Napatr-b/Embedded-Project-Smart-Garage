/*
 * Smart Garage: Gateway firmware (ESP32-S3, ESP-IDF)
 *
 * Tasks (E1), from highest to lowest priority:
 *   door_task     (6)  servo + door state machine; safety-critical, must react first
 *   sensors_task  (5)  ultrasonics + IR obstacle; feeds door interlocks
 *   uart_rx_task  (4)  receives node frames, ACKs, de-duplicates
 *   cloud_task    (3)  Sheets/MQTT upload with buffering (E4); network can wait
 *   face_task     (TBD) camera + ESP-WHO (E7), added once the camera works
 *
 * Status: TEMPLATE. Only UART receive + ACK is sketched. Not yet built or tested.
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"

#include "garage_protocol.h"
#include "pins.h"
/* #include "secrets.h"   // create from secrets.example.h when adding Wi-Fi/cloud */

static const char *TAG = "gateway";

#define NODE_UART       UART_NUM_1
#define NODE_BAUD       115200
#define UART_RX_BUF     1024

typedef struct {
  uint8_t       node_id;
  uint16_t      seq;
  gp_readings_t r;
} node_reading_t;

static QueueHandle_t s_reading_q;   /* uart_rx_task -> cloud_task */

/* ---------- UART link to sensor node(s) ---------- */

static void send_ack(uint8_t node_id, uint16_t seq) {
  uint8_t f[GP_MAX_FRAME];
  size_t n = gp_build_frame(f, GP_MSG_ACK, node_id, seq, NULL, 0);
  uart_write_bytes(NODE_UART, f, n);
}

static void uart_rx_task(void *arg) {
  gp_parser_t parser = {0};
  int32_t last_seq[GP_MAX_NODES];
  for (int i = 0; i < GP_MAX_NODES; i++) last_seq[i] = -1;

  uint8_t rx[64];
  for (;;) {
    int n = uart_read_bytes(NODE_UART, rx, sizeof(rx), pdMS_TO_TICKS(50));
    for (int i = 0; i < n; i++) {
      size_t len = gp_parser_feed(&parser, rx[i]);
      if (!len) continue;

      const uint8_t *f = parser.buf;
      uint8_t  id  = gp_node_id(f);
      uint16_t seq = gp_seq(f);

      if (gp_type(f) != GP_MSG_READINGS || gp_len(f) != sizeof(gp_readings_t) || id >= GP_MAX_NODES) {
        ESP_LOGW(TAG, "unexpected frame type=%u len=%u node=%u", gp_type(f), gp_len(f), id);
        continue;
      }

      send_ack(id, seq);                     /* ACK even duplicates: the first ACK may have been lost */
      if (last_seq[id] == seq) {
        ESP_LOGI(TAG, "dup node=%u seq=%u", id, seq);
        continue;
      }
      last_seq[id] = seq;

      node_reading_t msg = { .node_id = id, .seq = seq };
      memcpy(&msg.r, gp_payload(f), sizeof(msg.r));
      ESP_LOGI(TAG, "node=%u seq=%u T=%.2f H=%.2f ldr=%u eco2=%u tvoc=%u aqi=%u led=%u",
               id, seq, msg.r.temp_c_x100 / 100.0, msg.r.humidity_x100 / 100.0,
               msg.r.ldr_raw, msg.r.eco2_ppm, msg.r.tvoc_ppb, msg.r.aqi, msg.r.led_on);

      if (xQueueSend(s_reading_q, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "reading queue full, dropping");
      }
    }
  }
}

/* ---------- Gateway sensors ---------- */

static void sensors_task(void *arg) {
  for (;;) {
    /* TODO: ultrasonic entry (approach -> trigger face capture)
     * TODO: ultrasonic bay (occupancy)
     * TODO: IR obstacle via GPIO ISR + deferred processing (E2) */
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

/* ---------- Door ---------- */

typedef enum { DOOR_CLOSED, DOOR_OPENING, DOOR_OPEN, DOOR_CLOSING, DOOR_FAULT } door_state_t;

static void door_task(void *arg) {
  door_state_t state = DOOR_CLOSED;
  (void)state;
  for (;;) {
    /* TODO: LEDC PWM servo on PIN_SERVO
     * TODO: commands from face_task / MQTT via queue; Stop; movement timeout -> FAULT
     * TODO: never close while IR obstacle is active */
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

/* ---------- Cloud ---------- */

static void cloud_task(void *arg) {
  node_reading_t msg;
  for (;;) {
    if (xQueueReceive(s_reading_q, &msg, portMAX_DELAY) == pdTRUE) {
      /* TODO: add timestamp (SNTP), POST to Sheets, publish MQTT (TLS, E5)
       * TODO: if offline, store in a ring buffer and replay on reconnect (E4) */
    }
  }
}

void app_main(void) {
  const uart_config_t cfg = {
    .baud_rate  = NODE_BAUD,
    .data_bits  = UART_DATA_8_BITS,
    .parity     = UART_PARITY_DISABLE,
    .stop_bits  = UART_STOP_BITS_1,
    .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
  };
  ESP_ERROR_CHECK(uart_driver_install(NODE_UART, UART_RX_BUF, 0, 0, NULL, 0));
  ESP_ERROR_CHECK(uart_param_config(NODE_UART, &cfg));
  ESP_ERROR_CHECK(uart_set_pin(NODE_UART, PIN_NODE_UART_TX, PIN_NODE_UART_RX,
                               UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

  s_reading_q = xQueueCreate(32, sizeof(node_reading_t));

  xTaskCreate(door_task,    "door",    4096, NULL, 6, NULL);
  xTaskCreate(sensors_task, "sensors", 4096, NULL, 5, NULL);
  xTaskCreate(uart_rx_task, "uart_rx", 4096, NULL, 4, NULL);
  xTaskCreate(cloud_task,   "cloud",   8192, NULL, 3, NULL);

  ESP_LOGI(TAG, "gateway started");
}
