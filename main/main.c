/* SPIFFS filesystem example.
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "camera.h"
#include "bitmap.h"
#include "esp_wifi.h"
#include "tcp_perf.h"
#include "esp_event_loop.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"


#define CAMERA_LED_GPIO 16

static const char *TAG = "example";

static camera_pixelformat_t s_pixel_format;
// 设置相机的尺寸 分辨率:  800x600 (见 camera.h camera_framesize_t)
// 可以自行修改分辨率.
#define CAMERA_FRAME_SIZE CAMERA_FS_SVGA


//#define EXAMPLE_ESP_WIFI_SSID "ChinaNet-jMCy"
//#define EXAMPLE_ESP_WIFI_PASS "kykv3jik"
#define EXAMPLE_ESP_WIFI_SSID "Xiaomi_47C5"
#define EXAMPLE_ESP_WIFI_PASS "00000000"
#define CAMERA_LED_GPIO 16

static EventGroupHandle_t s_wifi_event_group;
const int CONNECTED_BIT = BIT0;
static ip4_addr_t s_ip_addr;

static void wifi_init_sta();
static esp_err_t event_handler(void* ctx, system_event_t* event);
static void tcp_conn(void *pvParameters);

void app_main(void)
{
	esp_log_level_set("wifi", ESP_LOG_WARN);
	esp_log_level_set("gpio", ESP_LOG_WARN);

	esp_err_t err = nvs_flash_init();
	if (err != ESP_OK) {
		ESP_ERROR_CHECK( nvs_flash_erase() );
		ESP_ERROR_CHECK( nvs_flash_init() );
		ESP_LOGI(TAG, "Flash problem");
	}

    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    gpio_set_direction(CAMERA_LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(CAMERA_LED_GPIO, 0);

    camera_config_t camera_config = {
        .ledc_channel = LEDC_CHANNEL_0,
        .ledc_timer = LEDC_TIMER_0,
        .pin_d0 = CONFIG_D0,
        .pin_d1 = CONFIG_D1,
        .pin_d2 = CONFIG_D2,
        .pin_d3 = CONFIG_D3,
        .pin_d4 = CONFIG_D4,
        .pin_d5 = CONFIG_D5,
        .pin_d6 = CONFIG_D6,
        .pin_d7 = CONFIG_D7,
        .pin_xclk = CONFIG_XCLK,
        .pin_pclk = CONFIG_PCLK,
        .pin_vsync = CONFIG_VSYNC,
        .pin_href = CONFIG_HREF,
        .pin_sscb_sda = CONFIG_SDA,
        .pin_sscb_scl = CONFIG_SCL,
        .pin_reset = CONFIG_RESET,
        .xclk_freq_hz = CONFIG_XCLK_FREQ,
    };

    camera_model_t camera_model;
    err = camera_probe(&camera_config, &camera_model);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera probe failed with error 0x%x", err);
        return;
    }

    if (camera_model == CAMERA_OV2640) {
        // 检测到相机模式为 CAMERA_OV2640
        // 即相机的感光芯片为 OV2640
        ESP_LOGI(TAG, "Detected OV2640 camera, using JPEG format");
        // 设定像素格式为JPEG图像(编号 3)
        s_pixel_format = CAMERA_PF_JPEG;
        // 设定图像的分辨率
        camera_config.frame_size = CAMERA_FRAME_SIZE;
        // 设定jpg图像的质量
        camera_config.jpeg_quality = 10;
    } else {
        ESP_LOGE(TAG, "Camera not supported");
        return;
    }

    camera_config.pixel_format = s_pixel_format;
    err = camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        return;
    }


    ESP_LOGI(TAG, "Initializing SPIFFS");
    
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };
    
    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%d)", ret);
        }
        return;
    }
    
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information");
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

	ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
	wifi_init_sta();

}

static void wifi_init_sta()
{
    s_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .sta = {.ssid = EXAMPLE_ESP_WIFI_SSID, .password = EXAMPLE_ESP_WIFI_PASS},
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s", EXAMPLE_ESP_WIFI_SSID,
            EXAMPLE_ESP_WIFI_PASS);

    xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
    xTaskCreate(&tcp_conn, "tcp_conn", 4096, NULL, 5, NULL);
}

static void tcp_conn(void *pvParameters)
{
    while (1) {

        g_rxtx_need_restart = false;

        ESP_LOGI(TAG, "task tcp_conn.");


        ESP_LOGI(TAG, "sta has connected to ap.");

        int socket_ret = ESP_FAIL;

        TaskHandle_t tx_task = NULL;
        TaskHandle_t rx_task = NULL;

        if (socket_ret == ESP_FAIL) {
            ESP_LOGI(TAG, "tcp_client will start after 2s...");
            vTaskDelay(2000 / portTICK_RATE_MS);
            ESP_LOGI(TAG, "create_tcp_client.");
            socket_ret = create_tcp_client();
        }

        if (socket_ret == ESP_FAIL) {
            ESP_LOGI(TAG, "create tcp socket error,stop.");
            continue;
        }

        /*create a task to tx/rx data*/

//#if EXAMPLE_ESP_TCP_PERF_TX
        if (tx_task == NULL) {
            if (pdPASS != xTaskCreate(&send_data, "send_data", 4096, NULL, 4, &tx_task)) {
                ESP_LOGE(TAG, "Send task create fail!");
            }
        }
//#else /*EXAMPLE_ESP_TCP_PERF_TX*/
        if (rx_task == NULL) {
            if (pdPASS != xTaskCreate(&recv_data, "recv_data", 4096, NULL, 4, &rx_task)) {
                ESP_LOGE(TAG, "Recv task create fail!");
            }
        }
//#endif


        while (1) {

            vTaskDelay(3000 / portTICK_RATE_MS);//every 3s


            if (g_rxtx_need_restart) {
                printf("send or receive task encoutner error, need to restart\n");
                break;
            }


        }

        close_socket();
    }

    vTaskDelete(NULL);
}

static esp_err_t event_handler(void* ctx, system_event_t* event)
{
  switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
      esp_wifi_connect();
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      ESP_LOGI(TAG, "got ip:%s", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
      s_ip_addr = event->event_info.got_ip.ip_info.ip;
      xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
      break;
    case SYSTEM_EVENT_AP_STACONNECTED:
      ESP_LOGI(TAG, "station:" MACSTR " join, AID=%d", MAC2STR(event->event_info.sta_connected.mac),
               event->event_info.sta_connected.aid);
#if EXAMPLE_ESP_WIFI_MODE_AP
      xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
#endif
      break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
      ESP_LOGI(TAG, "station:" MACSTR "leave, AID=%d", MAC2STR(event->event_info.sta_disconnected.mac),
               event->event_info.sta_disconnected.aid);
#if EXAMPLE_ESP_WIFI_MODE_AP
      xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
#endif
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      esp_wifi_connect();
      xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
      break;
    default:
      break;
  }
  return ESP_OK;
}

