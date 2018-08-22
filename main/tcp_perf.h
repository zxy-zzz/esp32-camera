/* tcp_perf Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


#ifndef __TCP_PERF_H__
#define __TCP_PERF_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif


/*test options*/
#define EXAMPLE_ESP_WIFI_MODE_AP CONFIG_TCP_PERF_WIFI_MODE_AP //TRUE:AP FALSE:STA
#define EXAMPLE_ESP_TCP_MODE_SERVER CONFIG_TCP_PERF_SERVER //TRUE:server FALSE:client
#define EXAMPLE_ESP_TCP_PERF_TX CONFIG_TCP_PERF_TX //TRUE:send FALSE:receive
#define EXAMPLE_ESP_TCP_DELAY_INFO CONFIG_TCP_PERF_DELAY_DEBUG //TRUE:show delay time info

/*AP info and tcp_server info*/
#define EXAMPLE_DEFAULT_SSID "Xiaomi_47C5"
#define EXAMPLE_DEFAULT_PWD "00000000"
#define EXAMPLE_DEFAULT_PORT 1234
#define EXAMPLE_DEFAULT_PKTSIZE 32
#define EXAMPLE_MAX_STA_CONN 1 //how many sta can be connected(AP mode)
#define CONFIG_TCP_PERF_TX 1



#ifdef CONFIG_TCP_PERF_SERVER_IP
#define EXAMPLE_DEFAULT_SERVER_IP CONFIG_TCP_PERF_SERVER_IP
#else
#define EXAMPLE_DEFAULT_SERVER_IP "192.168.31.224"
#endif /*CONFIG_TCP_PERF_SERVER_IP*/



#define EXAMPLE_PACK_BYTE_IS 97 //'a'


/* FreeRTOS event group to signal when we are connected to wifi*/
extern EventGroupHandle_t tcp_event_group;
#define WIFI_CONNECTED_BIT BIT0


extern bool g_rxtx_need_restart;

#if EXAMPLE_ESP_TCP_PERF_TX && EXAMPLE_ESP_TCP_DELAY_INFO
extern int g_total_pack;
extern int g_send_success;
extern int g_send_fail;
extern int g_delay_classify[5];
#endif/*EXAMPLE_ESP_TCP_PERF_TX && EXAMPLE_ESP_TCP_DELAY_INFO*/


//create a tcp server socket. return ESP_OK:success ESP_FAIL:error
esp_err_t create_tcp_server();
//create a tcp client socket. return ESP_OK:success ESP_FAIL:error
esp_err_t create_tcp_client();

//send data task
void send_data(void *pvParameters);
//receive data task
void recv_data(void *pvParameters);

//close all socket
void close_socket();

//get socket error code. return: error code
int get_socket_error_code(int socket);

//show socket error code. return: error code
int show_socket_error_reason(const char* str, int socket);

//check working socket
int check_working_socket();


#ifdef __cplusplus
}
#endif


#endif /*#ifndef __TCP_PERF_H__*/

