/* tcp_perf Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <sys/socket.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "tcp_perf.h"
#include "camera.h"
#include "bitmap.h"
#include "camera_common.h"

/* FreeRTOS event group to signal when we are connected to wifi */
EventGroupHandle_t tcp_event_group;

/*socket*/
static int server_socket = 0;
static struct sockaddr_in server_addr;
static struct sockaddr_in client_addr;
static unsigned int socklen = sizeof(client_addr);
static int connect_socket = 0;
bool g_rxtx_need_restart = false;



#if EXAMPLE_ESP_TCP_PERF_TX && EXAMPLE_ESP_TCP_DELAY_INFO

int g_total_pack = 0;
int g_send_success = 0;
int g_send_fail = 0;
int g_delay_classify[5] = { 0 };

#endif /*EXAMPLE_ESP_TCP_PERF_TX && EXAMPLE_ESP_TCP_DELAY_INFO*/

extern camera_state_t* s_state;

static const char* TAG = "tcp_perf";



//send data
void send_data(void *pvParameters)
{
    int len = 0;
    ESP_LOGI(TAG, "start sending...");
    while (1) {
        //send function
    	esp_err_t err = camera_run();
		if (err != ESP_OK) {
			ESP_LOGI(TAG, "Camera capture failed with error = %d", err);
			return;
		}

		len = send(connect_socket, camera_get_fb(), camera_get_data_size(), 0);
		len += send(connect_socket, "pictureend", 10, 0);
		if (len > 0) {

			//vTaskDelay(3000 / portTICK_RATE_MS);//every 3s
		} else {
			int err = get_socket_error_code(connect_socket);
			if (err != ENOMEM) {
				show_socket_error_reason("send_data", connect_socket);
				break;
			}
		}
    }
    ESP_LOGI(TAG, "end sending...");
    g_rxtx_need_restart = true;
    vTaskDelete(NULL);
}

//receive data
void recv_data(void *pvParameters)
{
    int len = 0;
    char *databuff = (char *)malloc(EXAMPLE_DEFAULT_PKTSIZE * sizeof(char));
    while (1) {
        int to_recv = EXAMPLE_DEFAULT_PKTSIZE;
        while (to_recv > 0) {
        	vTaskDelay(10 / portTICK_RATE_MS);//every 3s
            len = recv(connect_socket, databuff + (EXAMPLE_DEFAULT_PKTSIZE - to_recv), to_recv, 0);
            if (len > 0) {
                to_recv -= len;
                ESP_LOGI(TAG, "%s", databuff);
            } else {
                show_socket_error_reason("recv_data", connect_socket);
                break;
            }
        }
//        if(strstr(databuff,"takephoto"))
//        {
//        	esp_err_t err = camera_run();
//			if (err != ESP_OK) {
//				ESP_LOGI(TAG, "Camera capture failed with error = %d", err);
//				return;
//			}
//			else
//			{
//				ESP_LOGI(TAG, "Camera capture successed");
//			}
//        }

    }


    free(databuff);
    vTaskDelete(NULL);
}


//use this esp32 as a tcp client. return ESP_OK:success ESP_FAIL:error
esp_err_t create_tcp_client()
{
    ESP_LOGI(TAG, "client socket....serverip:port=%s:%d\n",
             EXAMPLE_DEFAULT_SERVER_IP, EXAMPLE_DEFAULT_PORT);
    connect_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (connect_socket < 0) {
        show_socket_error_reason("create client", connect_socket);
        return ESP_FAIL;
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(EXAMPLE_DEFAULT_PORT);
    server_addr.sin_addr.s_addr = inet_addr(EXAMPLE_DEFAULT_SERVER_IP);
    ESP_LOGI(TAG, "connecting to server...");
    if (connect(connect_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        show_socket_error_reason("client connect", connect_socket);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "connect to server success!");
    return ESP_OK;
}

int get_socket_error_code(int socket)
{
    int result;
    u32_t optlen = sizeof(int);
    int err = getsockopt(socket, SOL_SOCKET, SO_ERROR, &result, &optlen);
    if (err == -1) {
        ESP_LOGE(TAG, "getsockopt failed:%s", strerror(err));
        return -1;
    }
    return result;
}

int show_socket_error_reason(const char *str, int socket)
{
    int err = get_socket_error_code(socket);

    if (err != 0) {
        ESP_LOGW(TAG, "%s socket error %d %s", str, err, strerror(err));
    }

    return err;
}

int check_working_socket()
{
    int ret;
    ESP_LOGD(TAG, "check connect_socket");
    ret = get_socket_error_code(connect_socket);
    if (ret != 0) {
        ESP_LOGW(TAG, "connect socket error %d %s", ret, strerror(ret));
    }
    if (ret != 0) {
        return ret;
    }
    return 0;
}

void close_socket()
{
    close(connect_socket);
    close(server_socket);
}

