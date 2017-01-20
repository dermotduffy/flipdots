#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_wifi.h"

#include "mutex-util.h"
#include "network.h"

#define SERVER_TASK_NAME             "server"
#define SERVER_TASK_STACK_WORDS      2<<13
#define SERVER_TASK_PRORITY          2
#define SERVER_UDP_PORT              7000
#define SERVER_ERROR_BUF_LEN      80
#define SERVER_RECV_TIMEOUT_SECS     10

#define DELAY_SERVER_ERROR_MS        1000

EventGroupHandle_t network_event_group;

SemaphoreHandle_t network_data_mutex;
uint8_t network_data_buf[NETWORK_DATA_BUF_SIZE];
uint8_t network_data_buf_used_size;

static xTaskHandle task_server_handle;
const static char *LOG_TAG = "server";

static void submit_data(
    const uint8_t recv_buf[NETWORK_DATA_BUF_SIZE], int bytes_used) {
  assert(bytes_used <= NETWORK_DATA_BUF_SIZE);

  mutex_lock(&network_data_mutex);
  memcpy(network_data_buf, recv_buf, bytes_used); 
  network_data_buf_used_size = bytes_used;
  mutex_unlock(&network_data_mutex);

  xEventGroupSetBits(network_event_group, NETWORK_EVENT_DATA_READY_BIT);
}

static void server_task() {
  int server_socket;
  struct sockaddr_in serv_addr, client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  uint8_t recv_buf[NETWORK_DATA_BUF_SIZE];
  char error_buffer[SERVER_ERROR_BUF_LEN];

  while (true) {
    server_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server_socket < 0) {
      ESP_LOGW(LOG_TAG, "socket(...) call failed");
      vTaskDelay(DELAY_SERVER_ERROR_MS / portTICK_PERIOD_MS);
      continue;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(SERVER_UDP_PORT);
  
    if (bind(server_socket,
             (struct sockaddr*)&serv_addr,
             sizeof(serv_addr))) {
      ESP_LOGW(LOG_TAG, "bind(...) call failed");
      close(server_socket);

      vTaskDelay(DELAY_SERVER_ERROR_MS / portTICK_PERIOD_MS);
      continue;
    }

    memset(recv_buf, 0, NETWORK_DATA_BUF_SIZE);
    while (true) {
      ssize_t bytes = recvfrom(
          server_socket,
          recv_buf,
          NETWORK_DATA_BUF_SIZE,
          0,
          (struct sockaddr *)&client_addr,
          &client_addr_len);

      char* ip_addr_string = inet_ntoa(client_addr.sin_addr);
      ESP_LOGI(LOG_TAG, "Received data from %s", ip_addr_string)

      if (bytes > 0) {
        submit_data(recv_buf, bytes);
      } else if (bytes < 0) {
        strerror_r(errno, error_buffer, SERVER_ERROR_BUF_LEN);
        ESP_LOGW(LOG_TAG, "recvfrom error (%i): %s",
            bytes, error_buffer);
      }
    }

    close(server_socket);
  }

  // Never reached.
  vTaskDelete(NULL);
  return;
}

static void server_task_create(void) {
  configASSERT(xTaskCreate(server_task,
                           SERVER_TASK_NAME,
                           SERVER_TASK_STACK_WORDS,
                           NULL,
                           SERVER_TASK_PRORITY,
                           &task_server_handle) == pdTRUE);
}

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event) {
  switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
      ESP_LOGD(LOG_TAG, "WIFI start ...");
      esp_wifi_connect();
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      ESP_LOGD(LOG_TAG, "WIFI got IP...");
      xEventGroupSetBits(
          network_event_group,
          NETWORK_EVENT_CONNECTED_BIT | NETWORK_EVENT_CHANGE_BIT);
      server_task_create();
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      ESP_LOGD(LOG_TAG, "Disconnected...");
      xEventGroupClearBits(
          network_event_group, NETWORK_EVENT_CONNECTED_BIT);
      xEventGroupSetBits(
          network_event_group, NETWORK_EVENT_CHANGE_BIT);

      /* This is a workaround as ESP32 WiFi libs don't currently
         auto-reassociate. */
      esp_wifi_connect(); 
      break;
    default:
      break;
  }
  return ESP_OK;
}

void networking_setup(void) {
  tcpip_adapter_init();

  network_event_group = xEventGroupCreate();
  configASSERT(network_event_group != NULL);

  ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

  wifi_config_t wifi_config = {
    .sta = {
      .ssid = WIFI_SSID,
      .password = WIFI_PASSWORD,
      .bssid_set = false,
    },
  };

  ESP_LOGI(LOG_TAG, "Setting WiFi configuration SSID %s...",
      wifi_config.sta.ssid);
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
}

void networking_start() {
  ESP_ERROR_CHECK(esp_wifi_start());
}
