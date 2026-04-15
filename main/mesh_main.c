/* Mesh Internal Communication Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <inttypes.h>
#include "esp_wifi.h"
#include "esp_coexist.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mesh.h"
#include "nvs_flash.h"
#include "mesh_netif.h"
#include "driver/gpio.h"
#include "freertos/semphr.h"

//--shcho-------------------------------------------
//  #include <stdio.h>
//  #include <string.h>
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "esp_heap_caps.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "ethernet_init.h"
#include "sdkconfig.h"
extern void hexdump3(char *title, void *pack, size_t size) ;
extern uint8_t* mesh_netif_get_station_mac(void);
extern uint8_t* mesh_netif_get_rootnode_mac(void);

/* MQTT 라이브러리 내부 태스크 TCB 할당을 위한 내부 SRAM 예약 블록.
 * app_main() 시작 직후 할당 → 메모리 파편화 전에 연속 블록 확보.
 * mqtt_app_start()에서 esp_mqtt_client_start() 직전 해제.            */
uint8_t *g_mqtt_internal_sram_reserve = NULL;
extern void app_main_wifi_scan();
extern void app_main_stella(void);
extern void mqtt_app_reconnect(void);
extern bool mqtt_app_has_client(void);
extern void app_main_nimble_sec(void);
extern void ade9153a_reset(void);
#include "nimble/nimble_port.h"
extern uint32_t  SPI_Read_32(spi_device_handle_t spi,  uint16_t  Address);
extern uint32_t read_ade9153a_wav(void);
//--shcho-------------------------------------------
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "protocol_examples_common.h"
#include "string.h"
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>
#include "qrcode.h"

//  #include <stdio.h>
//  #include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <net/if.h>
//  #include "freertos/FreeRTOS.h"
//  #include "freertos/task.h"
#include "driver/uart.h"
#include "esp_app_format.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "light_sleep_example.h"
#include "esp_check.h"
//  #include "esp_sleep.h"

#ifdef CONFIG_EXAMPLE_USE_CERT_BUNDLE
	#include "esp_crt_bundle.h"
#endif

//-----------------------------------------------------
// add for console : 2024_0607
#include "cmd_decl.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "esp_vfs_fat.h"
#include "mesh_main_iotech.h"

#include "iotech_global.h"
#include "stella_global.h"
//-----------------------------------------------------
//-----------------------------------------------------
#include "esp_chip_info.h"
#include <cJSON.h>
//-----------------------------------------------------
#include  "spifss.h"






#define STR_MATCH	(0)

uint8_t my_mac_factory[20];
#define OTA_URL_SIZE 256
uint8_t ota_url[OTA_URL_SIZE];
#define STORAGE_NAMESPACE "storage"
#define MESH_AP_SSID_DEFAULT "iJoonAP_2G_2"
#define MESH_AP_PASSWD_DEFAULT "ipcadasic"
uint8_t mesh_ap_ssid_current_connected[32]; // mesh_router structure limit
uint8_t mesh_ap_ssid[32]; // mesh_router structure limit
uint8_t mesh_ap_passwd[64]; // mesh_router structure limit
uint8_t myip_str[32];           // 20 =>32
// 2024-09-12  cho  
uint8_t mygw_str[32];           // 32
uint8_t myntp_str[32];          
char    g_hostname[128];
char    mqtt_topic_prefix[100];
int16_t count_no_parent=0;


#include "nvs.h"
#include "protocol_examples_common.h"
#include <sys/socket.h>
#include "cJSON.h"

// 24-06-29
// shlee
#include  "ade9153a_spi.h"
// 24-07-03
#include  "rtc_time.h"
#include  "meter_app.h"

#if CONFIG_EXAMPLE_CONNECT_WIFI
#include "esp_wifi.h"
#endif

#define HASH_LEN 32

#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF
	/* The interface name value can refer to if_desc in esp_netif_defaults.h */
	#if CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF_ETH
		static const char *bind_interface_name = EXAMPLE_NETIF_DESC_ETH;
	#elif CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF_STA
		static const char *bind_interface_name = EXAMPLE_NETIF_DESC_STA;
	#endif
#endif

typedef struct _mesh_wifi_info
{
	char cmd;
	char ssid[32];
	char passwd[64];
	char ntpserver[32];
	char mqtt_broker_uri[128];
}stMeshWifiInfo;

static const char *TAG_OTA = "simple_ota_example";
extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

char   num[2]= "1"; 
char   device_id[DEVICE_ID_LEN] = "3W12345";
char   fw_version[32] = "v1.0.1"; 
char   avccValue[10] ="0";
char   aiccValue[10] ="0";

extern char  str_rated_voltage[];
extern char  str_rated_current[];
extern char  str_swell_voltage[];
extern char  str_dip_voltage[];
extern char  str_over_current[];
extern char  str_warning_duration[];
extern char  str_relay[];
extern char  str_rated_freq[];

//org:  bool         prev_relay_state;
//org:  extern  bool relay_state;
//shcho
int         prev_relay_state = -2;
extern  int relay_state;

extern  uint16_t  swell_voltage;
extern  uint16_t  dip_voltage;
extern  uint16_t  rated_freq;

// 큐 핸들 선언
QueueHandle_t txQue;


esp_err_t  nvs_set_mesh_ap_ssid_passwd(uint8_t *ssid, uint8_t *passwd);
void nvs_get_mesh_ap_ssid_passwd(uint8_t *ssid, uint8_t *passwd);

esp_err_t iotech_set_nvs_str(uint8_t *key, uint8_t *value);
esp_err_t iotech_get_nvs_str(uint8_t *key, uint8_t *passwd);
extern void app_main_tcp_server(void);
extern void app_main_light_sleep(void);
extern void app_main_continuous_read_adc(void *pvParameters);
extern void app_main_sntp(void *pvParameters);
extern void mqtt_app_publish(char* topic, char *publish_string);
extern char *stella_ota_progress_topic;
extern char *stella_ota_result_topic;

void  register_date(void);

extern int show_adc;
extern int adc_suspend ;
TaskHandle_t xHandle_adc = NULL;
extern void esp_deep_sleep(uint64_t time_in_us);
extern void esp_light_sleep(uint64_t time_in_us);
extern char mqtt_broker_uri[128];
extern int flag_IS_WEARABLE;
/** 웨어러블: 메시 없이 공유기 STA만 사용할 때 true (esp_mesh_* 호출 금지) */
static bool g_stella_wearable_plain_sta;

static int do_tasks_info(int argc, char **argv) ;
esp_err_t iotech_get_nvs_str(uint8_t *key, uint8_t *value);
esp_err_t  iotech_set_nvs_str(uint8_t *key, uint8_t *value);
void nvs_get_ntp_server(uint8_t *ntpserver);
void check_ADE9053a(void);

extern void wifi_scan_best_rssi_among_Iotech_Router(char *ssid);


// define in main/include/iotech_global.h
//  #define TCP_SERVER_TEST_ONLY (1)

#define OTA_URL_SIZE 256

#define RX_SIZE_TMP          (1500)
#define TX_SIZE_TMP          (1460)
uint8_t tx_buf_tmp[TX_SIZE_TMP] = { 0, };
//  	uint8_t rx_buf_tmp[RX_SIZE_TMP] = { 0, };

//  esp_err_t iotech_set_hostname(char *hostname)
esp_err_t stella_set_hostname(char *hostname)
{
	esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
	esp_err_t          err = esp_netif_set_hostname(sta_netif, g_hostname);

	sta_netif              = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
	err                    = esp_netif_set_hostname(sta_netif, g_hostname);

	return err;
}

/* OTA HTTP 진단: 기본 로그 레벨(INFO)에서도 연결·헤어·수신 진행·종료를 볼 수 있게 함.
 * ON_DATA는 청크마다 INFO로 찍지 않고 첫 청크 + 256KB 단위 누적만 INFO. */
static uint32_t s_ota_http_rx_total;

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        if (evt->client) {
            int errn = esp_http_client_get_errno(evt->client);
            ESP_LOGW(TAG_OTA, "HTTP_EVENT_ERROR errno=%d (%s)", errn, strerror(errn));
        } else {
            ESP_LOGW(TAG_OTA, "HTTP_EVENT_ERROR");
        }
        break;
    case HTTP_EVENT_ON_CONNECTED:
        s_ota_http_rx_total = 0;
        ESP_LOGI(TAG_OTA, "HTTP_EVENT_ON_CONNECTED (TCP up, awaiting HTTP response)");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGI(TAG_OTA, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGI(TAG_OTA, "HTTP_EVENT_ON_HEADER key=%s value=%s",
                 evt->header_key ? evt->header_key : "(null)",
                 evt->header_value ? evt->header_value : "(null)");
        break;
    case HTTP_EVENT_ON_DATA: {
        int len = evt->data_len;
        if (len <= 0) {
            break;
        }
        uint32_t prev = s_ota_http_rx_total;
        s_ota_http_rx_total += (uint32_t)len;
        if (prev == 0) {
            ESP_LOGI(TAG_OTA, "HTTP_EVENT_ON_DATA first chunk len=%d (payload started)", len);
        }
        const uint32_t step = 256U * 1024U;
        if (prev / step != s_ota_http_rx_total / step) {
            ESP_LOGI(TAG_OTA, "HTTP_EVENT_ON_DATA cumulative rx=%lu bytes", (unsigned long)s_ota_http_rx_total);
        }
        ESP_LOGD(TAG_OTA, "HTTP_EVENT_ON_DATA len=%d total=%lu", len, (unsigned long)s_ota_http_rx_total);
        break;
    }
    case HTTP_EVENT_ON_FINISH:
        if (evt->client) {
            int status = esp_http_client_get_status_code(evt->client);
            int64_t content_len = esp_http_client_get_content_length(evt->client);
            ESP_LOGI(TAG_OTA, "HTTP_EVENT_ON_FINISH status=%d Content-Length=%lld rx_total=%lu",
                     status, (long long)content_len, (unsigned long)s_ota_http_rx_total);
        } else {
            ESP_LOGI(TAG_OTA, "HTTP_EVENT_ON_FINISH rx_total=%lu", (unsigned long)s_ota_http_rx_total);
        }
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG_OTA, "HTTP_EVENT_DISCONNECTED (rx_total=%lu)", (unsigned long)s_ota_http_rx_total);
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGI(TAG_OTA, "HTTP_EVENT_REDIRECT");
        break;
    }
    return ESP_OK;
}

int flag_ota_run = 0 ;

volatile int stella_sensors_paused_for_ota = 0;

/* OTA begin/perform 실패 시 무한 재시도하면 센서 일시정지가 풀리지 않음(mqtt도 정지 상태라 ota_run=0 수신 불가) */
#ifndef STELLA_OTA_MAX_TOTAL_ATTEMPTS
#define STELLA_OTA_MAX_TOTAL_ATTEMPTS 30
#endif

void stella_wait_while_ota_sensors_paused(void)
{
	while (stella_sensors_paused_for_ota) {
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

/* RTC_NOINIT: 소프트웨어 리셋 후에도 값 유지 (전원 OFF 시 초기화) */
static RTC_NOINIT_ATTR uint32_t ota_success_flag;
#define OTA_SUCCESS_MAGIC  0xA55AA55A

void simple_ota_example_task(void *pvParameter)
{
    char print[6*3+1+20];
    extern void mqtt_app_stop(void);
    extern void mqtt_app_start(void);

	while(1)
	{
		if( flag_ota_run == 1 )
		{
        stella_sensors_paused_for_ota = 1;
        ESP_LOGW(TAG_OTA, "Sensors paused for OTA (heap relief)");
        vTaskDelay(pdMS_TO_TICKS(50));

		    ESP_LOGW(TAG_OTA, "\n\n\t Starting OTA example task\n\n\n");

		snprintf(print, sizeof(print),"%02x%02x%02x_%02x%02x%02x", MAC2STR(my_mac_factory));

        extern int flag_mqtt_connect;
        if (stella_ota_progress_topic && flag_mqtt_connect) {
            mqtt_app_publish(stella_ota_progress_topic, "{\"progress\":0,\"status\":\"started\"}");
            vTaskDelay(500 / portTICK_PERIOD_MS);
        } else {
            ESP_LOGW(TAG_OTA, "MQTT not connected, skip 'started' publish");
        }

        mqtt_app_stop();
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        ESP_LOGW(TAG_OTA, "OTA: plain STA mode — direct connection to AP");

        /* ── STA 바인딩 (항상 공유기 직결) ── */
	    struct ifreq ota_bind_ifr;
	    memset(&ota_bind_ifr, 0, sizeof(ota_bind_ifr));
	    struct ifreq *ota_if_name_ptr = NULL;
#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF
	    esp_netif_t *netif = get_example_netif_from_desc(bind_interface_name);
	    if (netif == NULL) {
	        ESP_LOGE(TAG_OTA, "Can't find netif from interface description");
	        abort();
	    }
	    esp_netif_get_netif_impl_name(netif, ota_bind_ifr.ifr_name);
	    ESP_LOGI(TAG_OTA, "OTA bind (sdkconfig): if=%s", ota_bind_ifr.ifr_name);
	    ota_if_name_ptr = &ota_bind_ifr;
#else
	    {
	        esp_netif_t *sta_nf = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
	        if (sta_nf != NULL && esp_netif_get_netif_impl_name(sta_nf, ota_bind_ifr.ifr_name) == ESP_OK) {
	            ESP_LOGI(TAG_OTA, "OTA HTTP bind to STA if=%s", ota_bind_ifr.ifr_name);
	            ota_if_name_ptr = &ota_bind_ifr;
	        } else {
	            ESP_LOGW(TAG_OTA, "OTA: STA if bind skipped");
	        }
	    }
#endif

	    esp_http_client_config_t config = {
		        .url = (char*)ota_url,
		        .event_handler = _http_event_handler,
		        .keep_alive_enable = false,
		        .timeout_ms = 60000,
		        .buffer_size = 4096,
		        .buffer_size_tx = 1024,
		        .if_name = ota_if_name_ptr,
#ifdef CONFIG_EXAMPLE_USE_CERT_BUNDLE
		        .crt_bundle_attach = esp_crt_bundle_attach,
#endif
		    };

		    esp_https_ota_config_t ota_config = {
		        .http_config = &config,
		        .partial_http_download = 0,
		    };
		ESP_LOGW(TAG_OTA, "OTA URL: %s", config.url);
		ESP_LOGW(TAG_OTA, "heap after mqtt_stop: free=%lu int=%lu int_largest=%lu",
		         (unsigned long)esp_get_free_heap_size(),
		         (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
		         (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
	
		int count = 0;
		bool ota_done = false;

		while (!ota_done)
		{
			if (flag_ota_run != 1) break;
			if (count >= STELLA_OTA_MAX_TOTAL_ATTEMPTS) {
				ESP_LOGE(TAG_OTA, "OTA: max attempts (%d) — aborting (unpause sensors, restart MQTT)",
				         STELLA_OTA_MAX_TOTAL_ATTEMPTS);
				break;
			}

			ESP_LOGW(TAG_OTA, "OTA attempt %d: %s", count, config.url);

			/* ── 단계별 OTA: 실시간 진행률 보고 ── */
			esp_https_ota_handle_t ota_handle = NULL;
			ESP_LOGW(TAG_OTA, "heap before esp_https_ota_begin: int=%lu int_largest=%lu",
			         (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
			         (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
			esp_err_t begin_ret = esp_https_ota_begin(&ota_config, &ota_handle);
			if (begin_ret != ESP_OK || ota_handle == NULL) {
				ESP_LOGE(TAG_OTA, "esp_https_ota_begin failed (try=%d): %s", count, esp_err_to_name(begin_ret));
				count++;
				if (count % 10 == 0)
					ESP_LOGW(TAG_OTA, "OTA retry count=%d (still trying...)", count);
				vTaskDelay(5000 / portTICK_PERIOD_MS);
				continue;
			}

			int image_size = esp_https_ota_get_image_size(ota_handle);
			int last_pct = -1;
			esp_err_t perform_ret = ESP_OK;

			while (1) {
				perform_ret = esp_https_ota_perform(ota_handle);
				if (perform_ret != ESP_ERR_HTTPS_OTA_IN_PROGRESS) break;

				/* MQTT가 꺼진 상태이므로 진행률은 로그로만 출력 */
				if (image_size > 0) {
					int written = esp_https_ota_get_image_len_read(ota_handle);
					int pct = (written * 100) / image_size;
					pct = (pct / 5) * 5;
					if (pct != last_pct) {
						last_pct = pct;
						ESP_LOGI(TAG_OTA, "OTA progress: %d%%", pct);
					}
				}
			}

		if (perform_ret == ESP_OK && esp_https_ota_is_complete_data_received(ota_handle)) {
			esp_err_t finish_ret = esp_https_ota_finish(ota_handle);
			ota_handle = NULL;
			if (finish_ret == ESP_OK) {
				ESP_LOGW(TAG_OTA, "\n\n\tOTA Succeed! Saving flag and rebooting...\n\n");
				/* RTC 메모리에 성공 플래그 저장 → 재부팅 후 MQTT 연결되면 발행 */
				ota_success_flag = OTA_SUCCESS_MAGIC;
				flag_ota_run = 0;
				ota_done = true;
				vTaskDelay(500 / portTICK_PERIOD_MS);
				esp_restart();
				} else {
					ESP_LOGE(TAG_OTA, "esp_https_ota_finish failed: %s", esp_err_to_name(finish_ret));
				}
			} else {
				ESP_LOGE(TAG_OTA, "OTA incomplete or perform error (try=%d): %s", count, esp_err_to_name(perform_ret));
				if (ota_handle) {
					esp_https_ota_abort(ota_handle);
					ota_handle = NULL;
				}
			}

			if (!ota_done) {
				count++;
				if (count % 10 == 0)
					ESP_LOGW(TAG_OTA, "OTA retry count=%d (still trying...)", count);
				vTaskDelay(5000 / portTICK_PERIOD_MS);
			}
		}

		flag_ota_run = 0;
		memset(ota_url, 0, sizeof(ota_url));

		mqtt_app_start();
		stella_sensors_paused_for_ota = 0;
		vTaskDelay(2000 / portTICK_PERIOD_MS);
		if (stella_ota_result_topic)
			mqtt_app_publish(stella_ota_result_topic, "{\"success\":false,\"note\":\"cancelled or failed\"}");
		}
		else
		{
	        vTaskDelay(500 / portTICK_PERIOD_MS);
		}

//      while (1) {
//          vTaskDelay(1000 / portTICK_PERIOD_MS);
//      }
	}
}

static void print_sha256(const uint8_t *image_hash, const char *label)
{
    char hash_print[HASH_LEN * 2 + 1];
    hash_print[HASH_LEN * 2] = 0;
    for (int i = 0; i < HASH_LEN; ++i) {
        sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
    }
    ESP_LOGI(TAG_OTA, "%s %s", label, hash_print);
}

static void get_sha256_of_partitions(void)
{
    uint8_t sha_256[HASH_LEN] = { 0 };
    esp_partition_t partition;

    // get sha256 digest for bootloader
    partition.address   = ESP_BOOTLOADER_OFFSET;
    partition.size      = ESP_PARTITION_TABLE_OFFSET;
    partition.type      = ESP_PARTITION_TYPE_APP;
    esp_partition_get_sha256(&partition, sha_256);
    print_sha256(sha_256, "SHA-256 for bootloader: ");

    // get sha256 digest for running partition
    esp_partition_get_sha256(esp_ota_get_running_partition(), sha_256);
    print_sha256(sha_256, "SHA-256 for current firmware: ");
}


/*******************************************************
 *                Macros
 *******************************************************/
//  #define EXAMPLE_BUTTON_GPIO     0
#define EXAMPLE_BUTTON_GPIO     4

// commands for internal mesh communication:
// <CMD> <PAYLOAD>, where CMD is one character, payload is variable dep. on command
#define CMD_KEYPRESSED 0x55
// CMD_KEYPRESSED: payload is always 6 bytes identifying address of node sending keypress event
#define CMD_ROUTE_TABLE 0x56
//shcho add
#define CMD_WIFI_INFO   0x57


// CMD_KEYPRESSED: payload is a multiple of 6 listing addresses in a routing table
/*******************************************************
 *                Constants
 *******************************************************/
static const char *MESH_TAG = "mesh_main";

#if (TCP_SERVER_TEST_ONLY == 0 ) 
//  //  static const uint8_t MESH_ID[6] = { 0x77, 0x77, 0x77, 0x77, 0x77, 0x76};
// IoTech : 임시
//  static const uint8_t MESH_ID[6] = { 0x77, 0x7f, 0x7d, 0x7e, 0x77, 0x76};
// shcho : iotech_default
//    static const uint8_t MESH_ID[6] = { 0x88, 0x7f, 0x7d, 0x7e, 0x77, 0x76};
//shcho test
static const uint8_t MESH_ID[6] = { 0x78, 0x7f, 0x7d, 0x7e, 0xff, 0x76};
#endif

/*******************************************************
 *                Variable Definitions
 *******************************************************/
static bool is_running = true;
static mesh_addr_t mesh_parent_addr;
static int mesh_layer = -1;
static esp_ip4_addr_t s_current_ip;
static mesh_addr_t s_route_table[CONFIG_MESH_ROUTE_TABLE_SIZE];
static int s_route_table_size = 0;
static SemaphoreHandle_t s_route_table_lock = NULL;
static uint8_t s_mesh_tx_payload[CONFIG_MESH_ROUTE_TABLE_SIZE*6+1];


/*******************************************************
 *                Function Declarations
 *******************************************************/
// interaction with public mqtt broker
void mqtt_app_start(void);
void mqtt_app_publish(char* topic, char *publish_string);

/*******************************************************
 *                Function Definitions
 *******************************************************/

static void initialise_button(void)
{
    gpio_config_t io_conf = {0};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = BIT64(EXAMPLE_BUTTON_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    io_conf.pull_down_en = 0;
    gpio_config(&io_conf);
}

void static __attribute__((unused)) recv_cb(mesh_addr_t *from, mesh_data_t *data)
{
	mesh_light_ctl_t light_on_off;
	union _ch4_int
	{
		uint8_t ch[4];
		uint32_t i;
	};
	union _ch4_int ch4_int;

	int reboot_flag = 0 ; 

// 모두 자기가 보낸 것으로 인식하네??
//   	if (MAC_ADDR_EQUAL(from->addr, my_mac_factory)) 
//  	{
//  		ESP_LOGE("recv_cb", "\n\npacket recved from myself: %2x%02x%02x_%02x%02x%02x\n\n", MAC2STR(from->addr));
//  		return ;
//  	}

	ESP_LOGI("recv_cb", "                           macaddr_from: %2X%02X%02X_%02X%02X%02X", MAC2STR(from->addr));
	memcpy( (char *)&ch4_int, (char *)&from->mip.ip4.addr, 4);
//  	ESP_LOGI("recv_cb(ip  )", "not perfect it is union : ip_from: " IPSTR , ch4_int.ch[3], ch4_int.ch[2], ch4_int.ch[1], ch4_int.ch[0]);
//  	ESP_LOGI("recv_cb(port)", "not perfect it is union :    port: %d", from->mip.port );
	hexdump3("recv_cb(from)", from, sizeof(mesh_addr_t));

    if (data->data[0] == CMD_ROUTE_TABLE) {
        ESP_LOGW(MESH_TAG, "CMD_ROUTE_TABLE==================================");
        int size =  data->size - 1;
        if (s_route_table_lock == NULL || size%6 != 0) {
            ESP_LOGE(MESH_TAG, "Error in receiving raw mesh data: Unexpected size");
            return;
        }
        xSemaphoreTake(s_route_table_lock, portMAX_DELAY);
        s_route_table_size = size / 6;
        for (int i=0; i < s_route_table_size; ++i) {
            ESP_LOGI(MESH_TAG, "Received Routing table   [%d] "
                    MACSTR, i, MAC2STR(data->data + 6*i + 1));
        }
        memcpy(&s_route_table, data->data + 1, size);
        xSemaphoreGive(s_route_table_lock);
    } else if (data->data[0] == CMD_KEYPRESSED) {
        ESP_LOGW(MESH_TAG, "CMD_KEYPRESSED==================================");
        if (data->size != 7) {
            ESP_LOGE(MESH_TAG, "Error in receiving raw mesh data: Unexpected size");
            return;
        }
        ESP_LOGW(MESH_TAG, "Keypressed detected on node: "
                MACSTR, MAC2STR(data->data + 1));
    } else if (data->data[0] == MESH_CONTROL_CMD) {
        ESP_LOGW(MESH_TAG, "MESH_CONTROL_CMD==================================");
		hexdump3("mesh_data(recv_cb)", data, sizeof(mesh_data_t));
//  		hexdump3("mesh_data__data(recv_cb)", data->data, data->size );

		memcpy((char *)&light_on_off, data->data, sizeof(mesh_light_ctl_t));

		printf("---(recv_cb)-------------------------------------------------\n");
		printf("light_on_off.cmd=%d\n", light_on_off.cmd);
		printf("light_on_off.on=%d\n", light_on_off.on);
		printf("light_on_off.token_id=%d\n", light_on_off.token_id);
		printf("light_on_off.token_value=0x%4x\n", light_on_off.token_value);
		printf("---- light_on_off disabled to view \n");
		hexdump3("light_on_off(recv_cb)", (char *)&light_on_off,sizeof(mesh_light_ctl_t));

    } else if (data->data[0] == CMD_WIFI_INFO) {
		stMeshWifiInfo MESH_wifi_info;
		uint8_t buf_ssid[32];
		uint8_t buf_passwd[64];
		uint8_t buf_ntp_broker[128];
		esp_err_t err;

		if( esp_mesh_is_root() == 1 )
		{
	        ESP_LOGW(MESH_TAG, " I am Root Node : So No need to apply this Information(CMD_WIFI_INFO) ======");
		}
		else
		{
			memset((char *)&MESH_wifi_info, 0, sizeof(MESH_wifi_info));
			memcpy((char *)&MESH_wifi_info,    data->data, data->size);

			hexdump3("MESH_wifi_info @ recv_cb", &MESH_wifi_info, data->size);
			ESP_LOGW(MESH_TAG, "CMD_WIFI_INFO==================================");
	        ESP_LOGW(MESH_TAG, "WiFi Info from Root node: ssid : %s", MESH_wifi_info.ssid); 
	        ESP_LOGW(MESH_TAG, "                        passwd : %s", MESH_wifi_info.passwd); 
	        ESP_LOGW(MESH_TAG, "                     ntpserver : %s", MESH_wifi_info.ntpserver); 
	        ESP_LOGW(MESH_TAG, "               mqtt_broker_uri : %s", MESH_wifi_info.mqtt_broker_uri); 
	        ESP_LOGW(MESH_TAG, "");

			//----------- for wifi ssid/passwd :for networking ---------------------------
	
			nvs_get_mesh_ap_ssid_passwd((uint8_t*)buf_ssid, (uint8_t *)buf_passwd);

			// 마지막 한 글자만 다를수도 있어서 strncmp --> strcmp
			if(    (strcmp((char *)buf_ssid,   MESH_wifi_info.ssid   ) != STR_MATCH )
			    || (strcmp((char *)buf_passwd, MESH_wifi_info.passwd ) != STR_MATCH ))
			{
				err = nvs_set_mesh_ap_ssid_passwd((uint8_t*)MESH_wifi_info.ssid, (uint8_t *)MESH_wifi_info.passwd);	
			    if (err != ESP_OK) 
				{
					ESP_LOGE("\n", "nvs_open(%s,,,) Failed(%s)", STORAGE_NAMESPACE, esp_err_to_name(err));
		 			return ;
				}
		
				nvs_get_mesh_ap_ssid_passwd((uint8_t*)buf_ssid, (uint8_t *)buf_passwd);	
				nvs_get_ntp_server((uint8_t*)myntp_str);
			
				hexdump3("nvs_ssid(확인)",   buf_ssid,   strlen((char *)buf_ssid));
				hexdump3("nvs_passwd(확인)", buf_passwd, strlen((char *)buf_passwd));
				hexdump3("nvs_ntpserver(확인)", myntp_str, strlen((char *)myntp_str));
				reboot_flag = 1 ;
			}
			else
			{
	        	ESP_LOGW(MESH_TAG, "WiFi SSID/PASSWD is same : no need to nvs set ");
			}
	
			//----------- for ntp server ---------------------------
			memset(buf_ntp_broker, 0, sizeof(buf_ntp_broker));
			err = iotech_get_nvs_str((uint8_t*)"ntpserver", (uint8_t *)buf_ntp_broker);
	
			if( err != ESP_OK)
			{
				ESP_LOGE("\n", "nvs_open(%s,,,) Failed(%s)", STORAGE_NAMESPACE, esp_err_to_name(err));
			}
			// 마지막 한 글자만 다를수도 있어서 strncmp --> strcmp
			else if( strcmp ( (char *)buf_ntp_broker,   
			                    MESH_wifi_info.ntpserver)   != STR_MATCH )
			{
				printf(" |||||||||||||||||||||||| 1 |||||||||||||||||||||||||||\n");
				err = iotech_set_nvs_str( (uint8_t*)"ntpserver", (uint8_t *)MESH_wifi_info.ntpserver);
			    if (err != ESP_OK) 
				{
					ESP_LOGE("\n", "nvs_open(%s,,,) Failed(%s)", STORAGE_NAMESPACE, esp_err_to_name(err));
		 			return ;
				}
				printf(" |||||||||||||||||||||||| 2 |||||||||||||||||||||||||||\n");
				iotech_get_nvs_str( (uint8_t*)"ntpserver", (uint8_t *)buf_ntp_broker);
			
				hexdump3("buf_ntp_broker(확인)_ntp",   buf_ntp_broker,   strlen((char *)buf_ntp_broker));

				reboot_flag = 1 ;
			}
			else
			{
	        	ESP_LOGW(MESH_TAG, "NTP Server info      is same : no need to nvs set "); 
			}

			//----------- for mqtt_broker_uri ---------------------------
			memset(buf_ntp_broker, 0, sizeof(buf_ntp_broker));
			err = iotech_get_nvs_str((uint8_t*)"mqtt_broker_uri", (uint8_t *)buf_ntp_broker);
	
			if( err != ESP_OK) 
			{
					ESP_LOGE("\n", "nvs_open(%s,,,) Failed(%s)", STORAGE_NAMESPACE, esp_err_to_name(err));
			}
			// 마지막 한 글자만 다를수도 있어서 strncmp --> strcmp
			else if( strcmp ( (char *)buf_ntp_broker,   
			                    MESH_wifi_info.mqtt_broker_uri) != STR_MATCH )
			{
				printf(" |||||||||||||||||||||||| 3 |||||||||||||||||||||||||||\n");
				err = iotech_set_nvs_str( (uint8_t*)"mqtt_broker_uri", (uint8_t *)MESH_wifi_info.mqtt_broker_uri);
			    if (err != ESP_OK) 
				{
					ESP_LOGE("\n", "nvs_open(%s,,,) Failed(%s)", STORAGE_NAMESPACE, esp_err_to_name(err));
		 			return ;
				}
		
				printf(" |||||||||||||||||||||||| 4 |||||||||||||||||||||||||||\n");
				iotech_get_nvs_str( (uint8_t*)"mqtt_broker_uri", (uint8_t *)buf_ntp_broker);
			
				hexdump3("buf_ntp_broker(확인)_mqtt_broker_uri",   buf_ntp_broker,   strlen((char *)buf_ntp_broker));

				reboot_flag = 1 ;
			}
			else
			{
	        	ESP_LOGW(MESH_TAG, "mqtt_broker_uri info is same : no need to nvs set "); 
			}
	
			if( reboot_flag == 1 ) 
			{
				ESP_LOGE("shcho: CMD_WIFI_INFO", "Information is changed need to esp_restart() After 2 secs");
        		vTaskDelay(2000 / portTICK_PERIOD_MS);
				esp_restart();
			}
		}
	} else {
        ESP_LOGE(MESH_TAG, "Error in receiving raw mesh data: Unknown command");
//  		hexdump3("mesh_data", data, sizeof(mesh_data_t));
//  		hexdump3("mesh_data__data", data->data, data->size );
    }
}

static void check_button(void* args)
{
    static bool old_level = true;
    bool new_level;
	#if 0
//      static bool old_level_real = true;
//      bool new_level_real;
	#endif
    bool run_check_button = true;
    initialise_button();
	static unsigned int count = 0 ;
	static unsigned int count_key_pressed = 0 ;
	int DELAY_IOTECH = 5000;


    while (run_check_button) {

        if (stella_sensors_paused_for_ota) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

#if 0
//      new_level_real = gpio_get_level(EXAMPLE_BUTTON_GPIO);
//      if (!new_level_real && old_level_real) 
//  	{
//  		if( new_level_real == 0 )
//  		{
//  			flag_ota_run = 1; 
//  			ESP_LOGW(MESH_TAG, "Key Pressed !!!!!\n");
//  
//  		}
//  	}
//  	old_level_real = new_level_real;
#endif

	#if 0 // Real Button or cable
        new_level = gpio_get_level(EXAMPLE_BUTTON_GPIO);
	#else
        new_level = !old_level; //-->무조건 다르게 설정
//          vTaskDelay(30000 / portTICK_PERIOD_MS);
//          vTaskDelay(3000 / portTICK_PERIOD_MS);
		DELAY_IOTECH=5000;
        ESP_LOGW(MESH_TAG, "================================shcho!(wait %d)", DELAY_IOTECH/1000);
//          vTaskDelay(10000 / portTICK_PERIOD_MS);
	#endif

        if (!new_level && old_level)
		{
            {
                ESP_LOGW(MESH_TAG, "Key pressed!###(%d)", count_key_pressed);
                char print[6*3+1+20];
				snprintf(print, sizeof(print),"%02x%02x%02x_%02x%02x%02x", MAC2STR(my_mac_factory));
				ESP_LOGW(MESH_TAG, "my_mac_factory:%s", print);

				if( count != 0 )
				{
					int ret_s = snprintf(print, sizeof(print),"%02x%02x%02x_%02x%02x%02x__K%d_P%d",
					                                MAC2STR(my_mac_factory), count_key_pressed++, count++);
					ESP_LOGW(MESH_TAG, "ret_s=%d:%s", ret_s, print);
				}
				else
				{
					count++;
					count_key_pressed++;
					sprintf(g_hostname,"stella-%02X%02X%02X--%02X%02X%02X", MAC2STR(my_mac_factory));
					stella_set_hostname(g_hostname);
				}
            }
        }
        old_level = new_level;
//  //          vTaskDelay(50 / portTICK_PERIOD_MS);
//          vTaskDelay(10000 / portTICK_PERIOD_MS);
//          vTaskDelay(3000 / portTICK_PERIOD_MS);
        vTaskDelay(DELAY_IOTECH / portTICK_PERIOD_MS);
//          vTaskDelay(30000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);

}


void __attribute__((unused)) esp_mesh_mqtt_task(void *arg)
{
    is_running = true;
    ESP_LOGE(MESH_TAG, ">>>>>> esp_mesh_mqtt_task STARTED, is_root=%d, heap=%lu",
             (int)esp_mesh_is_root(), (unsigned long)esp_get_free_heap_size());
//      char *print;
    mesh_data_t data;
    mesh_data_t data_wifi;
    esp_err_t err;

	char buffer_send[300];
	int len_str=0;

	int count=0;
	//============================================================================
	if( esp_mesh_is_root() )
	{
		while( 1 ) 
		{
			if( strcmp((char *)mesh_ap_ssid, (char *)"iotech2-x" ) ==  STR_MATCH )
			{
				ESP_LOGE("Wait SSID to set", "\nWait for ssid is set( default is iotech2-x( sleep 2 secs )");
				ESP_LOGE("Wait SSID to set", "            ssid(current)=%s(count=%d)\n", mesh_ap_ssid, count);
				count++;
				if( count > 30 ) 
				{
					ESP_LOGW("wait timeout go on", "ssid(current)=%s\n", mesh_ap_ssid);
					break;
				}
	        	vTaskDelay(2 * 1000 / portTICK_PERIOD_MS);
			}
			else
			{
				break;
			}
		}
	}

	/* mqtt_topic_prefix: 레거시 ijoon_iotech 토픽용, 현재 미사용 */
	memset(mqtt_topic_prefix, 0, sizeof(mqtt_topic_prefix));
	sprintf(mqtt_topic_prefix, "ijoon_iotech/ip_mesh/%s", mesh_ap_ssid);
	//============================================================================

    mqtt_app_start();

	/* OTA 성공 후 재부팅 감지: RTC 플래그 확인 → MQTT 연결 대기 후 결과 발행 */
	extern int flag_mqtt_connect;
	extern char *stella_ota_result_topic;
	if (ota_success_flag == OTA_SUCCESS_MAGIC) {
		ota_success_flag = 0;  /* 플래그 즉시 초기화 (중복 발행 방지) */
		ESP_LOGW(MESH_TAG, "OTA success flag detected! Waiting for MQTT to send result...");
		int wait_ms = 0;
		while (flag_mqtt_connect == 0 && wait_ms < 90000) {
			vTaskDelay(200 / portTICK_PERIOD_MS);
			wait_ms += 200;
		}
		ESP_LOGW(MESH_TAG, "OTA result: MQTT connected=%d (waited %dms)", flag_mqtt_connect, wait_ms);
		if (flag_mqtt_connect && stella_ota_result_topic) {
			mqtt_app_publish(stella_ota_result_topic, "{\"success\":true,\"note\":\"reboot complete\"}");
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			ESP_LOGW(MESH_TAG, "OTA success result published!");
		}
	}

	char install_addr[128];

    while (is_running) {
        if (stella_sensors_paused_for_ota) {
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
	// shcho: 2024_0604: No need to publish
//          asprintf(&print, "layer:%d IP:" IPSTR, esp_mesh_get_layer(), IP2STR(&s_current_ip));
//          ESP_LOGI(MESH_TAG, "Tried to publish %s", print);
//  //          mqtt_app_publish("/topic/ip_mesh", print);
//          mqtt_app_publish("ijoon_iotech/ip_mesh", print);
//          free(print);
        if (esp_mesh_is_root()) {
			// 1. send routing table
            esp_mesh_get_routing_table((mesh_addr_t *) &s_route_table,
                                       CONFIG_MESH_ROUTE_TABLE_SIZE * 6, &s_route_table_size);
            data.size = s_route_table_size * 6 + 1;
            data.proto = MESH_PROTO_BIN;
            data.tos = MESH_TOS_P2P;
            s_mesh_tx_payload[0] = CMD_ROUTE_TABLE;
            memcpy(s_mesh_tx_payload + 1, s_route_table, s_route_table_size*6);
            data.data = s_mesh_tx_payload;

			memset(buffer_send,0, sizeof(buffer_send));
			len_str = 0 ;

			memset(install_addr, 0 , sizeof(install_addr));
			iotech_get_nvs_str((uint8_t*)"install_addr", (uint8_t*)install_addr);
	        uint8_t *my_mac = mesh_netif_get_rootnode_mac();
			len_str += sprintf( &buffer_send[len_str], "addr:<%s>:root>" MACSTR ">" , install_addr, MAC2STR(my_mac));
            for (int i = 0; i < s_route_table_size; i++) 
			{
                err = esp_mesh_send(&s_route_table[i], &data, MESH_DATA_P2P, NULL, 0);
                ESP_LOGW(MESH_TAG, " >>>>>>>>>   Sending routing table to [%d] "
                        MACSTR ": sent with err code: %d", i, MAC2STR(s_route_table[i].addr), err);

				// 1.1 Routing table 정보를 MQTT Broker에도 pub하기위한 준비
				len_str += sprintf( &buffer_send[len_str], MACSTR "," , MAC2STR(s_route_table[i].addr));
	        	ESP_LOGW(MESH_TAG, "len_str=%d:%s", len_str, buffer_send);
            }
			buffer_send[len_str-1] = 0; //마지막 쉼표 없애기
	//============================================================================
//  	    ESP_LOGW(MESH_TAG, "ijoon_iotech/ip_mesh : Tried to publish %s", buffer_send);
//  	    mqtt_app_publish("ijoon_iotech/ip_mesh", buffer_send);
	        ESP_LOGW(MESH_TAG,           "%s/ip_mesh: Tried to publish %s (disabled)", mqtt_topic_prefix, buffer_send);
	//        mqtt_app_publish(mqtt_topic_prefix, buffer_send);  /* ip_mesh 토픽 발행 비활성화 */
	//============================================================================

			//-------------------------------------------------------------------------

			// 2. SSID 정보를 NODE들에 전달 : 보낼때 같이 보내면 좋아서: 거의 MAC 통신임
            esp_mesh_get_routing_table((mesh_addr_t *) &s_route_table,
                                       CONFIG_MESH_ROUTE_TABLE_SIZE * 6, &s_route_table_size);

			stMeshWifiInfo MESH_wifi_info;
			memset((char *)&MESH_wifi_info, 0, sizeof(MESH_wifi_info));
			MESH_wifi_info.cmd = CMD_WIFI_INFO;
			strcpy(MESH_wifi_info.ssid,   (char *)mesh_ap_ssid);
			strcpy(MESH_wifi_info.passwd, (char *)mesh_ap_passwd);
			// ntp server는 7628 Router의 IP Address
//  			strcpy(MESH_wifi_info.ntpserver, (char *)mygw_str);
			nvs_get_ntp_server((uint8_t*)myntp_str);
			strcpy(MESH_wifi_info.ntpserver, (char *)myntp_str);

			iotech_get_nvs_str((uint8_t *)"mqtt_broker_uri", (uint8_t*)mqtt_broker_uri);
			strcpy(MESH_wifi_info.mqtt_broker_uri, (char *)mqtt_broker_uri);

			hexdump3("MESH_wifi_info.ntpserver(@ rootnode)", &MESH_wifi_info, sizeof(MESH_wifi_info));

            data_wifi.proto = MESH_PROTO_BIN;
            data_wifi.tos = MESH_TOS_P2P;
            data_wifi.data = (uint8_t *)&MESH_wifi_info;
            data_wifi.size = sizeof(MESH_wifi_info);
            for (int i = 0; i < s_route_table_size; i++) 
			{
                err = esp_mesh_send(&s_route_table[i], &data_wifi, MESH_DATA_P2P, NULL, 0);
                ESP_LOGW(MESH_TAG, " >>>>>>>>>   Sending Mesh WiFi info from Root-Node to [%d] "
                        MACSTR ": sent with err code: %d", i, MAC2STR(s_route_table[i].addr), err);

            }
			//---------------------------------------------------


        }
	//          vTaskDelay(2 * 1000 / portTICK_PERIOD_MS);
	        vTaskDelay(10 * 1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

/** 웨어러블 STA 전용: mqtt_app + OTA 재부팅 알림만 (메시 송수신·check_button 없음) */
static void wearable_mqtt_task(void *arg)
{
    (void)arg;
    ESP_LOGW(MESH_TAG, "wearable_mqtt_task: STA-only (no mesh)");
    memset(mqtt_topic_prefix, 0, sizeof(mqtt_topic_prefix));
    sprintf(mqtt_topic_prefix, "ijoon_iotech/ip_mesh/%s", mesh_ap_ssid);

    mqtt_app_start();

    extern int flag_mqtt_connect;
    extern char *stella_ota_result_topic;
    if (ota_success_flag == OTA_SUCCESS_MAGIC) {
        ota_success_flag = 0;
        ESP_LOGW(MESH_TAG, "OTA success flag (wearable): waiting for MQTT...");
        int wait_ms = 0;
        while (flag_mqtt_connect == 0 && wait_ms < 90000) {
            vTaskDelay(200 / portTICK_PERIOD_MS);
            wait_ms += 200;
        }
        ESP_LOGW(MESH_TAG, "OTA result: MQTT connected=%d (waited %dms)", flag_mqtt_connect, wait_ms);
        if (flag_mqtt_connect && stella_ota_result_topic) {
            mqtt_app_publish(stella_ota_result_topic, "{\"success\":true,\"note\":\"reboot complete\"}");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

static esp_err_t wearable_comm_mqtt_task_start(void)
{
    static bool started = false;
    if (started) {
        return ESP_OK;
    }
    s_route_table_lock = xSemaphoreCreateMutex();
    ESP_LOGW(MESH_TAG, "wearable mqtt: internal free=%lu, total free=%lu",
             (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned long)esp_get_free_heap_size());
    BaseType_t ret = xTaskCreateWithCaps(wearable_mqtt_task, "mqtt_wear", 8192,
                                         NULL, 5, NULL,
                                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ret != pdPASS) {
        ESP_LOGE(MESH_TAG, "wearable mqtt task create FAILED (ret=%d)", (int)ret);
        return ESP_FAIL;
    }
    xTaskCreateWithCaps(check_button, "check button task", 3072, NULL, 5, NULL,
                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    started = true;
    return ESP_OK;
}

esp_err_t esp_mesh_comm_mqtt_task_start(void)
{
    static bool is_comm_mqtt_task_started = false;


    s_route_table_lock = xSemaphoreCreateMutex();

    if (!is_comm_mqtt_task_started) {
        ESP_LOGW(MESH_TAG, "mqtt task: internal free=%lu, total free=%lu",
                 (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned long)esp_get_free_heap_size());
        /* 내부 SRAM 고갈 문제: PSRAM에 스택 할당 */
        BaseType_t ret = xTaskCreateWithCaps(esp_mesh_mqtt_task, "mqtt task", 8192,
                                             NULL, 5, NULL,
                                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (ret != pdPASS) {
            ESP_LOGE(MESH_TAG, "mqtt task xTaskCreateWithCaps FAILED (ret=%d)", (int)ret);
        } else {
            ESP_LOGW(MESH_TAG, "mqtt task created in PSRAM OK");
        }
        xTaskCreateWithCaps(check_button, "check button task", 3072, NULL, 5, NULL,
                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        is_comm_mqtt_task_started = true;
    }
    return ESP_OK;
}

/* AP/STA 비활성 타임아웃 완화, Wi‑Fi 우선 coex
 * 주의: esp_wifi_set_inactive_time(STA, 0) 및 (AP, 0)은 IDF에서 무효( STA≥3초, AP≥10초 ).
 * 0으로 호출 시 실패 → STA 기본 6초 등이 유지되어 reason:4(inactivity)·reason:2(auth) 유발 가능. */
static void __attribute__((unused)) mesh_apply_wifi_stability_settings(void)
{
    const uint16_t sta_inactive_sec = 600; /* 부모 비콘/데이터 공백 허용 (초) */
    const uint16_t ap_inactive_sec  = 600; /* 메시 자식 STA 비활성 시 deauth 지연 (초) */
    esp_err_t e = esp_wifi_set_inactive_time(WIFI_IF_STA, sta_inactive_sec);
    if (e != ESP_OK) {
        ESP_LOGW(MESH_TAG, "esp_wifi_set_inactive_time(STA,%u): %s",
                 (unsigned)sta_inactive_sec, esp_err_to_name(e));
    }
    /* 메시 내부 소프트AP는 일부 IDF/메시 조합에서 AP 비활성 시간 설정이 무효(ESP_ERR_INVALID_ARG).
     * 자식 노드 안정성은 주로 STA 쪽이 중요함. */
    e = esp_wifi_set_inactive_time(WIFI_IF_AP, ap_inactive_sec);
    if (e != ESP_OK && e != ESP_ERR_INVALID_ARG) {
        ESP_LOGW(MESH_TAG, "esp_wifi_set_inactive_time(AP,%u): %s",
                 (unsigned)ap_inactive_sec, esp_err_to_name(e));
    }
    esp_coex_preference_set(ESP_COEX_PREFER_WIFI);
}

/* flag_IS_WEARABLE는 app_main_stella()의 mux 이후에만 유효 → 그때 device_id 보정 */
void mesh_refresh_device_id_after_board_detect(void)
{
    extern int flag_IS_WEARABLE;
    char nvs_id[32] = {0};
    char old_id[DEVICE_ID_LEN];

    strncpy(old_id, device_id, sizeof(old_id));
    old_id[sizeof(old_id) - 1] = '\0';

    iotech_get_nvs_str((uint8_t*)"ID", (uint8_t*)nvs_id);
    if (nvs_id[0] != '\0') {
        strncpy(device_id, nvs_id, sizeof(device_id) - 1);
    } else {
        strncpy(device_id, flag_IS_WEARABLE ? "3W12345" : "3012345", sizeof(device_id) - 1);
    }
    device_id[sizeof(device_id) - 1] = '\0';
    ESP_LOGW(MESH_TAG, "device_id(refresh after mux)=[%s] (nvs=%s, wearable=%d)",
             device_id, nvs_id[0] ? nvs_id : "none", flag_IS_WEARABLE);
    if (strcmp(old_id, device_id) != 0 && mqtt_app_has_client()) {
        mqtt_app_reconnect();
    }
}

void __attribute__((unused)) mesh_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{

    mesh_addr_t id = {0,};
    static uint8_t last_layer = 0;
	static int count_not_found=0;

    switch (event_id) {
    case MESH_EVENT_STARTED: {
        esp_mesh_get_id(&id);
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_MESH_STARTED>ID:"MACSTR"", MAC2STR(id.addr));
        mesh_layer = esp_mesh_get_layer();
    }
    break;
    case MESH_EVENT_STOPPED: {
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_STOPPED>");
        mesh_layer = esp_mesh_get_layer();
    }
    break;
    case MESH_EVENT_CHILD_CONNECTED: {
        mesh_event_child_connected_t *child_connected = (mesh_event_child_connected_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_CHILD_CONNECTED>aid:%d, "MACSTR"",
                 child_connected->aid,
                 MAC2STR(child_connected->mac));

		count_no_parent = 0 ;

		// ===========================================================
		// connected이면 mqtt_broker_uri에 접속했었기 때문에
		char buf[10];
		memset(buf,0,sizeof(buf));
		sprintf(buf,"%d",count_no_parent);
		iotech_set_nvs_str((uint8_t*)"count_no_parent", (uint8_t*)buf);
		// ===========================================================
		printf("$$$$$$\n");
//  		printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
//  		printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
//  		printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
//  		printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
		// ===========================================================
    }
    break;
    case MESH_EVENT_CHILD_DISCONNECTED: {
        mesh_event_child_disconnected_t *child_disconnected = (mesh_event_child_disconnected_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_CHILD_DISCONNECTED>aid:%d, "MACSTR"",
                 child_disconnected->aid,
                 MAC2STR(child_disconnected->mac));
    }
    break;
    case MESH_EVENT_ROUTING_TABLE_ADD: {
        mesh_event_routing_table_change_t *routing_table = (mesh_event_routing_table_change_t *)event_data;
        ESP_LOGW(MESH_TAG, "<MESH_EVENT_ROUTING_TABLE_ADD>add %d, new:%d",
                 routing_table->rt_size_change,
                 routing_table->rt_size_new);
    }
    break;
    case MESH_EVENT_ROUTING_TABLE_REMOVE: {
        mesh_event_routing_table_change_t *routing_table = (mesh_event_routing_table_change_t *)event_data;
        ESP_LOGW(MESH_TAG, "<MESH_EVENT_ROUTING_TABLE_REMOVE>remove %d, new:%d",
                 routing_table->rt_size_change,
                 routing_table->rt_size_new);
    }
    break;
    case MESH_EVENT_NO_PARENT_FOUND: {
        mesh_event_no_parent_found_t *no_parent = (mesh_event_no_parent_found_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_NO_PARENT_FOUND>scan times:%d(횟수)",
                 no_parent->scan_times);
		// shcho add
//  		멍하는경우가 있어서
//  		if( count_not_found == 1 ) //바로reboot하는 것으로 변경함 
		{

		// ===========================================================
			char buf[10];
			memset(buf,0,sizeof(buf));

			count_no_parent++;
			sprintf(buf,"%d",count_no_parent);

			iotech_set_nvs_str((uint8_t*)"count_no_parent", (uint8_t*)buf);
//  			ESP_LOGW("count_no_parent", "");
//  			ESP_LOGW("count_no_parent", "");
//  			ESP_LOGW("count_no_parent", "");
//  //  		ESP_LOGW("count_no_parent", "count_no_parent를 증가시키고,  booting할 때 Check : 2 이상이면(3번째 Reboot시)  ssid를 iotech2-x로 변경함");
			ESP_LOGW("count_no_parent", "count_no_parent를 증가시키고,  booting할 때 Check : 5 이상이면(3번째 Reboot시)  ssid를 iotech2-x로 변경함");
//  			ESP_LOGW("count_no_parent", "");
//  			ESP_LOGW("count_no_parent", "");
//  			ESP_LOGW("count_no_parent", "");

			iotech_get_nvs_str((uint8_t*)"count_no_parent", (uint8_t*)buf); 
			hexdump3("count_no_parent(after set)", buf, sizeof(buf));
		// ===========================================================

        	ESP_LOGE(MESH_TAG, "shcho force restart() after 2sec");
			vTaskDelay(2000 / portTICK_PERIOD_MS);
			esp_restart();

		}
		count_not_found++;
			
    }
    /* TODO handler for the failure */
    break;
    case MESH_EVENT_PARENT_CONNECTED: {
		// ===========================================================
		count_no_parent = 0 ;
		char buf[10];
		memset(buf,0,sizeof(buf));
		sprintf(buf,"%d",count_no_parent);
		iotech_set_nvs_str((uint8_t*)"count_no_parent", (uint8_t*)buf);
		// ===========================================================

        mesh_event_connected_t *connected = (mesh_event_connected_t *)event_data;
        esp_mesh_get_id(&id);
        mesh_layer = connected->self_layer;
        memcpy(&mesh_parent_addr.addr, connected->connected.bssid, 6);
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_PARENT_CONNECTED>layer:%d-->%d, parent:"MACSTR"%s, ID:"MACSTR"",
                 last_layer, mesh_layer, MAC2STR(mesh_parent_addr.addr),
                 esp_mesh_is_root() ? "<ROOT>" :
                 (mesh_layer == 2) ? "<layer2>" : "", MAC2STR(id.addr));
        last_layer = mesh_layer;

//  		iotech_set_hostname(g_hostname);
		stella_set_hostname(g_hostname);

        mesh_netifs_start(esp_mesh_is_root());
        mesh_apply_wifi_stability_settings();
        /* app_main_stella()에서 세마포 등 준비된 뒤에만 실제 부하 시작 */
        g_mesh_parent_connected_flag = true;
        stella_maybe_start_heavy_if_ready();

    }
    break;
    case MESH_EVENT_PARENT_DISCONNECTED: {
        mesh_event_disconnected_t *disconnected = (mesh_event_disconnected_t *)event_data;
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_PARENT_DISCONNECTED>reason:%d",
                 disconnected->reason);
        mesh_layer = esp_mesh_get_layer();
        mesh_netifs_stop();
        mesh_apply_wifi_stability_settings();
    }
    break;
    case MESH_EVENT_LAYER_CHANGE: {
        mesh_event_layer_change_t *layer_change = (mesh_event_layer_change_t *)event_data;
        mesh_layer = layer_change->new_layer;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_LAYER_CHANGE>layer:%d-->%d%s",
                 last_layer, mesh_layer,
                 esp_mesh_is_root() ? "<ROOT>" :
                 (mesh_layer == 2) ? "<layer2>" : "");
        last_layer = mesh_layer;
    }
    break;
    case MESH_EVENT_ROOT_ADDRESS: {
        mesh_event_root_address_t *root_addr = (mesh_event_root_address_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROOT_ADDRESS>root address:"MACSTR"",
                 MAC2STR(root_addr->addr));

		ESP_LOGE("ddd", "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
//  		iotech_set_hostname(g_hostname);
		stella_set_hostname(g_hostname);
    }
    break;
    case MESH_EVENT_VOTE_STARTED: {
        mesh_event_vote_started_t *vote_started = (mesh_event_vote_started_t *)event_data;
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_VOTE_STARTED>attempts:%d, reason:%d, rc_addr:"MACSTR"",
                 vote_started->attempts,
                 vote_started->reason,
                 MAC2STR(vote_started->rc_addr.addr));
    }
    break;
    case MESH_EVENT_VOTE_STOPPED: {
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_VOTE_STOPPED>");
        break;
    }
    case MESH_EVENT_ROOT_SWITCH_REQ: {
        mesh_event_root_switch_req_t *switch_req = (mesh_event_root_switch_req_t *)event_data;
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_ROOT_SWITCH_REQ>reason:%d, rc_addr:"MACSTR"",
                 switch_req->reason,
                 MAC2STR( switch_req->rc_addr.addr));
    }
    break;
    case MESH_EVENT_ROOT_SWITCH_ACK: {
        /* new root */
        mesh_layer = esp_mesh_get_layer();
        esp_mesh_get_parent_bssid(&mesh_parent_addr);
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROOT_SWITCH_ACK>layer:%d, parent:"MACSTR"", mesh_layer, MAC2STR(mesh_parent_addr.addr));
    }
    break;
    case MESH_EVENT_TODS_STATE: {
        mesh_event_toDS_state_t *toDs_state = (mesh_event_toDS_state_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_TODS_REACHABLE>state:%d", *toDs_state);
    }
    break;
    case MESH_EVENT_ROOT_FIXED: {
        mesh_event_root_fixed_t *root_fixed = (mesh_event_root_fixed_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROOT_FIXED>%s",
                 root_fixed->is_fixed ? "fixed" : "not fixed");
    }
    break;
    case MESH_EVENT_ROOT_ASKED_YIELD: {
        mesh_event_root_conflict_t *root_conflict = (mesh_event_root_conflict_t *)event_data;
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_ROOT_ASKED_YIELD>"MACSTR", rssi:%d, capacity:%d",
                 MAC2STR(root_conflict->addr),
                 root_conflict->rssi,
                 root_conflict->capacity);
    }
    break;
    case MESH_EVENT_CHANNEL_SWITCH: {
        mesh_event_channel_switch_t *channel_switch = (mesh_event_channel_switch_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_CHANNEL_SWITCH>new channel:%d", channel_switch->channel);
    }
    break;
    case MESH_EVENT_SCAN_DONE: {
        mesh_event_scan_done_t *scan_done = (mesh_event_scan_done_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_SCAN_DONE>number:%d",
                 scan_done->number);
    }
    break;
    case MESH_EVENT_NETWORK_STATE: {
        mesh_event_network_state_t *network_state = (mesh_event_network_state_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_NETWORK_STATE>is_rootless:%d",
                 network_state->is_rootless);
    }
    break;
    case MESH_EVENT_STOP_RECONNECTION: {
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_STOP_RECONNECTION>");
    }
    break;
    case MESH_EVENT_FIND_NETWORK: {
        mesh_event_find_network_t *find_network = (mesh_event_find_network_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_FIND_NETWORK>new channel:%d, router BSSID:"MACSTR"",
                 find_network->channel, MAC2STR(find_network->router_bssid));
    }
    break;
    case MESH_EVENT_ROUTER_SWITCH: {
        mesh_event_router_switch_t *router_switch = (mesh_event_router_switch_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROUTER_SWITCH>new router:%s, channel:%d, "MACSTR"",
                 router_switch->ssid, router_switch->channel, MAC2STR(router_switch->bssid));
    }
    break;
    default:
        ESP_LOGI(MESH_TAG, "unknown id:%" PRId32 "", event_id);
        break;
    }
}

/** WiFi 재연결 — BLE 공존을 위해 지수 백오프 적용 */
#define WIFI_BACKOFF_INIT_MS    2000
#define WIFI_BACKOFF_MAX_MS     30000
#define WIFI_REPROV_FAIL_COUNT  10
static int  s_wifi_backoff_ms = WIFI_BACKOFF_INIT_MS;
static int  s_wifi_fail_count = 0;
static esp_timer_handle_t s_wifi_reconnect_timer = NULL;

static void wifi_reconnect_timer_cb(void *arg)
{
    (void)arg;
    if (s_current_ip.addr != 0) return;
    ESP_LOGW(MESH_TAG, "WiFi reconnect attempt (backoff was %dms)", s_wifi_backoff_ms);
    esp_wifi_connect();
}

static void wifi_reconnect_timer_init(void)
{
    if (s_wifi_reconnect_timer) return;
    const esp_timer_create_args_t args = {
        .callback = wifi_reconnect_timer_cb,
        .name = "wifi_recon",
    };
    esp_timer_create(&args, &s_wifi_reconnect_timer);
}

/** plain-STA WiFi 연결 해제 시 자동 재연결 — BLE 공존을 위해 지수 백오프 적용 */
static void wifi_event_sta_disconnected_handler(void *arg, esp_event_base_t event_base,
                                                int32_t event_id, void *event_data)
{
    wifi_event_sta_disconnected_t *disc = (wifi_event_sta_disconnected_t *)event_data;
    ESP_LOGW(MESH_TAG, "<WIFI_EVENT_STA_DISCONNECTED> reason=%d, retry in %dms (fail %d/%d)",
             disc->reason, s_wifi_backoff_ms, s_wifi_fail_count + 1, WIFI_REPROV_FAIL_COUNT);

    s_current_ip.addr = 0;

    if (strlen((char *)mesh_ap_ssid) == 0) {
        ESP_LOGW(MESH_TAG, "STA disconnect: no SSID configured, skip reconnect");
        return;
    }

    s_wifi_fail_count++;

    if (flag_IS_WEARABLE == 1 && s_wifi_fail_count >= WIFI_REPROV_FAIL_COUNT) {
        ESP_LOGW(MESH_TAG, "WiFi failed %d times — clearing credentials for re-provisioning",
                 s_wifi_fail_count);
        memset(mesh_ap_ssid, 0, sizeof(mesh_ap_ssid));
        memset(mesh_ap_passwd, 0, sizeof(mesh_ap_passwd));
        nvs_set_mesh_ap_ssid_passwd(mesh_ap_ssid, mesh_ap_passwd);
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
        return;
    }

    if (s_wifi_reconnect_timer) {
        esp_timer_stop(s_wifi_reconnect_timer);
        esp_timer_start_once(s_wifi_reconnect_timer, (uint64_t)s_wifi_backoff_ms * 1000);
    }

    if (s_wifi_backoff_ms < WIFI_BACKOFF_MAX_MS) {
        s_wifi_backoff_ms *= 2;
        if (s_wifi_backoff_ms > WIFI_BACKOFF_MAX_MS)
            s_wifi_backoff_ms = WIFI_BACKOFF_MAX_MS;
    }
}

/* ── WiFi Provisioning (웨어러블 전용) ────────────────────────── */
bool s_prov_in_progress = false;
static EventGroupHandle_t s_prov_event_group = NULL;
#define PROV_WIFI_CONNECTED BIT0
#define PROV_ENDED          BIT1

#define PROV_QR_VERSION     "v1"
#define PROV_POP            "abcd1234"
#define PROV_QRCODE_URL     "https://espressif.github.io/esp-jumpstart/qrcode.html"

static void prov_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    if (base == WIFI_PROV_EVENT) {
        switch (id) {
        case WIFI_PROV_START:
            ESP_LOGW(MESH_TAG, "[PROV] Provisioning started");
            break;
        case WIFI_PROV_CRED_RECV: {
            wifi_sta_config_t *cfg = (wifi_sta_config_t *)data;
            ESP_LOGW(MESH_TAG, "[PROV] Received: SSID=%s", (char *)cfg->ssid);
            memset(mesh_ap_ssid, 0, sizeof(mesh_ap_ssid));
            memset(mesh_ap_passwd, 0, sizeof(mesh_ap_passwd));
            strncpy((char *)mesh_ap_ssid, (char *)cfg->ssid, sizeof(mesh_ap_ssid) - 1);
            strncpy((char *)mesh_ap_passwd, (char *)cfg->password, sizeof(mesh_ap_passwd) - 1);
            nvs_set_mesh_ap_ssid_passwd(mesh_ap_ssid, mesh_ap_passwd);
            ESP_LOGW(MESH_TAG, "[PROV] Credentials saved to NVS");
            break;
        }
        case WIFI_PROV_CRED_FAIL: {
            wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)data;
            ESP_LOGE(MESH_TAG, "[PROV] Failed: %s",
                     (*reason == WIFI_PROV_STA_AUTH_ERROR) ? "Auth error" : "AP not found");
            break;
        }
        case WIFI_PROV_CRED_SUCCESS:
            ESP_LOGW(MESH_TAG, "[PROV] Provisioning successful");
            break;
        case WIFI_PROV_END:
            ESP_LOGW(MESH_TAG, "[PROV] Provisioning ended, deinit manager");
            wifi_prov_mgr_deinit();
            s_prov_in_progress = false;
            if (s_prov_event_group)
                xEventGroupSetBits(s_prov_event_group, PROV_ENDED);
            break;
        default: break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGW(MESH_TAG, "[PROV] Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        if (s_prov_event_group)
            xEventGroupSetBits(s_prov_event_group, PROV_WIFI_CONNECTED);
    }
}

static void prov_print_qr(const char *name)
{
    char payload[150] = {0};
    snprintf(payload, sizeof(payload),
             "{\"ver\":\"%s\",\"name\":\"%s\",\"pop\":\"%s\",\"transport\":\"ble\"}",
             PROV_QR_VERSION, name, PROV_POP);
    ESP_LOGI(MESH_TAG, "Scan this QR code from the ESP BLE Prov app:");
    esp_qrcode_config_t qr_cfg = ESP_QRCODE_CONFIG_DEFAULT();
    esp_qrcode_generate(&qr_cfg, payload);
    ESP_LOGI(MESH_TAG, "Or open: %s?data=%s", PROV_QRCODE_URL, payload);
}

static void stella_wearable_start_provisioning(void)
{
    s_prov_event_group = xEventGroupCreate();
    s_prov_in_progress = true;

    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
        PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
        PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &prov_event_handler, NULL));

    wifi_prov_mgr_config_t prov_cfg = {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE,
    };
    ESP_ERROR_CHECK(wifi_prov_mgr_init(prov_cfg));

    char service_name[33];
    snprintf(service_name, sizeof(service_name), "stella-PROV_%02X%02X%02X",
             my_mac_factory[3], my_mac_factory[4], my_mac_factory[5]);

    uint8_t custom_service_uuid[] = {
        0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
        0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02,
    };
    wifi_prov_scheme_ble_set_service_uuid(custom_service_uuid);

    wifi_prov_security_t security = WIFI_PROV_SECURITY_1;
    const char *pop = PROV_POP;

    ESP_LOGW(MESH_TAG, "[PROV] Starting BLE provisioning as '%s'", service_name);
    ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(
        security, (const void *)pop, service_name, NULL));

    prov_print_qr(service_name);

    ESP_LOGW(MESH_TAG, "[PROV] Waiting for provisioning + WiFi connection ...");
    xEventGroupWaitBits(s_prov_event_group,
                        PROV_WIFI_CONNECTED | PROV_ENDED,
                        pdTRUE, pdTRUE, portMAX_DELAY);

    esp_event_handler_unregister(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler);
    esp_event_handler_unregister(PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler);
    esp_event_handler_unregister(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &prov_event_handler);

    vEventGroupDelete(s_prov_event_group);
    s_prov_event_group = NULL;

    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    s_current_ip.addr = 1;

    ESP_LOGW(MESH_TAG, "[PROV] Done! WiFi connected. Continuing to NimBLE + normal operation.");
}
/* ── WiFi Provisioning 끝 ──────────────────────────────────── */

void ip_event_handler(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    ESP_LOGI(MESH_TAG, "<IP_EVENT_STA_GOT_IP>IP(shcho):" IPSTR, IP2STR(&event->ip_info.ip));

    s_wifi_backoff_ms = WIFI_BACKOFF_INIT_MS;
    s_wifi_fail_count = 0;

    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

	//=============================================================================
	char *hostname;
	esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
//  //  	esp_err_t err = esp_netif_get_hostname(sta_netif, &tx_buf_tmp[26+strlen(argv[2])]);
//  	esp_err_t err = esp_netif_get_hostname(sta_netif, (const char **)&hostname);
	esp_netif_get_hostname(sta_netif, (const char **)&hostname);
//  	memcpy(&tx_buf_tmp[128], hostname, strlen(hostname) );
//  	ESP_LOGW("hostname", "hostname=%s", hostname);
	hexdump3("hostname(@ip_event_handler)", hostname, strlen(hostname));
	//=============================================================================

	//===========DHCP Server정보에 반영되도록 미리=======================================================================
	//여기서는 반영이 되지 않나????
	sprintf(g_hostname,"stella-%02X%02X%02X--%02X%02X%02X", MAC2STR(my_mac_factory));
	hexdump3("set hostname(@ip_event_handler)", g_hostname, sizeof(g_hostname));

//  //  	esp_netif_t *sta_netif_1 = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
//  //  	esp_netif_set_hostname(sta_netif_1, g_hostname);

//  	iotech_set_hostname(g_hostname);
	stella_set_hostname(g_hostname);
	//==================================================================================

	sprintf((char *)myip_str, IPSTR, IP2STR(&event->ip_info.ip)) ;
	sprintf((char *)mygw_str, IPSTR, IP2STR(&event->ip_info.gw)) ;
//  	strncpy( (char *)myip_str, IP2STR(&event->ip_info.ip), sizeof( myip_str ) ) ;

    s_current_ip.addr = event->ip_info.ip.addr;
#if !CONFIG_MESH_USE_GLOBAL_DNS_IP
    if (!g_stella_wearable_plain_sta) {
        esp_netif_t *netif = event->esp_netif;
        esp_netif_dns_info_t dns;
        ESP_ERROR_CHECK(esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns));
        mesh_netif_start_root_ap(esp_mesh_is_root(), dns.ip.u_addr.ip4.addr);
    }
#endif

    if (g_stella_wearable_plain_sta) {
        g_mesh_parent_connected_flag = true;
        stella_maybe_start_heavy_if_ready();
    }

	app_main_tcp_server();

    if (g_stella_wearable_plain_sta) {
        wearable_comm_mqtt_task_start();
    } else {
        esp_mesh_comm_mqtt_task_start();
    }
}

//  //-----------------------shcho :start -----------------------------
//  
//  static const char *TAG_eth = "eth_example";
//  
//  /** Event handler for Ethernet events */
//  static void eth_event_handler(void *arg, esp_event_base_t event_base,
//                                int32_t event_id, void *event_data)
//  {
//      uint8_t mac_addr[6] = {0};
//      /* we can get the ethernet driver handle from event data */
//      esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;
//  
//      switch (event_id) {
//      case ETHERNET_EVENT_CONNECTED:
//          esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
//          ESP_LOGI(TAG_eth, "Ethernet Link Up");
//          ESP_LOGI(TAG_eth, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
//                   mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
//          break;
//      case ETHERNET_EVENT_DISCONNECTED:
//          ESP_LOGI(TAG_eth, "Ethernet Link Down");
//          break;
//      case ETHERNET_EVENT_START:
//          ESP_LOGI(TAG_eth, "Ethernet Started");
//          break;
//      case ETHERNET_EVENT_STOP:
//          ESP_LOGI(TAG_eth, "Ethernet Stopped");
//          break;
//      default:
//          break;
//      }
//  }
//  
//  /** Event handler for IP_EVENT_ETH_GOT_IP */
//  static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
//                                   int32_t event_id, void *event_data)
//  {
//      ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
//      const esp_netif_ip_info_t *ip_info = &event->ip_info;
//  
//      ESP_LOGI(TAG_eth, "Ethernet Got IP Address");
//      ESP_LOGI(TAG_eth, "~~~~~~~~~~~");
//      ESP_LOGI(TAG_eth, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
//      ESP_LOGI(TAG_eth, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
//      ESP_LOGI(TAG_eth, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
//      ESP_LOGI(TAG_eth, "~~~~~~~~~~~");
//  }
//  
//  void app_main_ethernet(void)
//  {
//      // Initialize Ethernet driver
//      uint8_t eth_port_cnt = 0;
//      esp_eth_handle_t *eth_handles;
//      ESP_ERROR_CHECK(example_eth_init(&eth_handles, &eth_port_cnt));
//  
//      // Initialize TCP/IP network interface aka the esp-netif (should be called only once in application)
//      ESP_ERROR_CHECK(esp_netif_init());
//      // Create default event loop that running in background
//  //      ESP_ERROR_CHECK(esp_event_loop_create_default());
//  
//      // Create instance(s) of esp-netif for Ethernet(s)
//      if (eth_port_cnt == 1) {
//          // Use ESP_NETIF_DEFAULT_ETH when just one Ethernet interface is used and you don't need to modify
//          // default esp-netif configuration parameters.
//          esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
//          esp_netif_t *eth_netif = esp_netif_new(&cfg);
//          // Attach Ethernet driver to TCP/IP stack
//  		ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[0])));
//      } else {
//          // Use ESP_NETIF_INHERENT_DEFAULT_ETH when multiple Ethernet interfaces are used and so you need to modify
//          // esp-netif configuration parameters for each interface (name, priority, etc.).
//          esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
//          esp_netif_config_t cfg_spi = {
//              .base = &esp_netif_config,
//              .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
//          };
//          char if_key_str[10];
//          char if_desc_str[10];
//          char num_str[3];
//          for (int i = 0; i < eth_port_cnt; i++) {
//              itoa(i, num_str, 10);
//              strcat(strcpy(if_key_str, "ETH_"), num_str);
//              strcat(strcpy(if_desc_str, "eth"), num_str);
//              esp_netif_config.if_key = if_key_str;
//              esp_netif_config.if_desc = if_desc_str;
//              esp_netif_config.route_prio -= i*5;
//              esp_netif_t *eth_netif = esp_netif_new(&cfg_spi);
//  
//              // Attach Ethernet driver to TCP/IP stack
//              ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[i])));
//          }
//      }
//  
//      // Register user defined event handers
//      ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
//      ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));
//  
//      // Start Ethernet driver state machine
//      for (int i = 0; i < eth_port_cnt; i++) {
//          ESP_ERROR_CHECK(esp_eth_start(eth_handles[i]));
//      }
//  }
//  //-----------------------shcho : end -----------------------------




// The code says that calling uxTaskGetSystemState directly
// rather than VTaskList() is preferred
static int do_tasks_info(int argc, char **argv) {

    const size_t bytes_per_task = 45; /* see vTaskList description */
    // config file has max name length = 24
    //    see component config -> FreeRTOS ->
    // status is a single char. one byte
    // current priority is at most a two digit number. two bytes.
    // StackHighWaterMark is at most four digit numbers. four bytes
    // task number is two digits. two bytes
    // affinity is sign indicator and one digit. two bytes
    // five tabs. five bytes.
    // carriage return line feed. two bytes
    // 24+1+2+4+2+2+5+2 = 42. Set to 45 just to give some spare

    printf("heap size before malloc %ld\n", esp_get_free_heap_size());
    char *task_list_buffer = malloc(uxTaskGetNumberOfTasks() * bytes_per_task);
    if (task_list_buffer == NULL) {
        ESP_LOGE("TASK_INFO", "failed to allocate buffer for vTaskList output");
        return 1;
    }
    printf("heap size after malloc %ld\n", esp_get_free_heap_size());
    fputs("Task Name\t\tStatus\tPrio\tHWM\tTask#", stdout);
#ifdef CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID
    fputs("\tAffinity", stdout);
#endif
    fputs("\n", stdout);
    vTaskList(task_list_buffer);
    fputs(task_list_buffer, stdout);
    free(task_list_buffer);
    printf("heap size after free %ld\n", esp_get_free_heap_size());
    return 0;
}

static int  register_view_tasks()
{
//      esp_console_config_t console_config = ESP_CONSOLE_CONFIG_DEFAULT();
//      ESP_ERROR_CHECK(esp_console_init(&console_config));

    const esp_console_cmd_t cmd = {
        .command = "task",
        .help = "View Task INFO ",
        .hint = NULL,
        .func = do_tasks_info,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
//      // re-register the same command, just for test
//      ESP_OK(esp_console_cmd_register(&cmd));
//      ESP_OK(esp_console_deinit());
    return 0;
}


void  register_test_spi()
{
   const esp_console_cmd_t cmd = {
      .command = "test_spi",
      .help = "Test SPI : INIT ",
      .hint = NULL, 
      .func = ade9153a_spi_init, 
   };
   ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));    
}


static  int  test (int  argc,  char ** argv) {
     //   인자의 개수를 확인 
     if (argc != 4) {
        ESP_LOGE(MESH_TAG, "Error: Expected 3 arguments, but received %d", argc-1);
        return 1;  // 오류 코드 반환
     }   

    // 각 인자를 사용한 추가 로직 (예시)
    ESP_LOGI(MESH_TAG, "Argument 1: %s",  argv[1]);      // "a"            
    ESP_LOGI(MESH_TAG, "Argument 2: %s",  argv[2]);      // "b" 
    ESP_LOGI(MESH_TAG, "Argument 3: %s",  argv[3]);      // "c"

    // 각 인자를 사용한 추가 로직 (예시)
    ESP_LOGI(MESH_TAG, "Processing: %s %s %s", argv[1], argv[2], argv[3]);
    return 0;  // 정상 종료
}





 void  register_test() {
      const esp_console_cmd_t cmd= {
        .command= "test",
        .help= "Usage: test <a> <b> <c>",
        .hint=  NULL, 
        .func=  test,  // test 함수를 등록
      };

      ESP_ERROR_CHECK(esp_console_cmd_register(&cmd)); 
      ESP_LOGI(MESH_TAG, "Command 'test' registered");
 }



static const char *RELAY_TAG = "relay";


static int do_relay_on(int argc, char **argv)
{
    gpio_relay_on();
	ESP_LOGI(RELAY_TAG,  "relay on");
    return 0;
}



static int do_relay_off(int argc,  char **argv)
{
     gpio_relay_off();
     ESP_LOGI(RELAY_TAG,  "relay off");
     return 0;
}



static  int do_relay_nop(int argc,  char **argv)
{
    gpio_relay_nop();
    ESP_LOGI(RELAY_TAG,  "relay nop");
    return 0;
}




void register_relayOn(void)
{
    const esp_console_cmd_t  cmd = {
        .command = "relayOn",
		.help  =  "Relay On :",
        .hint  =  NULL, 
        .func  = do_relay_on,
	};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}


void  register_relayOff(void)
{
     const esp_console_cmd_t  cmd = {
        .command =  "relayOff",
		.help =  "Relay Off : ",
        .hint =  NULL,     
        .func =  do_relay_off,  
	 };
     ESP_ERROR_CHECK(esp_console_cmd_register(&cmd)); 
}


void  register_relayNop(void)
{
      const esp_console_cmd_t cmd = {
        .command =  "relayNop",
        .help    =  "Relay Nop : ", 
        .hint    =  NULL,
        .func    =  do_relay_nop, 
	  };      
      ESP_ERROR_CHECK(esp_console_cmd_register(&cmd)); 
}




void  register_date(void)
{
    const esp_console_cmd_t cmd = {
        .command = "date",
        .help =  "Show Date : ",
        .hint =  NULL,
        .func =  read_rtc_time,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}


void  register_meter(void)
{
    const esp_console_cmd_t cmd = {
       .command = "meter",
       .help =  "show meter: ", 
       .hint = NULL, 
       .func =  powerRead, 
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));

}



void  register_temperature(void)
{
    const esp_console_cmd_t cmd = {
       .command = "temp",
       .help =  "show temperature: ", 
       .hint = NULL, 
       .func =  temperatureRead, 
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}



void  register_autocalibration(void)
{
      const  esp_console_cmd_t cmd = {
         .command = "auto", 
         .help = "exec autocalibration: ",  
         .hint = NULL,
         .func =  auto_calibration,
      };
      ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}


void  register_eventflag(void)
{
    const  esp_console_cmd_t  cmd =  {
         .command =  "eventflag",
         .help    =  "event flag: ",
         .hint    =   NULL, 
         .func    =   eventFlag,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}



void  register_readDB(void)
{
    const esp_console_cmd_t  cmd =   {
        .command = "readDB",  
        .help    = "read db: ", 
        .hint    =  NULL,  
        .func    = select_meter_1,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));    
}


int RespOK()
{
   ESP_LOGW(MESH_TAG, "Resp OK");
   return 0;
}




void  register_respOK(void)
{
     const esp_console_cmd_t cmd={
		.command  =  "ready?",	
        .help     =  "ready: ", 
        .hint     =   NULL,  
        .func     =   RespOK, 
	 };
     ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));    
}





void  register_readConstant(void)
{
   const esp_console_cmd_t  cmd =  {
         .command = "constant",
         .help    =  "read constant: ", 
         .hint    =  NULL,
         .func    =  ReadConstant, 
   };
   ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}





static int do_test_ota_stop(int argc, char **argv)
{
    printf("OTA_RUN_STOP \n");
	flag_ota_run = 0;
	stella_sensors_paused_for_ota = 0; /* OTA 루프 밖에서도 센서가 멈춘 것처럼 보일 때 복구 */
    return 0;
}


static  int  do_set_constant(int argc,  char **argv)
{
   
   ESP_LOGI(MESH_TAG, "argc : %d",  argc);
   ESP_LOGI(MESH_TAG, "cal_irms_cc  : %s",   argv[1]);
   ESP_LOGI(MESH_TAG, "cal_vrns_cc  : %s",   argv[2]);
   ESP_LOGI(MESH_TAG, "cal_energy_cc: %s",   argv[3]);
   ESP_LOGI(MESH_TAG, "cal_power_cc : %s",   argv[4]);
   return 0;      
}

void register_setConstant(void)
{
   
     const esp_console_cmd_t  cmd = {
           .command = "setConstant",
		   .help    = "set constant: ",
		   .hint    = NULL,
           .func    = do_set_constant, 
	 };
     ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}




static  int do_set_meter(int argc,  char **argv)
{
    ESP_LOGI(MESH_TAG, "argc : %d", argc);

	if(argc>6)
    {
		ESP_LOGI(MESH_TAG, "rated_voltage: %s",   argv[1]);
		ESP_LOGI(MESH_TAG, "rated_current: %s",   argv[2]);
       	ESP_LOGI(MESH_TAG, "rated_freq: %s",      argv[3]);
  		ESP_LOGI(MESH_TAG, "swell: %s",           argv[4]);
        ESP_LOGI(MESH_TAG, "dip: %s",             argv[5]);
	   	ESP_LOGI(MESH_TAG, "over_current: %s",    argv[6]);
    }
    return 0; 
}



void  register_setMeter(void)
{
     const esp_console_cmd_t  cmd = {
          .command = "setMeter",
		  .help    = "set meter: ",
		  .hint    = NULL, 
		  .func    = do_set_meter,
	 };
     ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}


void register_testDMA(void)
{
     const esp_console_cmd_t cmd = {
        .command = "testDma",
        .help    = "test spi DMA:", 
		.hint    =  NULL,     
        .func    =  read_samples_with_dma,
	 };
	  ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}


static  int do_view_ade9153a_av_wav(int argc,  char **argv)
{

    read_AV_WAVE();
//	perform_fft_and_thd();
    return 0;
}


void  register_view_av_wav_show(void)
{
     const esp_console_cmd_t cmd = {
         .command = "wave",
		 .help  =   "ADE9153A_AV_WAV1(show):dma?? ",     
         .hint  =  NULL, 
		 .func  =  do_view_ade9153a_av_wav, 
	 };
     ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));  
}


void register_ota_stop(void)
{
//      esp_console_config_t console_config = ESP_CONSOLE_CONFIG_DEFAULT();
//      ESP_ERROR_CHECK(esp_console_init(&console_config));

    const esp_console_cmd_t cmd = {
        .command = "ota_stop",
        .help = "OTA_TEST STOP : ota_run=0",
        .hint = NULL,
        .func = do_test_ota_stop,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
//      // re-register the same command, just for test
//      ESP_OK(esp_console_cmd_register(&cmd));
//      ESP_OK(esp_console_deinit());
}

static int do_test_ota_run(int argc, char **argv)
{
    printf("Hello World : OTA_TEST\n");
    ESP_LOGW("       TEST_OTA_RUN", "Test: set ota_url(argv[1])=%s", argv[1]);
	memset(ota_url, 0, sizeof(ota_url));
	strncpy((char *)ota_url, &argv[1][8], sizeof(ota_url)-1);
	flag_ota_run = 1;
	stella_sensors_paused_for_ota = 1;
    ESP_LOGW("       TEST_OTA_RUN", "Test: set ota_url(ota_url)=%s", ota_url);
    return 0;
}

void register_ota_run_test(void)
{
//      esp_console_config_t console_config = ESP_CONSOLE_CONFIG_DEFAULT();
//      ESP_ERROR_CHECK(esp_console_init(&console_config));

    const esp_console_cmd_t cmd = {
        .command = "ota_run",
        .help = "OTA_TEST: ota_run=1, and url\n""to mqtt_broker\n""ota_url=https://iotech.iptime.org:38070/ota_image_shcho/ip_internal_network.bin",
        .hint = NULL,
        .func = do_test_ota_run,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
//      // re-register the same command, just for test
//      ESP_OK(esp_console_cmd_register(&cmd));
//      ESP_OK(esp_console_deinit());
}



//-------------------------------------------------
static int do_view_wifi_mesh_ap(int argc, char **argv)
{
	uint8_t nvs_ssid[32];         // 30 => 32
	uint8_t nvs_passwd[64];       // 40 => 64
	uint8_t mqtt_broker_uri[128];
	uint8_t ota_url[256];
	uint8_t install_addr[256];
//  	esp_err_t err ;

	memset( nvs_ssid, 0, sizeof(nvs_ssid));
	memset( nvs_passwd, 0, sizeof(nvs_passwd));

	nvs_get_mesh_ap_ssid_passwd((uint8_t*)nvs_ssid, (uint8_t *)nvs_passwd);	
	iotech_get_nvs_str((uint8_t*)"mqtt_broker_uri", mqtt_broker_uri);
	iotech_get_nvs_str((uint8_t*)"ota_url", ota_url);
	iotech_get_nvs_str((uint8_t*)"install_addr", install_addr);

	hexdump3("nvs_ssid",   nvs_ssid,   strlen((char *)nvs_ssid));
	hexdump3("nvs_passwd", nvs_passwd, strlen((char *)nvs_passwd));

	printf("----------------------------------------------------------------------------------\n");
	hexdump3("mesh_ap_ssid_current_connected",   mesh_ap_ssid_current_connected,   strlen((char *)mesh_ap_ssid_current_connected));
	hexdump3("mesh_ap_ssid(current)",   mesh_ap_ssid,   strlen((char *)mesh_ap_ssid));
	hexdump3("mesh_ap_passwd(current)", mesh_ap_passwd, strlen((char *)mesh_ap_passwd));
	hexdump3("myip_str(current)",       myip_str, strlen((char *)myip_str)+1 );
	hexdump3("mygw_str(current)",       mygw_str, strlen((char *)mygw_str)+1 );
	hexdump3("mqtt_broker_uri(current)",       mqtt_broker_uri, strlen((char *)mqtt_broker_uri)+1 );
	hexdump3("ota_url(current)",       ota_url, strlen((char *)ota_url)+1 );
	hexdump3("install_addr",       install_addr, strlen((char *)install_addr)+1 );
	printf(  "esp_read_mac(ESP_MAC_EFUSE_FACTORY) :     " MACSTR "\n\n", MAC2STR(my_mac_factory) );
	printf(  "======================================================================\n");
    printf(  "  Node Type: %s\n\n", esp_mesh_is_root()? "Root Node": "Just Node");
	printf(  "======================================================================\n");

	#ifdef ESP_APP_DESC_MAGIC_WORD // enable functionality only if present in IDF by testing for macro.

	esp_partition_iterator_t it = esp_partition_find( ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);

	esp_app_desc_t app_info;

	const esp_partition_t *boot_part = esp_ota_get_boot_partition();


	int i = 1 ;
	for( ; it !=NULL ; it = esp_partition_next(it) )
	{
		const esp_partition_t *part = esp_partition_get(it);
		char *str;

		if( memcmp((void*)boot_part , (void*)part , sizeof(esp_partition_t)) == 0 )
		{
			str="boot partition";
		}
		else
		{
			str="-";
		}

		if (esp_ota_get_partition_description(part, &app_info) == ESP_OK)
		{
		    ESP_LOGI("ver", "======= partition type for App(%d)== %s =====", i++, str);
		    ESP_LOGI("ver", "      magic_word : < 0x%08x >", (int)app_info.magic_word);
		    ESP_LOGI("ver", "         version : < %s >",     app_info.version);
		    ESP_LOGI("ver", "compilation_time : < %s >",     app_info.time);
		    ESP_LOGI("ver", "         idf_ver : < %s >",     app_info.idf_ver);
		    ESP_LOGI("ver", "  secure_version : < 0x%08x >", (int)app_info.secure_version);
			hexdump3("app_elf_sha256", app_info.app_elf_sha256, sizeof(app_info.app_elf_sha256)); 
			hexdump3("MESH ID", (void*)MESH_ID, sizeof(MESH_ID)); 

		}
		else
		{
			ESP_LOGW("ver", "----- empty(partition %d : esp_ota_partition_description() != ESP_OK)",i++);
		}
	}
	#endif
	time_t now;
	struct tm timeinfo;
    char strftime_buf[64];
    time(&now);
	setenv("TZ", "KST-9", 1);
	tzset();
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	ESP_LOGI("show", "The current date/time in Korea(South) : %s", strftime_buf);
	ESP_LOGI("ntpserver", "  ntpserver : %s", (char *)myntp_str);
	printf("----------------------------------------------------------------------------------\n");

	if( strcmp( (char *)mesh_ap_ssid_current_connected, (char *)mesh_ap_ssid) != STR_MATCH ) 
	{
		ESP_LOGE("WiFi ssid", "");
		ESP_LOGE("WiFi ssid", " current connected SSID is differ from ssid in nvs : need to Reboot");
		ESP_LOGE("WiFi ssid", "     %s   : vs : %s ", mesh_ap_ssid_current_connected, mesh_ap_ssid);
		ESP_LOGE("WiFi ssid", "");
	}


	return 0;
//  	return (int)err;
}

void register_view_wifi_mesh_ap(void)
{
    const esp_console_cmd_t cmd = {
        .command = "show",
        .help = "view WIFI_MESH AP SSID",
        .hint = NULL,
        .func = do_view_wifi_mesh_ap,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
//      // re-register the same command, just for test
//      ESP_OK(esp_console_cmd_register(&cmd));
//      ESP_OK(esp_console_deinit());
}
//-------------------------------------------------



static int do_setup_wifi_mesh_ap(int argc, char **argv)
{
	esp_err_t err ;
//      ESP_ERROR_CHECK(esp_mesh_stop());
//  
//      /*  mesh initialization */
//      ESP_ERROR_CHECK(esp_mesh_init());
//  //  	//----------------------------------------------------------------
//      ESP_ERROR_CHECK(esp_event_handler_register(MESH_EVENT, ESP_EVENT_ANY_ID, &mesh_event_handler, NULL));
//  //      ESP_ERROR_CHECK(esp_mesh_set_max_layer(CONFIG_MESH_MAX_LAYER));
//      ESP_ERROR_CHECK(esp_mesh_set_max_layer(6));
//  
//      ESP_ERROR_CHECK(esp_mesh_set_vote_percentage(1));
//      ESP_ERROR_CHECK(esp_mesh_set_ap_assoc_expire(10));
//      mesh_cfg_t cfg = MESH_INIT_CONFIG_DEFAULT();
//      /* mesh ID */
//      memcpy((uint8_t *) &cfg.mesh_id, MESH_ID, 6);
//      /* router */
//  //      cfg.channel = CONFIG_MESH_CHANNEL;
//      cfg.channel = 0;
//  
//      cfg.router.ssid_len = strlen(argv[1]);
//      memcpy((uint8_t *) &cfg.router.ssid, argv[1], cfg.router.ssid_len);
//      memcpy((uint8_t *) &cfg.router.password, argv[2], strlen(argv[2]));
//      /* mesh softAP */
//  //      ESP_ERROR_CHECK(esp_mesh_set_ap_authmode(CONFIG_MESH_AP_AUTHMODE));
//      ESP_ERROR_CHECK(esp_mesh_set_ap_authmode(4));
//  
//  //      cfg.mesh_ap.max_connection = CONFIG_MESH_AP_CONNECTIONS;
//      cfg.mesh_ap.max_connection = 8;
//  
//  //      cfg.mesh_ap.nonmesh_max_connection = CONFIG_MESH_NON_MESH_AP_CONNECTIONS;
//      cfg.mesh_ap.nonmesh_max_connection = 1;
//  
//      memcpy((uint8_t *) &cfg.mesh_ap.password, argv[2], strlen(argv[2]));
//      ESP_ERROR_CHECK(esp_mesh_set_config(&cfg));
//  
//  	//===============================================================================
//  	//shcho add
//      ESP_ERROR_CHECK(esp_mesh_send_block_time(5000));
//  
//      ESP_ERROR_CHECK(esp_mesh_start());
//      ESP_LOGI(MESH_TAG, "mesh starts successfully, heap:%" PRId32 ", %s",  esp_get_free_heap_size(),
//               esp_mesh_is_root_fixed() ? "root fixed" : "root not fixed");
//  
//  #if CONFIG_EXAMPLE_CONNECT_WIFI
//      /* Ensure to disable any WiFi power save mode, this allows best throughput
//       * and hence timings for overall OTA operation.
//       */
//      esp_wifi_set_ps(WIFI_PS_NONE);
//  #endif // CONFIG_EXAMPLE_CONNECT_WIFI
//      return 0;
	err = nvs_set_mesh_ap_ssid_passwd((uint8_t*)argv[1], (uint8_t *)argv[2]);	
	return (int)err;
}

void register_setup_wifi_mesh_ap(void)
{
    const esp_console_cmd_t cmd = {
        .command = "mesh_setup",
        .help = "reconfig WIFI_MESH AP SSID : after set : reboot",
        .hint = NULL,
        .func = do_setup_wifi_mesh_ap,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static esp_err_t do_prov_reset(int argc, char **argv)
{
    ESP_LOGW(MESH_TAG, "Clearing WiFi credentials from NVS for re-provisioning...");
    memset(mesh_ap_ssid, 0, sizeof(mesh_ap_ssid));
    memset(mesh_ap_passwd, 0, sizeof(mesh_ap_passwd));
    nvs_set_mesh_ap_ssid_passwd(mesh_ap_ssid, mesh_ap_passwd);
    ESP_LOGW(MESH_TAG, "WiFi SSID/password cleared. Rebooting to start provisioning...");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

static void register_prov_reset(void)
{
    const esp_console_cmd_t cmd = {
        .command = "prov_reset",
        .help = "Clear WiFi credentials and reboot to start BLE provisioning (QR code)",
        .hint = NULL,
        .func = do_prov_reset,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static esp_err_t do_get_nvs_str(int argc, char **argv)
{
	char str[200];
	iotech_get_nvs_str((uint8_t *)argv[1], (uint8_t *)str);
	hexdump3( argv[1], str, sizeof(str));
    return 0;
}

void register_nvs_get_str(void)
{
    const esp_console_cmd_t cmd = {
        .command = "get_nvs_str",
        .help = "get_nvs_str key ",
        .hint = NULL,
        .func = do_get_nvs_str,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static esp_err_t do_set_nvs_str(int argc, char **argv)
{
	char str[200];

	ESP_LOGW("shcho","한글 argc=%d, argv[1]=%s, argv[2]=%s(len=%d)", argc, argv[1], argv[2], strlen(argv[2]) );

	iotech_set_nvs_str((uint8_t *)argv[1], (uint8_t *)argv[2]);

	iotech_get_nvs_str((uint8_t *)argv[1], (uint8_t *)str);
	hexdump3( argv[1] , str, sizeof(str));
    return 0;
}

void register_nvs_set_str(void)
{
    const esp_console_cmd_t cmd = {
        .command = "set_nvs_str",
        .help = "set_nvs_str key value",
        .hint = NULL,
        .func = do_set_nvs_str,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}






//  #include <tcpip_adapter.h>
static int do_hello_cmd(int argc, char **argv)
{
// org : simple
    printf("Hello World: mesh_send() test: light_on/light_off to mesh_node except myself\n");
//      return 0;

//  	tcpip_adapter_ip_info_t ipInfo;
//  	char str[256];
//  	
//  	// IP address.
//  	tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipInfo);
//  //  	sprintf(str, "%x", ipInfo.ip.addr);
//  //  	ESP_LOGW("IP ADDR", "%s", str);
//  	printf("\n\n");
//  	printf("My IP: " IPSTR "\n", IP2STR(&ipinfo.ip));
//  	printf("\n\n");


//---------------------------------------------------------------
//  	// mesh에서는 root node에서도 0
//  	wifi_sta_list_t sta_list;
//  	esp_wifi_ap_get_sta_list( & sta_list ) ;
//  
//  	ESP_LOGW("STA_LIST", "sta_list.num=%d", sta_list.num);
//  
//  	for( int i = 0 ; i < sta_list.num ; i ++ )
//  	{
//  //  		ESP_LOGW("STA_LIST", "rssi=%d, is_mesh_child=%d :" MACSTR , 
//  //  					  sta_list.sta[i].is_mesh_child,
//  //  		              MAC2STR(sta_list.sta[i].mac) ); 
//  		printf("rssi=%d, is_mesh_child=%d", 
//  					  sta_list.sta[i].rssi,
//  					  sta_list.sta[i].is_mesh_child );
//  		hexdump3("mac addr", sta_list.sta[i].mac, 6);
//  	}
//---------------------------------------------------------------
    /* non-root do nothing but print */
	int send_count=0;
    esp_err_t err;
    mesh_addr_t route_table[CONFIG_MESH_ROUTE_TABLE_SIZE];
    int route_table_size = 0;

//  typedef struct {
//      uint8_t *data;         /**< data */
//      uint16_t size;         /**< data size */
//      mesh_proto_t proto;    /**< data protocol */
//      mesh_tos_t tos;        /**< data type of service */
//  } mesh_data_t;
    mesh_data_t data;
    data.data = tx_buf_tmp;
    data.size = sizeof(tx_buf_tmp); // 1460 : 0x05B4
    data.proto = MESH_PROTO_BIN; // enum 0
    data.tos = MESH_TOS_P2P;     // enum 0
    is_running = true;

	mesh_light_ctl_t light_on = {		 // packed 되지 않아서 structure member의 Size보다 크다
	    .cmd = MESH_CONTROL_CMD,         // uint8_t 0x02
	    .on = 1,                         // bool    0x01
	    .token_id = MESH_TOKEN_ID,       // uint8_t 0x00 
	    .token_value = MESH_TOKEN_VALUE, // uint16_t // 0xbeef
	};
	
	mesh_light_ctl_t light_off = {
	    .cmd = MESH_CONTROL_CMD,
	    .on = 0,
	    .token_id = MESH_TOKEN_ID,
	    .token_value = MESH_TOKEN_VALUE,
	};

//      if (!esp_mesh_is_root()) 
	{
        ESP_LOGI(MESH_TAG, "layer:%d, rtableSize:%d, %s", mesh_layer,
                 esp_mesh_get_routing_table_size(),
//                   (is_mesh_connected && esp_mesh_is_root()) ? "ROOT" : is_mesh_connected ? "NODE" : "DISCONNECT");
                 esp_mesh_is_root() ? "ROOT" : "NODE");
//          vTaskDelay(10 * 1000 / portTICK_PERIOD_MS);
//  //          continue;
    }
//  	else
	{
	    esp_mesh_get_routing_table((mesh_addr_t *) &route_table,
	                               CONFIG_MESH_ROUTE_TABLE_SIZE * 6, &route_table_size);
//  	    if (send_count && !(send_count % 100)) {
	        ESP_LOGI(MESH_TAG, "route_table size:%d/%d", route_table_size, esp_mesh_get_routing_table_size());
//  	    }
//  	    send_count++;

		memset(tx_buf_tmp, 0, sizeof(tx_buf_tmp));
		if( argc == 3 )
		{
			send_count=atoi(argv[1]);
		    tx_buf_tmp[25] = (send_count >> 24) & 0xff;
		    tx_buf_tmp[24] = (send_count >> 16) & 0xff;
		    tx_buf_tmp[23] = (send_count >> 8) & 0xff;
		    tx_buf_tmp[22] = (send_count >> 0) & 0xff;
			memcpy(&tx_buf_tmp[26], argv[2], strlen(argv[2]) );

			esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
			char *hostname;
//  //  			esp_err_t err = esp_netif_get_hostname(sta_netif, &tx_buf_tmp[26+strlen(argv[2])]);
//  			esp_err_t err = esp_netif_get_hostname(sta_netif, (const char **)&hostname);
			esp_netif_get_hostname(sta_netif, (const char **)&hostname);
			memcpy(&tx_buf_tmp[128], hostname, strlen(hostname) );
			ESP_LOGW("hostname", "hostname=%s", hostname);
			hexdump3("                   tx_buf_tmp(1st 160bytes only)", tx_buf_tmp, 160);

		}
		else
		{
			ESP_LOGE("TEST" , " hello 243 string_to_send");
			return 0;
		}

	    if (send_count % 2) {
	        memcpy(tx_buf_tmp, (uint8_t *)&light_on, sizeof(light_on));
	    } else {
	        memcpy(tx_buf_tmp, (uint8_t *)&light_off, sizeof(light_off));
	    }

		printf("data.data=%p\n", data.data);
		printf("data.size=%d(0x%04x)\n", data.size, data.size);
		printf("data.proto=%d\n", data.proto);
		printf("data.tos=%d\n", data.tos);
		hexdump3("data", &data, sizeof(data));
		hexdump3("data.size", &data.size, sizeof(data.size));
		hexdump3("data.proto", &data.proto, sizeof(data.proto));
		hexdump3("data.tos", &data.tos, sizeof(data.tos));
//  		hexdump3("data__data(before_send)", data.data, data.size );

	
		// Stored 된 s_route_table을 이용해야 한다. : NODE에서는 자기 자신만 나온다.
		printf("\n\n	route_table_size=%d(s_route_table_size=%d)\n\n\n", route_table_size, s_route_table_size);
//  	    for (int i = 0; i < route_table_size; i++) 
	    for (int i = 0; i < s_route_table_size; i++) 
		{
            if (MAC_ADDR_EQUAL(s_route_table[i].addr, my_mac_factory)) 
			{
				ESP_LOGW("mesh_send>>>>>", "\n\n	do not send to myself(%d):" MACSTR ">>>>>\n\n" , 
				                  i, MAC2STR((char*)&s_route_table[i].addr));
				continue;
			}
			ESP_LOGI("mesh_send", ">>>>>>>>>>>>>>>(%d):" MACSTR ">>>>>", 
			                      i, MAC2STR((char*)&s_route_table[i].addr));

	        err = esp_mesh_send(&s_route_table[i], &data, MESH_DATA_P2P, NULL, 0);

			printf("err=%d(%s):1\n", err, esp_err_to_name(err));
	        if (err) 
			{
	            ESP_LOGE(MESH_TAG,
	                     "err=%d ---- 1:[ROOT-2-UNICAST:%d][L:%d]parent:"MACSTR" to "MACSTR", heap:%" PRId32 "[err:0x%x, proto:%d, tos:%d]",
						 err,
	                     send_count, mesh_layer, MAC2STR(mesh_parent_addr.addr),
	                     MAC2STR(s_route_table[i].addr), esp_get_minimum_free_heap_size(),
	                     err, data.proto, data.tos);
	        } 
			else
//  			else if (!(send_count % 100)) 
			{
	            ESP_LOGW(MESH_TAG,
	                     "err=%d ---- 2:[ROOT-2-UNICAST:%d][L:%d][rtableSize:%d]parent:"MACSTR" to "MACSTR", heap:%" PRId32 "[err:0x%x, proto:%d, tos:%d]",
						 err,
	                     send_count, mesh_layer,
	                     esp_mesh_get_routing_table_size(),
	                     MAC2STR(mesh_parent_addr.addr),
	                     MAC2STR(s_route_table[i].addr), esp_get_minimum_free_heap_size(),
	                     err, data.proto, data.tos);

			    /* if route_table_size is less than 10, add delay to avoid watchdog in this task. */
			    if (s_route_table_size < 10) {
			        vTaskDelay(1 * 1000 / portTICK_PERIOD_MS);
			    }
		    }
		}	
	}
	return 0;
}

void register_hello(void)
{
//      esp_console_config_t console_config = ESP_CONSOLE_CONFIG_DEFAULT();
//      ESP_ERROR_CHECK(esp_console_init(&console_config));

    const esp_console_cmd_t cmd = {
        .command = "hello",
//          .help = "Print Hello World",
    	.help = "Hello World: mesh_send() test: light_on/light_off to mesh_node except myself",
        .hint = NULL,
        .func = do_hello_cmd,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
//      // re-register the same command, just for test
//      ESP_OK(esp_console_cmd_register(&cmd));
//      ESP_OK(esp_console_deinit());
}


//  #include <tcpip_adapter.h>
static int do_show_adc(int argc, char **argv)
{
	if( argc == 1 )
	{
		ESP_LOGW("do_show_adc", "      do_show_adc : set to default 0 ");
		ESP_LOGW("do_show_adc", "      do_show_adc : set to default 0 ");
		ESP_LOGW("do_show_adc", "      do_show_adc : set to default 0 ");
		show_adc = 0;
	}
	else
		show_adc = atoi(argv[1]);
	return 0;

}
void register_show_adc(void)
{
//      esp_console_config_t console_config = ESP_CONSOLE_CONFIG_DEFAULT();
//      ESP_ERROR_CHECK(esp_console_init(&console_config));

    const esp_console_cmd_t cmd = {
        .command = "show_adc",
        .help = "show_adc result on/off ( 0/1/2(verbose) ) ",
        .hint = NULL,
        .func = do_show_adc,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
//      // re-register the same command, just for test
//      ESP_OK(esp_console_cmd_register(&cmd));
//      ESP_OK(esp_console_deinit());
}

//  #include <tcpip_adapter.h>
static int do_sleep(int argc, char **argv)
{
	if( argc == 3 )
	{
		if( strncmp(argv[1], "deep", 4) == STR_MATCH )
		{
    		ESP_LOGI("do_sleep", "Entering deep sleep for %d seconds", atoi(argv[1]));
			esp_deep_sleep( atoi(argv[2]) * 1000000LL);
		}
		else if( strncmp(argv[1], "light", 5) == STR_MATCH )
		{
			#if 0
    		ESP_LOGI("do_sleep", "Entering light sleep for %d seconds", atoi(argv[1]));
			esp_light_sleep( atoi(argv[2]) * 1000000LL);
			#else 


			// Wakeup Source 등록
		    ESP_RETURN_ON_ERROR(esp_sleep_enable_timer_wakeup(atoi(argv[2])*1000000LL), "light_sleep : timer setting", "Configure timer as wakeup source failed");
		    ESP_LOGI("Timer Wakeup", "timer wakeup source is ready : %lld(usecs)", atoi(argv[2])*1000000LL );

			// already installed : uart_initialization()를 하는데 :error: already initialized
		    /* Enable wakeup from light sleep by uart */
		    example_register_uart_wakeup();

        	int64_t t_before_us = esp_timer_get_time();
	        /* Enter sleep mode */
	        esp_light_sleep_start();
	
	        /* Get timestamp after waking up from sleep */
	        int64_t t_after_us = esp_timer_get_time();
	
	        /* Determine wake up reason */
	        const char* wakeup_reason;
	        switch (esp_sleep_get_wakeup_cause()) 
			{
	            case ESP_SLEEP_WAKEUP_TIMER:
	                wakeup_reason = "timer";
	                break;
	            case ESP_SLEEP_WAKEUP_GPIO:
	                wakeup_reason = "pin";
	                break;
	            case ESP_SLEEP_WAKEUP_UART:
	                wakeup_reason = "uart";
	                /* Hang-up for a while to switch and execuse the uart task
	                 * Otherwise the chip may fall sleep again before running uart task */
	                vTaskDelay(1);
	                break;
				#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
	            case ESP_SLEEP_WAKEUP_TOUCHPAD:
	                wakeup_reason = "touch";
	                break;
				#endif
	            default:
	                wakeup_reason = "other";
	                break;
	        }
			#if CONFIG_NEWLIB_NANO_FORMAT
		        /* printf in newlib-nano does not support %ll format, causing example test fail */
		        printf("Returned from light sleep, reason: %s, t=%d ms, slept for %d ms\n",
		                wakeup_reason, (int) (t_after_us / 1000), (int) ((t_after_us - t_before_us) / 1000));
			#else
		        printf("Returned from light sleep, reason: %s, t=%lld ms, slept for %lld ms\n",
		                wakeup_reason, t_after_us / 1000, (t_after_us - t_before_us) / 1000);
			#endif
	        if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) 
			{
	            /* Waiting for the gpio inactive, or the chip will continously trigger wakeup*/
	            example_wait_gpio_inactive();
        	}
			#endif
		}
	}

	return 0;

}
void register_sleep(void)
{
//      esp_console_config_t console_config = ESP_CONSOLE_CONFIG_DEFAULT();
//      ESP_ERROR_CHECK(esp_console_init(&console_config));

    const esp_console_cmd_t cmd = {
        .command = "sleep",
        .help = "sleep [deep/light] [ secs ] ",
//          .command = "light_sleep",
//          .help = "light_sleep [ secs ] ",
        .hint = NULL,
        .func = do_sleep,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
//      // re-register the same command, just for test
//      ESP_OK(esp_console_cmd_register(&cmd));
//      ESP_OK(esp_console_deinit());
}

//  #include <tcpip_adapter.h>
static int do_adc_suspend(int argc, char **argv)
{
	adc_suspend = atoi(argv[1]);
	return 0;

}
void register_adc_suspend(void)
{
//      esp_console_config_t console_config = ESP_CONSOLE_CONFIG_DEFAULT();
//      ESP_ERROR_CHECK(esp_console_init(&console_config));

    const esp_console_cmd_t cmd = {
        .command = "adc_suspend",
        .help = "set adc_suspend ( 0/1 ) ) ",
        .hint = NULL,
        .func = do_adc_suspend,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
//      // re-register the same command, just for test
//      ESP_OK(esp_console_cmd_register(&cmd));
//      ESP_OK(esp_console_deinit());
}






static void initialize_console(void)
{
    /* Drain stdout before reconfiguring it */
    fflush(stdout);
    fsync(fileno(stdout));

    /* Disable buffering on stdin */
    setvbuf(stdin, NULL, _IONBF, 0);

#if CONFIG_CONSOLE_UART_NUM == 0
    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    esp_vfs_dev_uart_port_set_rx_line_endings(0, ESP_LINE_ENDINGS_CR);

    /* Move the caret to the beginning of the next line on '\n' */
    esp_vfs_dev_uart_port_set_tx_line_endings(0, ESP_LINE_ENDINGS_CRLF);

    /* Configure UART. Note that REF_TICK is used so that the baud rate remains
     * correct while APB frequency is changing in light sleep mode.
     */
    const uart_config_t uart_config = {.baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
                                       .data_bits = UART_DATA_8_BITS,
                                       .parity = UART_PARITY_DISABLE,
                                       .stop_bits = UART_STOP_BITS_1,
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
                                       .source_clk = UART_SCLK_REF_TICK,
#else
                                       .source_clk = UART_SCLK_XTAL,
#endif
    };

    /* Install UART driver for interrupt-driven reads and writes */
    ESP_ERROR_CHECK(uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM,
                                        256, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config));

    /* Tell VFS to use UART driver */
    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
#else
    ESP_LOGI(TAG, "UART console is disabled. ");
#endif

    /* Initialize the console */
    esp_console_config_t console_config = {.max_cmdline_args = 8,
                                           .max_cmdline_length = 256,
#if CONFIG_LOG_COLORS
                                           .hint_color = atoi(LOG_COLOR_CYAN)
#endif
    };
    ESP_ERROR_CHECK(esp_console_init(&console_config));

    /* Configure linenoise line completion library */
    /* Enable multiline editing. If not set, long commands will scroll within
     * single line.
     */
    linenoiseSetMultiLine(1);

    /* Tell linenoise where to get command completions and hints */
    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback *)&esp_console_get_hint);

    /* Set command history size */
    linenoiseHistorySetMaxLen(100);
}


//void make_console(void)
void make_consoleTask(void* pvParameters)
{
const char *prompt = LOG_COLOR_I "esp32_mesh_iotech> " LOG_RESET_COLOR;

	initialize_console();
    /* Register commands */
    esp_console_register_help_command();
    register_system();
    register_hello();
    register_ota_run_test();
    register_ota_stop();
    register_view_tasks();
    register_setup_wifi_mesh_ap();
    register_prov_reset();
	register_view_wifi_mesh_ap();
	register_nvs_get_str();
	register_nvs_set_str();
	register_show_adc();
	register_adc_suspend();
	register_sleep();
    // register_test();
	register_test_spi();
    register_date();
    register_meter();
    register_temperature();
    register_autocalibration();
    register_eventflag();
    register_readDB();
    register_readConstant();
    register_relayOn();
    register_relayOff();
	register_relayNop();
    register_respOK();
    register_setConstant();
	register_setMeter();
   // register_testDMA();	
    register_view_av_wav_show();


//      register_router();
//      fillMac();
//      get_config_param_str("ssid", &ssid);
//      if (ssid == NULL)
//      {
//          ssid = param_set_default("");
//      }
//      get_config_param_str("passwd", &passwd);
//      if (passwd == NULL)
//      {
//          passwd = param_set_default("");
//      }
//      get_config_param_str("static_ip", &static_ip);
//      if (static_ip == NULL)
//      {
//          static_ip = param_set_default("");
//      }
//      get_config_param_str("subnet_mask", &subnet_mask);
//      if (subnet_mask == NULL)
//      {
//          subnet_mask = param_set_default("");
//      }
//      get_config_param_str("gateway_addr", &gateway_addr);
//      if (gateway_addr == NULL)
//      {
//          gateway_addr = param_set_default("");
//      }
//      get_config_param_str("ap_ssid", &ap_ssid);
//      if (ap_ssid == NULL)
//      {
//          ap_ssid = param_set_default("ESP32_NAT_Router");
//      }
//      get_config_param_str("ap_passwd", &ap_passwd);
//      if (ap_passwd == NULL)
//      {
//          ap_passwd = param_set_default("");
//      }
//      get_config_param_str("ap_ip", &ap_ip);
//      if (ap_ip == NULL)
//      {
//          char *defaultIP = getDefaultIPByNetmask();
//          ap_ip = param_set_default(defaultIP);
//          free(defaultIP);
//      }
//  
//      get_config_param_str("sta_user", &sta_user);
//      get_config_param_str("sta_identity", &sta_identity);
//  
//      get_config_param_str("lock_pass", &lock_pass);
//      if (lock_pass == NULL)
//      {
//          lock_pass = param_set_default("");
//      }
//  
//      char *scan_result = NULL;
//      get_config_param_str("scan_result", &scan_result);
//      int32_t result_shown = 0;
//      get_config_param_int("result_shown", &result_shown);
//  
//      if (scan_result != NULL && result_shown >= 3)
//      {
//          erase_key("scan_result");
//          erase_key("result_shown");
//          ESP_LOGI(TAG, "Scan result was shown %ld times. Result will be deleted", result_shown);
//      }
//      else if (scan_result != NULL && result_shown > 0)
//      {
//          nvs_handle_t nvs;
//          ESP_ERROR_CHECK(nvs_open(PARAM_NAMESPACE, NVS_READWRITE, &nvs));
//          nvs_set_i32(nvs, "result_shown", ++result_shown);
//          ESP_LOGI(TAG, "result_shown increased to %ld after reboot", result_shown);
//      }
//  
//      get_portmap_tab();
//  
//      // Setup WIFI
//      wifi_init(ssid, passwd, static_ip, subnet_mask, gateway_addr, ap_ssid, ap_passwd, ap_ip, sta_user, sta_identity);
//  
//  	//shcho start ----------------------------------
//      ESP_LOGW("ETHERNET Start", "\napp_main_ethernet() started ??????????\n");
//  	app_main_ethernet();
//  
//  	//shcho end ----------------------------------
//  
//  
//      pthread_t t1;
//      int32_t led_disabled = 0;
//      get_config_param_int("led_disabled", &led_disabled);
//      if (led_disabled == 0)
//      {
//          ESP_LOGI(TAG, "On board LED is enabled");
//          pthread_create(&t1, NULL, led_status_thread, NULL);
//      }
//      else
//      {
//          ESP_LOGI(TAG, "On board LED is disabled");
//      }
//      int32_t nat_disabled = 0;
//      get_config_param_int("nat_disabled", &nat_disabled);
//      if (nat_disabled == 0)
//      {
//          ip_napt_enable(my_ap_ip, 1);
//          ESP_LOGI(TAG, "NAT is enabled");
//      }
//      else
//      {
//          ESP_LOGI(TAG, "NAT is disabled");
//      }
//  
//      int32_t lock = 0;
//      get_config_param_int("lock", &lock);
//      if (lock == 0)
//      {
//          ESP_LOGI(TAG, "Starting config web server");
//          start_webserver();
//      }
//      else
//      {
//          ESP_LOGW(TAG, "Web server is disabled. Reenable with following commands and reboot afterwards:");
//          ESP_LOGW(TAG, "'nvs_namespace esp32_nat'");
//          ESP_LOGW(TAG, "'nvs_set lock i32 -v 0'");
//      }
//  
//      /* Prompt to be printed before each line.
//       * This can be customized, made dynamic, etc.
//       */
//      const char *prompt = LOG_COLOR_I "esp32nre> " LOG_RESET_COLOR;
//  
//      printf("\n"
//             "ESP32 NAT ROUTER EXTENDED\n"
//             "Type 'help' to get the list of commands.\n"
//             "Use UP/DOWN arrows to navigate through command history.\n"
//             "Press TAB when typing command name to auto-complete.\n");
//  
//      if (strlen(ssid) == 0)
//      {
//          printf("\n"
//                 "Unconfigured WiFi\n"
//                 "Configure using 'set_sta' and 'set_ap' and restart.\n");
//      }
//  
//      /* Figure out if the terminal supports escape sequences */
//      int probe_status = linenoiseProbe();
//      if (probe_status)
//      { /* zero indicates success */
//          printf("\n"
//                 "Your terminal application does not support escape sequences.\n"
//                 "Line editing and history features are disabled.\n"
//                 "On Windows, try using Putty instead.\n");
//          linenoiseSetDumbMode(1);
//  #if CONFIG_LOG_COLORS
//          /* Since the terminal doesn't support escape sequences,
//           * don't use color codes in the prompt.
//           */
//          prompt = "esp32nre> ";
//  #endif // CONFIG_LOG_COLORS
//      }
//  
//  
    /* Main loop */
    while (true)
    {
        /* Get a line using linenoise.
         * The line is returned when ENTER is pressed.
         */
        char *line = linenoise(prompt);
        if (line == NULL)
        { /* Ignore empty lines */
            continue;
        }
        /* Add the command to the history */
        linenoiseHistoryAdd(line);
        /* Try to run the command */
        int ret;
        esp_err_t err = esp_console_run(line, &ret);
        if (err == ESP_ERR_NOT_FOUND)
        {
            printf("Unrecognized command\n");
            printf("cmd: %s\n", line);
        }
        else if (err == ESP_ERR_INVALID_ARG)
        {
            // command was empty
        }
        else if (err == ESP_OK && ret != ESP_OK)
        {
            printf("Command returned non-zero error code: 0x%x (%s)\n", ret, esp_err_to_name(ret));
        }
        else if (err != ESP_OK)
        {
            printf("Internal error: %s\n", esp_err_to_name(err));
        }
        /* linenoise allocates line buffer on the heap, so need to free it */
        linenoiseFree(line);
    }
}

esp_err_t iotech_get_nvs_str(uint8_t *key, uint8_t *value)
{
	nvs_handle_t nvs_handle;
	size_t len= 0 ;

    esp_err_t  err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) 
	{
		ESP_LOGE("\n", "nvs_open(%s,,,) Failed(%s)", STORAGE_NAMESPACE, esp_err_to_name(err));
	}

	char *blob ;
    if( (err = nvs_get_str(nvs_handle, (char *)key,   NULL, &len)) == ESP_OK )
	{
		blob = (char *)malloc(len);
		if( (err = nvs_get_str(nvs_handle, (char *)key, blob, &len)) == ESP_OK )
		{
//  			ESP_LOGI("result nvs_get_str", "nvs_get_str() len=%d, err=%d(%s)(actually read): OK", len, err, esp_err_to_name(err));
//  			hexdump3((char *)key, blob, len);

			memcpy((char *)value, blob, len);
		}
		free(blob);
	}
	nvs_commit(nvs_handle);
	nvs_close(nvs_handle);

	return err;

}
esp_err_t  iotech_set_nvs_str(uint8_t *key, uint8_t *value)
{
	nvs_handle_t nvs_handle;
//  	size_t len= 0 ;

    esp_err_t  err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) 
	{
		ESP_LOGE("\n", "nvs_open(%s,,,) Failed(%s)", STORAGE_NAMESPACE, esp_err_to_name(err));
//  		return err;
	}

	err = nvs_set_str(nvs_handle, (char *)key, (char *)value);

	nvs_commit(nvs_handle);
	nvs_close(nvs_handle);

	return  err;
}

void nvs_get_ntp_server(uint8_t *ntpserver)
{
	nvs_handle_t nvs_handle;
	size_t len= 0 ;

    esp_err_t  err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) 
	{
		ESP_LOGE("\n", "nvs_open(%s,,,) Failed(%s)", STORAGE_NAMESPACE, esp_err_to_name(err));
//  		return err;
	}

	char *blob ;
//  	len = sizeof(mesh_ap_ssid);
//---------------------------------------------------------------------------------------------
//      err = nvs_get_str(nvs_handle, "mesh_ap_ssid",   (char *)mesh_ap_ssid, &len);
//=============================================================================================
    if( (err = nvs_get_str(nvs_handle, "ntpserver",   NULL, &len)) == ESP_OK )
	{
		blob = (char *)malloc(len);
		ESP_LOGI("s", "nvs_get_str() len=%d, err=%d(get_len_only):ntpserver", len, err);
		if( (err = nvs_get_str(nvs_handle, "ntpserver", blob, &len)) == ESP_OK )
		{
			ESP_LOGI("s", "nvs_get_str() len=%d, err=%d(%s)(actually read): OK:ntpserver", len, err, esp_err_to_name(err));
			hexdump3("ntpserver(blob)", blob, len);

	//  			memcpy(myntp_str, blob, len);
			memcpy(ntpserver, blob, len);
		}
		else
		{
			ESP_LOGE("\n", "nvs_get_blob(...ntpserver... ) Failed(%s)(actually) : set to default(none):ntpserver", esp_err_to_name(err));
			iotech_set_nvs_str((uint8_t *)"ntpserver",(uint8_t *)"ntp.iotech.org");
			iotech_get_nvs_str((uint8_t *)"ntpserver", ntpserver);
		}
		free(blob);
	}
	else
	{
		ESP_LOGE("NVS str", "nvs_get_str(): ntpserver: Error");
		iotech_set_nvs_str((uint8_t *)"ntpserver",(uint8_t *)"ntp.iotech.org");
		iotech_get_nvs_str((uint8_t *)"ntpserver", ntpserver);
	}
//---------------------------------------------------------------------------------------------
	nvs_commit(nvs_handle);
	nvs_close(nvs_handle);

}


void nvs_get_mesh_ap_ssid_passwd(uint8_t *ssid, uint8_t *passwd)
{
	nvs_handle_t nvs_handle;
	size_t len= 0 ;

    esp_err_t  err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) 
	{
		ESP_LOGE("\n", "nvs_open(%s,,,) Failed(%s)", STORAGE_NAMESPACE, esp_err_to_name(err));
		ESP_LOGE("\n", "nvs_open(%s,,,) Failed(%s)", STORAGE_NAMESPACE, esp_err_to_name(err));
		ESP_LOGE("\n", "nvs_open(%s,,,) Failed(%s)", STORAGE_NAMESPACE, esp_err_to_name(err));
		ESP_LOGE("\n", "nvs_open(%s,,,) Failed(%s)", STORAGE_NAMESPACE, esp_err_to_name(err));
		ESP_LOGE("\n", "nvs_open(%s,,,) Failed(%s)", STORAGE_NAMESPACE, esp_err_to_name(err));
		ESP_LOGE("\n", "nvs_open(%s,,,) Failed(%s)", STORAGE_NAMESPACE, esp_err_to_name(err));
		ESP_LOGE("\n", "nvs_open(%s,,,) Failed(%s)", STORAGE_NAMESPACE, esp_err_to_name(err));
//  		return err;
	}
	ESP_LOGE("\n", "nvs_open(%s,,,) err(%s)", STORAGE_NAMESPACE, esp_err_to_name(err));

	char *blob ;
//  	len = sizeof(mesh_ap_ssid);
//---------------------------------------------------------------------------------------------
//      err = nvs_get_str(nvs_handle, "mesh_ap_ssid",   (char *)mesh_ap_ssid, &len);
//=============================================================================================
    if( (err = nvs_get_str(nvs_handle, "mesh_ap_ssid",   NULL, &len)) == ESP_OK )
//      if( (err = nvs_get_str(nvs_handle, "mesh_ap_ssid",   (char*)ssid, &len)) == ESP_OK )
	{
		blob = (char *)malloc(len);
		ESP_LOGI("s", "nvs_get_str() len=%d, err=%d(get_len_only):mesh_ap_ssid", len, err);
		if( (err = nvs_get_str(nvs_handle, "mesh_ap_ssid", blob, &len)) == ESP_OK )
		{
//  			ESP_LOGI("s", "nvs_get_str() len=%d, err=%d(%s)(actually read): OK", len, err, esp_err_to_name(err));
//  			hexdump3("mesh_ap_ssid(blob)", blob, len);
			memcpy(ssid, blob, len);
		}
		else
		{
			ESP_LOGE("\n", "nvs_get_blob(... mesh_ap_ssid... ) Failed(%s)(actually) : set to default", esp_err_to_name(err));
	    	err = nvs_set_str(nvs_handle, "mesh_ap_ssid", MESH_AP_SSID_DEFAULT);
			memcpy(ssid, MESH_AP_SSID_DEFAULT, strlen(MESH_AP_SSID_DEFAULT));
		}
		free(blob);
	}
	else
	{
		ESP_LOGE("\n", "nvs_get_blob(... mesh_ap_ssid... ) Failed(%s) : set to default", esp_err_to_name(err));
	   	err = nvs_set_str(nvs_handle, "mesh_ap_ssid", MESH_AP_SSID_DEFAULT);
		memcpy(ssid, MESH_AP_SSID_DEFAULT, strlen(MESH_AP_SSID_DEFAULT));

		ESP_LOGE("\n", "nvs_set(%s) nvs_set_str(mesh_ap_ssid) : err(%s)", STORAGE_NAMESPACE, esp_err_to_name(err));

	}

    if( (err = nvs_get_str(nvs_handle, "mesh_ap_passwd",   NULL, &len)) == ESP_OK )
//      if( (err = nvs_get_str(nvs_handle, "mesh_ap_passwd",   (char *)passwd, &len)) == ESP_OK )
	{
		blob = (char *)malloc(len);
		ESP_LOGI("s", "nvs_get_str() len=%d, err=%d(get_len_only):mesh_ap_passwd", len, err);
		if( (err = nvs_get_str(nvs_handle, "mesh_ap_passwd", blob, &len)) == ESP_OK )
		{
//  			ESP_LOGI("s", "nvs_get_str() len=%d, err=%d(%s)(actually read): OK", len, err, esp_err_to_name(err));
//  			hexdump3("mesh_ap_passwd(blob)", blob, len);
			memcpy(passwd, blob, len);
		}
		else
		{
			ESP_LOGE("\n", "nvs_get_blob(... mesh_ap_passwd... ) Failed(%s)(actually) : set to default", esp_err_to_name(err));
	    	err = nvs_set_str(nvs_handle, "mesh_ap_passwd", MESH_AP_PASSWD_DEFAULT);
			memcpy(passwd, MESH_AP_PASSWD_DEFAULT, strlen(MESH_AP_PASSWD_DEFAULT));
		}
		free(blob);
	}
	else
	{
		ESP_LOGE("\n", "nvs_get_blob(... mesh_ap_passwd... ) Failed(%s) : set to default", esp_err_to_name(err));
	   	err = nvs_set_str(nvs_handle, "mesh_ap_passwd", MESH_AP_PASSWD_DEFAULT);
		memcpy(passwd, MESH_AP_PASSWD_DEFAULT, strlen(MESH_AP_PASSWD_DEFAULT));
		ESP_LOGE("\n", "nvs_set(%s) passwd : err(%s)", STORAGE_NAMESPACE, esp_err_to_name(err));
	}
//---------------------------------------------------------------------------------------------
	nvs_commit(nvs_handle);
	nvs_close(nvs_handle);

}

esp_err_t  nvs_set_mesh_ap_ssid_passwd(uint8_t *ssid, uint8_t *passwd)
{
	nvs_handle_t nvs_handle;
//  	size_t len= 0 ;

    esp_err_t  err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) 
	{
		ESP_LOGE("\n", "nvs_open(%s,,,) Failed(%s)", STORAGE_NAMESPACE, esp_err_to_name(err));
//  		return err;
	}

	err = nvs_set_str(nvs_handle, "mesh_ap_ssid", (char *)ssid);
	err = nvs_set_str(nvs_handle, "mesh_ap_passwd", (char *)passwd);

	nvs_commit(nvs_handle);
	nvs_close(nvs_handle);

	nvs_get_mesh_ap_ssid_passwd(mesh_ap_ssid, mesh_ap_passwd);
//  	esp_restart(); // --> 수동으로 restart
	return 0;

}


esp_err_t  nvs_set_mqtt_broker_uri(uint8_t *uri)
{
	nvs_handle_t nvs_handle;
//  	size_t len= 0 ;

    esp_err_t  err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) 
	{
		ESP_LOGE("\n", "nvs_open(%s,,,) Failed(%s)", STORAGE_NAMESPACE, esp_err_to_name(err));
//  		return err;
	}

	err = nvs_set_str(nvs_handle, "mqtt_broker_uri", (char *)uri);

	nvs_commit(nvs_handle);
	nvs_close(nvs_handle);

	return 0;



}

//  static const char *JSON_TAG = "JSON";
//  void test_json(void)
//  {
//  	//  I (1756) JSON: Serialize.....
//  	//  I (1766) JSON: my_json_string
//  	//  {
//  	//  	"version":	"v5.2.1-dirty",
//  	//  	"cores":	2,
//  	//  	"flag_true":	true,
//  	//  	"flag_false":	false
//  	//  }
//  	//  I (1776) JSON: Deserialize.....
//  	//  I (1776) JSON: version=v5.2.1-dirty
//  	//  I (1786) JSON: cores=2
//  	//  I (1786) JSON: flag_true=1
//  	//  I (1786) JSON: flag_false=0
//  
//  	ESP_LOGI(JSON_TAG, "Serialize.....");
//  	cJSON *root;
//  	root = cJSON_CreateObject();
//  	esp_chip_info_t chip_info;
//  	esp_chip_info(&chip_info);
//  	cJSON_AddStringToObject(root, "version", IDF_VER);
//  	cJSON_AddNumberToObject(root, "cores", chip_info.cores);
//  	cJSON_AddTrueToObject(root, "flag_true");
//  	cJSON_AddFalseToObject(root, "flag_false");
//  	//const char *my_json_string = cJSON_Print(root);
//  	char *my_json_string = cJSON_Print(root);
//  	ESP_LOGI(JSON_TAG, "my_json_string\n%s",my_json_string);
//  	cJSON_Delete(root);
//  
//  	ESP_LOGI(JSON_TAG, "Deserialize.....");
//  	cJSON *root2 = cJSON_Parse(my_json_string);
//  	if (cJSON_GetObjectItem(root2, "version")) {
//  		char *version = cJSON_GetObjectItem(root2,"version")->valuestring;
//  		ESP_LOGI(JSON_TAG, "version=%s",version);
//  	}
//  	if (cJSON_GetObjectItem(root2, "cores")) {
//  		int cores = cJSON_GetObjectItem(root2,"cores")->valueint;
//  		ESP_LOGI(JSON_TAG, "cores=%d",cores);
//  	}
//  	if (cJSON_GetObjectItem(root2, "flag_true")) {
//  		bool flag_true = cJSON_GetObjectItem(root2,"flag_true")->valueint;
//  		ESP_LOGI(JSON_TAG, "flag_true=%d",flag_true);
//  	}
//  	if (cJSON_GetObjectItem(root2, "flag_false")) {
//  		bool flag_false = cJSON_GetObjectItem(root2,"flag_false")->valueint;
//  		ESP_LOGI(JSON_TAG, "flag_false=%d",flag_false);
//  	}
//  	cJSON_Delete(root2);
//  
//  	// Buffers returned by cJSON_Print must be freed by the caller.
//  	// Please use the proper API (cJSON_free) rather than directly calling stdlib free.
//  	cJSON_free(my_json_string);
//  }

TaskHandle_t xTaskHandle_spi = NULL;
TimerHandle_t timer_handle;

volatile  bool  time_1min = false;
volatile  bool  time_1day = false;

static int counter;
static int counter_1day; 

void timer_callback(TimerHandle_t xTimer) {
    counter++;
    counter_1day++;
    // 1분(60초) 기준으로 카운터를 클리어
    if (counter >= 60) {
        time_1min = true;
        counter = 0;
    }
 
    if (counter_1day >= 86400) {
        time_1day = true;
        counter_1day = 0;
    }
}









void app_main(void)
{
//  	app_main_wifi_scan();

    /* 내부 SRAM 파편화 방지: 부팅 직후 연속 블록 예약.
     * MQTT 라이브러리 태스크 TCB는 반드시 내부 SRAM 연속 블록이 필요.
     * mqtt_app_start()에서 esp_mqtt_client_start() 직전에 해제됨.   */
    g_mqtt_internal_sram_reserve = heap_caps_malloc(8192, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (g_mqtt_internal_sram_reserve) {
        ESP_LOGW("app_main", "Internal SRAM reserve 8KB OK (for MQTT task)");
    } else {
        ESP_LOGE("app_main", "Internal SRAM reserve FAILED - MQTT task may fail later");
    }

#if (TCP_SERVER_TEST_ONLY == 0)
	//---------------------------------------------------
	// NVS: 한 번만 init. (연속 두 번 호출 시 ESP_ERR_INVALID_STATE로 실패할 수 있음)
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // 1.OTA app partition table has a smaller NVS partition size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // 2.NVS partition contains data in new format and cannot be recognized by this version of code.
        // If this happens, we erase NVS partition and initialize NVS again.
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
	ESP_ERROR_CHECK( err ) ;



	nvs_get_mesh_ap_ssid_passwd( mesh_ap_ssid, mesh_ap_passwd);
	// iotech_get_nvs_str((uint8_t *)"mesh_ap_ssid", mesh_ap_ssid);
	// iotech_get_nvs_str((uint8_t *)"mesh_ap_passswd", mesh_ap_passwd);

    memset(myntp_str, 0, sizeof(myntp_str));
	nvs_get_ntp_server((uint8_t*)myntp_str);  
    // iotech_get_nvs_str((uint8_t *)"ntpserver", myntp_str);

    xTaskCreate(make_consoleTask, "console_task", 4096, 0, 5, &xHandle_adc);

	ESP_LOGI("NVS mesh_ap_ssid  ", "mesh_ap_ssid  =%s", mesh_ap_ssid);
	ESP_LOGI("NVS mesh_ap_passwd(after nvs_read)", "mesh_ap_passwd=%s", mesh_ap_passwd);
	hexdump3("mesh_ap_ssid", mesh_ap_ssid, strlen((char *)mesh_ap_ssid));
	hexdump3("mesh_ap_passwd(after nvs_read)", mesh_ap_passwd, strlen((char *)mesh_ap_passwd));
	hexdump3("ntpserver(after nvs_read)", myntp_str, strlen((char *)myntp_str));

	ESP_LOGI(" ", "\n\n");

  //  init_db(); 




    get_sha256_of_partitions();
	//---------------------------------------------------
	//==========여기로 끌어올림: 이동함=====================================================================
//  I (1100) wifi:mode : sta (30:ae:a4:9e:54:04) + softAP (30:ae:a4:9e:54:05)
//  I (1100) wifi:enable tsf
//  I (1110) wifi:Total power save buffer number: 16
//  I (1110) wifi:Init max length of beacon: 752/752
//  I (1110) wifi:Init max length of beacon: 752/752
//  I (1130) mesh: <nvs>read layer:0
//  I (1130) mesh: <nvs>read assoc:0
//  I (1140) wifi:Total power save buffer number: 16
//  ***** esp_read_mac(ESP_MAC_BASE) 20 bytes *****
//  <0x0000> 30 AE A4 9E  54 04 FB 3F  00 00 00 00  FE 01 00 00  0...T..?........
//  <0x0010> 84 1E 40 3F                                         ..@?
//  
//  ***** esp_read_mac(ESP_MAC_EFUSE_FACTORY) 20 bytes *****
//  <0x0000> 30 AE A4 9E  54 04 FB 3F  00 00 00 00  FE 01 00 00  0...T..?........
//  <0x0010> 84 1E 40 3F                                         ..@?
//  
//  I (1430) wifi:mode : sta (30:ae:a4:9e:54:04)

//      ESP_ERROR_CHECK(esp_read_mac(my_mac_factory, ESP_MAC_BASE ));
//  	hexdump3("esp_read_mac(ESP_MAC_BASE)", my_mac_factory, sizeof(my_mac_factory));

    ESP_ERROR_CHECK(esp_read_mac(my_mac_factory, ESP_MAC_EFUSE_FACTORY ));
	hexdump3("esp_read_mac(ESP_MAC_EFUSE_FACTORY)", my_mac_factory, sizeof(my_mac_factory));


    /* NVS "ID" 우선; 없으면 임시 3W12345 (mux 검출 후 mesh_refresh_device_id_after_board_detect에서 보정) */
    {
        char nvs_id[32] = {0};
        iotech_get_nvs_str((uint8_t*)"ID", (uint8_t*)nvs_id);
        if (nvs_id[0] != 0) {
            strncpy(device_id, nvs_id, sizeof(device_id) - 1);
        } else {
            strncpy(device_id, "3W12345", sizeof(device_id) - 1);
        }
        device_id[sizeof(device_id) - 1] = '\0';
        ESP_LOGW(MESH_TAG, "device_id=[%s] (nvs=%s, pre-mux)", device_id, nvs_id[0] ? nvs_id : "none");
    }

    /* IP 수신·MQTT 시작 전에 mux·device_id 확정 (스태틱 OTA 완료 토픽이 잘못된 ID로 나가는 문제 방지) */
    extern void stella_early_mux_and_refresh_device_id(void);
    stella_early_mux_and_refresh_device_id();

    /*  tcpip initialization */
    ESP_ERROR_CHECK(esp_netif_init());
    /*  event initialization */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

	//---------------------------------------
    /* 메시 비활성화: 모든 디바이스(스태틱 포함)가 공유기에 직접 STA 연결.
     * 메시 루트/자식 구분이 없으므로 OTA·MQTT 모두 직결로 안정적 동작. */
    g_stella_wearable_plain_sta = true;

    if (1) { /* was: if (flag_IS_WEARABLE) — 이제 스태틱도 동일 경로 */
        if (esp_netif_create_default_wifi_sta() == NULL) {
            ESP_LOGE(MESH_TAG, "esp_netif_create_default_wifi_sta failed");
            abort();
        }
        wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
        sprintf(g_hostname, "stella-%02X%02X%02X--%02X%02X%02X", MAC2STR(my_mac_factory));
        hexdump3("set hostname(1:pre)", g_hostname, sizeof(g_hostname));
        stella_set_hostname(g_hostname);

        size_t sl = strlen((char *)mesh_ap_ssid);
        bool need_prov = (flag_IS_WEARABLE == 1) && (sl == 0 || sl >= 32);

        if (need_prov) {
            ESP_LOGW(MESH_TAG, "Wearable: no WiFi credentials — starting BLE provisioning");
            stella_wearable_start_provisioning();
            /* 프로비저닝 완료 후 WiFi 이미 연결됨. 이후 정상 핸들러 등록. */
            ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));
            wifi_reconnect_timer_init();
            ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                                       &wifi_event_sta_disconnected_handler, NULL));
        } else {
            ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));
            wifi_reconnect_timer_init();
            ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                                       &wifi_event_sta_disconnected_handler, NULL));
            sprintf(g_hostname, "stella-%02X%02X%02X--%02X%02X%02X", MAC2STR(my_mac_factory));
            stella_set_hostname(g_hostname);
            ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
            ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));
            ESP_ERROR_CHECK(esp_wifi_start());
            {
                wifi_config_t wc = { 0 };
                if (sl == 0 || sl >= sizeof(wc.sta.ssid)) {
                    ESP_LOGE(MESH_TAG, "STA: NVS SSID empty/invalid (mesh_ap_ssid)");
                } else {
                    strncpy((char *)wc.sta.ssid, (char *)mesh_ap_ssid, sizeof(wc.sta.ssid) - 1);
                    strncpy((char *)wc.sta.password, (char *)mesh_ap_passwd, sizeof(wc.sta.password) - 1);
                    wc.sta.ssid[sizeof(wc.sta.ssid) - 1] = '\0';
                    wc.sta.password[sizeof(wc.sta.password) - 1] = '\0';
                    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
                    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
                    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
                    ESP_ERROR_CHECK(esp_wifi_connect());
                }
            }
        }
        memcpy(mesh_ap_ssid_current_connected, mesh_ap_ssid, sizeof(mesh_ap_ssid));
        ESP_LOGW(MESH_TAG, "plain STA (no mesh), ssid=%s", mesh_ap_ssid);
    }
#if 0  /* ── 메시 초기화 블록 비활성화: 모든 디바이스가 plain STA 사용 ── */
    else {
    /*  crete network interfaces for mesh (only station instance saved for further manipulation, soft AP instance ignored */
    ESP_ERROR_CHECK(mesh_netifs_init(recv_cb));

    /*  wifi initialization */
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&config));

	//===========DHCP Server정보에 반영되도록 미리=======================================================================
	//여기서는 반영이 되지 않나????
	sprintf(g_hostname,"stella-%02X%02X%02X--%02X%02X%02X", MAC2STR(my_mac_factory));
	hexdump3("set hostname(1:pre)", g_hostname, sizeof(g_hostname));

//  	iotech_set_hostname(g_hostname);
	stella_set_hostname(g_hostname);
	//==================================================================================

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));

	//===========중간에 다시 espressif로 변경되어서 여기저기 넣어보고 있다. =============================================
	//여기서는 반영이 되지 않나????
	sprintf(g_hostname,"stella-%02X%02X%02X--%02X%02X%02X", MAC2STR(my_mac_factory));
	hexdump3("set hostname(1:pre)", g_hostname, sizeof(g_hostname));

	stella_set_hostname(g_hostname);
	//==================================================================================

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

	//---------------------------------------------------------------------------
	// BLE controller init before WiFi start (no radio contention)
	esp_err_t ble_ret = nimble_port_init();
	if (ble_ret != ESP_OK) {
		ESP_LOGE("BLE", "nimble_port_init failed: %d", ble_ret);
	} else {
		ESP_LOGI("BLE", "nimble_port_init OK (before WiFi start)");
	}

	// org:
	esp_err_t err_wifi;
	err_wifi = esp_wifi_start() ;
	switch(err_wifi)
	{
		case ESP_OK:
			ESP_LOGI("esp_wifi_start", "\nESP_OK\n");
			break;
		case ESP_ERR_WIFI_NOT_INIT:
			ESP_LOGE("esp_wifi_start", "\nESP_ERR_WIFI_NOT_INIT\n");
			break;
		case ESP_ERR_INVALID_ARG:
			ESP_LOGE("esp_wifi_start", "\nESP_ERR_INVALID_ARG\n");
			break;
		case ESP_ERR_NO_MEM:
			ESP_LOGE("esp_wifi_start", "\nESP_ERR_NO_MEM\n");
			break;
		case ESP_ERR_WIFI_CONN:
			ESP_LOGE("esp_wifi_start", "\nESP_ERR_WIFI_CONN\n");
			break;
		case ESP_FAIL:
			ESP_LOGE("esp_wifi_start", "\nESP_FAIL\n");
			break;
	}
	//---------------------------------------------------------------------------
    /*  mesh initialization */
    ESP_ERROR_CHECK(esp_mesh_init());
    
//  	//shcho ------------------------------------------------------

    ESP_ERROR_CHECK(esp_event_handler_register(MESH_EVENT, ESP_EVENT_ANY_ID, &mesh_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mesh_set_max_layer(CONFIG_MESH_MAX_LAYER));
    ESP_ERROR_CHECK(esp_mesh_set_vote_percentage(1));
    ESP_ERROR_CHECK(esp_mesh_set_ap_assoc_expire(60));
    mesh_cfg_t cfg = MESH_INIT_CONFIG_DEFAULT();
    /* mesh ID */
    memcpy((uint8_t *) &cfg.mesh_id, MESH_ID, 6);
    /* router */
    cfg.channel = CONFIG_MESH_CHANNEL;

//      cfg.router.ssid_len = strlen(CONFIG_MESH_ROUTER_SSID);
//      memcpy((uint8_t *) &cfg.router.ssid, CONFIG_MESH_ROUTER_SSID, cfg.router.ssid_len);
//      memcpy((uint8_t *) &cfg.router.password, CONFIG_MESH_ROUTER_PASSWD, strlen(CONFIG_MESH_ROUTER_PASSWD));

	//=========================================================================================
	char buf[10];
	memset(buf, 0, sizeof(buf));
	err_wifi = iotech_get_nvs_str((uint8_t*)"count_no_parent", (uint8_t*)buf); 
	hexdump3("count_no_parent", buf, sizeof(buf));
	if( strlen(buf) == 0 )
	{
		iotech_set_nvs_str((uint8_t*)"count_no_parent", (uint8_t*)"0");
	}
	else
	{
		count_no_parent=atoi(buf);
//  		if( count_no_parent >= 2 ) //너무 짧나?
		if( count_no_parent >= 5 ) 
		{
			#if 0
			ESP_LOGE("SSID/PASSWD", " ");
			ESP_LOGE("SSID/PASSWD", " set to default");
			ESP_LOGE("SSID/PASSWD", " ");
			memset(mesh_ap_ssid,  0,sizeof(mesh_ap_ssid)   );
			memset(mesh_ap_passwd,0,sizeof(mesh_ap_passwd) );
			sprintf((char*)mesh_ap_ssid,   "iotech2-x"  );
			sprintf((char*)mesh_ap_passwd, "iotech3000" );
			#else
		ESP_LOGE("SSID/PASSWD", " ");
		ESP_LOGE("SSID/PASSWD", " select best RSSI(IoTech-Router-...)");
		ESP_LOGE("SSID/PASSWD", " ");
		char scan_ssid[32];
		memset(scan_ssid, 0, sizeof(scan_ssid));
		wifi_scan_best_rssi_among_Iotech_Router(scan_ssid);
		if( strlen(scan_ssid) >= 4 )
		{
			/* IoTech-Router-... AP 찾음: 해당 SSID로 업데이트 */
			memset(mesh_ap_ssid,  0, sizeof(mesh_ap_ssid));
			memset(mesh_ap_passwd, 0, sizeof(mesh_ap_passwd));
			memcpy(mesh_ap_ssid, scan_ssid, strlen(scan_ssid));
			sprintf((char*)mesh_ap_passwd, MESH_AP_PASSWD_DEFAULT);
			nvs_set_mesh_ap_ssid_passwd(mesh_ap_ssid, mesh_ap_passwd);
			nvs_get_mesh_ap_ssid_passwd(mesh_ap_ssid, mesh_ap_passwd);
			ESP_LOGW("SSID/PASSWD", "Updated to IoTech AP: %s", mesh_ap_ssid);
		}
		else
		{
			/* IoTech-Router-... AP 없음: NVS 기존값 유지 */
			ESP_LOGE("SSID/PASSWD", "No IoTech AP found, keep existing NVS SSID/passwd");
			nvs_get_mesh_ap_ssid_passwd(mesh_ap_ssid, mesh_ap_passwd);
			ESP_LOGW("SSID/PASSWD", "Keeping existing SSID: %s", mesh_ap_ssid);
		}
			#endif
	}
	}
	//=========================================================================================

	memcpy(mesh_ap_ssid_current_connected, mesh_ap_ssid, sizeof(mesh_ap_ssid));
    cfg.router.ssid_len = strlen((char *)mesh_ap_ssid);
    memcpy((uint8_t *) &cfg.router.ssid, mesh_ap_ssid, cfg.router.ssid_len);
    memcpy((uint8_t *) &cfg.router.password, mesh_ap_passwd, strlen((char *)mesh_ap_passwd));
    /* mesh softAP */
    ESP_ERROR_CHECK(esp_mesh_set_ap_authmode(CONFIG_MESH_AP_AUTHMODE));
    cfg.mesh_ap.max_connection = CONFIG_MESH_AP_CONNECTIONS;
    cfg.mesh_ap.nonmesh_max_connection = CONFIG_MESH_NON_MESH_AP_CONNECTIONS;
//      memcpy((uint8_t *) &cfg.mesh_ap.password, CONFIG_MESH_AP_PASSWD, strlen(CONFIG_MESH_AP_PASSWD));
    memcpy((uint8_t *) &cfg.mesh_ap.password, (char *)mesh_ap_passwd, strlen((char *)mesh_ap_passwd));
    ESP_ERROR_CHECK(esp_mesh_set_config(&cfg));



//      ESP_ERROR_CHECK(esp_read_mac(my_mac_factory, ESP_MAC_BASE ));
//  	hexdump3("esp_read_mac(ESP_MAC_BASE)", my_mac_factory, sizeof(my_mac_factory));

    hexdump3("esp_read_mac(ESP_MAC_EFUSE_FACTORY)", my_mac_factory, sizeof(my_mac_factory));

	// unsupported
//  //      ESP_ERROR_CHECK(esp_read_mac(my_mac_factory, ESP_MAC_EFUSE_CUSTOM ));
//  //  	hexdump3("esp_read_mac(ESP_MAC_EFUSE_CUSTOM)", my_mac_factory, sizeof(my_mac_factory));
	//===============================================================================


	//shcho add
    ESP_ERROR_CHECK(esp_mesh_send_block_time(5000));

    /* mesh start */
    ESP_ERROR_CHECK(esp_mesh_start());
    mesh_apply_wifi_stability_settings();
    ESP_LOGI(MESH_TAG, "mesh starts successfully, heap:%" PRId32 ", %s",  esp_get_free_heap_size(),
             esp_mesh_is_root_fixed() ? "root fixed" : "root not fixed");
    } /* else !wearable */
#endif  /* ── 메시 초기화 블록 비활성화 끝 ── */

    /* WiFi PS는 BLE 공존을 위해 MIN_MODEM 유지 (plain STA 블록에서 이미 설정됨).
     * OTA 시에만 mqtt_app_stop() 후 WIFI_PS_NONE으로 전환. */

//  //  //  	tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_AP, WIFI_MDNS_HOSTNAME);
//  //  	esp_netif_create_default_wifi_sta();
//  //  	esp_netif_set_hostname(netif, "ESP32_Tutorials");

	//여기서는 반영이 되지 않나????
	esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
	sprintf(g_hostname,"stella-%02X%02X%02X--%02X%02X%02X", MAC2STR(my_mac_factory));
	hexdump3("set hostname", g_hostname, sizeof(g_hostname));
	err = esp_netif_set_hostname(sta_netif, g_hostname);

	
//      xTaskCreate(&simple_ota_example_task, "ota_example_task", 8192, NULL, 5, NULL);
//  // shcho change:
//   	char *ota_url=CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL;

//  //      xTaskCreate(&simple_ota_example_task, "ota_example_task", 8192, ota_url, 5, NULL);
//      xTaskCreate(&simple_ota_example_task, "ota_example_task", 8192+1024, ota_url, 5, NULL);
    
    //xTaskCreate(make_console, "make_console", 8192, 0, 3, NULL); 
    xTaskCreate(&simple_ota_example_task, "ota_example_task", 8192 + 1024, NULL, 7, NULL);
	


#endif   //#if (TCP_SERVER_TEST_ONLY == 0)



#if (TCP_SERVER_TEST_ONLY == 1)
	app_main_tcp_server();
#endif

// add adc task : 2024.06.20 16:25
//  //  	    xTaskCreate(app_main_continuous_read_adc, "app_main_continuous_read_adc", 4096, (void*)AF_INET, 5, NULL); .//OTA 시 : Error --> reboot

// continuous_adc를 task로 만들면 죽는일이 많아서 make_console을 task로 만듬
#if 1

	xTaskCreate(app_main_sntp, "sntp_task", 2048+1024, 0, 0, NULL);

	#if (TCP_SERVER_TEST_ONLY == 0)
		// shcho add console :2024_0607
	
//         extern AcalRegs  acalVals;
//         if((err  =  iotech_get_nvs_str((uint8_t *)"meter", (uint8_t*)num)) == ESP_OK)
//         {
//             iotech_get_nvs_str((uint8_t*)"aicc",   (uint8_t*)aiccValue);
//             iotech_get_nvs_str((uint8_t*)"avcc",   (uint8_t*)avccValue);
//             iotech_get_nvs_str((uint8_t*)"relay_con",   (uint8_t*)str_relay);
//             ReadConstant();
//          }
//         else 
//         {
//             iotech_set_nvs_str((uint8_t*)"meter",  (uint8_t*)num);           
//             iotech_set_nvs_str((uint8_t*)"aicc",   (uint8_t*)aiccValue);
//             iotech_set_nvs_str((uint8_t*)"avcc",   (uint8_t*)avccValue);
//             Write_CAL_IRMS_CC();
//             Write_CAL_VRMS_CC();
//             Write_CAL_ENERGY_CC();
//             Write_CAL_POWER_CC();
//  
//             //Write_Relay(); //여기서는 필요없는데 : 2024.10.16(by shcho)
//         }
//  
//          acalVals.AICC = strtof(aiccValue, NULL); 
//          acalVals.AVCC = strtof(avccValue, NULL); 
//          ESP_LOGI(MESH_TAG, "aicc: %f", acalVals.AICC);
//          ESP_LOGI(MESH_TAG, "avcc: %f", acalVals.AVCC); 
//          ESP_LOGI(MESH_TAG, "relay_con: %s", str_relay); 
//  
//     
//         if((err =  iotech_get_nvs_str((uint8_t *)"rated_current",(uint8_t *)str_rated_current))== ESP_OK)
//         {
//                 iotech_get_nvs_str((uint8_t*)"rated_voltage", (uint8_t*)str_rated_voltage); 
//                
//  			   iotech_get_nvs_str((uint8_t*)"swell_voltage", (uint8_t*)str_swell_voltage);
//                 ESP_LOGI(MESH_TAG, "swell_voltage: %s",  str_swell_voltage);
//                 swell_voltage= atoi(str_swell_voltage);
//  
//  
//                 iotech_get_nvs_str((uint8_t*)"dip_voltage",  (uint8_t*)str_dip_voltage);
//  			   ESP_LOGI(MESH_TAG, "dip_voltage: %s",  str_dip_voltage); 
//                 dip_voltage =  atoi(str_dip_voltage);
//  
//       		   iotech_get_nvs_str((uint8_t*)"rated_freq",   (uint8_t*)str_rated_freq);
//                 ESP_LOGI(MESH_TAG, "rated_freq: %s", str_rated_freq); 
//                 rated_freq=  atoi(str_rated_freq);  
//  
//  
//  
//  			   iotech_get_nvs_str((uint8_t*)"over_current",  (uint8_t*)str_over_current);
//                 iotech_get_nvs_str((uint8_t*)"warning_duration",  (uint8_t*)str_warning_duration);
//         }
//         else 
//         {
//                 Write_Rated_Current();
//                 Write_Rated_Voltage();
//                 Write_Swell_Voltage();
//  			   Write_Dip_Voltage();
//  			   Write_Rated_Freq();
//                 Write_Over_Current();
//                 Write_Warning_Duration();
//         }
//      
//  
//  
//          ESP_LOGI(MESH_TAG,  "rated_current: %s",        str_rated_current);
//          ESP_LOGI(MESH_TAG,  "swell_voltage: %s",        str_swell_voltage);
//          ESP_LOGI(MESH_TAG,  "dip_voltage: %s",          str_dip_voltage);
//          ESP_LOGI(MESH_TAG,  "rated_freq: %s",           str_rated_freq);                                        
//  		
//  		ESP_LOGI(MESH_TAG,  "over_current: %s",         str_over_current);
//          ESP_LOGI(MESH_TAG,  "warning_duration: %s",     str_warning_duration); 
//  
//          ESP_LOGI(MESH_TAG,  "spi_init");
//  
//           // 큐 생성 (큐 길이: 10, 요소 크기: 포인터 크기)
//          txQue = xQueueCreate(10, sizeof(tx_data_t *));
//  
//          spi_init();
//          gpio_init();
//          irq_init();
//          input_status(); 
//        
//  
//          
//        
//  //  //          //printf("ade9153a_spi_init\n");
//  //  //          ESP_LOGW(MESH_TAG, "ade9153a_spi_init");
//  //  //          ade9153a_spi_init(); 
//  //  
//  //  //     		thread:check_ADE9053a 
//  //  //  		     --> ade8153a_spi_init() , thread:app_measure 
//  //          //xTaskCreate(make_console, "make_console", 8192, 0, 3, NULL);  
//  //          xTaskCreate(check_ADE9053a,  "check_task", 2048+1024, 0,  0, NULL);
//          
//          //my_mac_factory[5]
//  
//          // MAC 주소의 마지막 바이트를 사용하여 랜덤 시드 설정
//          srand(my_mac_factory[5]);
//           // 0 ~ 59초 사이의 랜덤 지연 시간 생성
//          int delay_seconds = rand() % 60;
//          ESP_LOGI(MESH_TAG, "Random Delay Seconds: %d", delay_seconds);
//          // 지정된 지연 시간 대기
//          vTaskDelay(pdMS_TO_TICKS(delay_seconds * 1000));
//   
//         timer_handle = xTimerCreate(
//  	        "MyTimer",            // 타이머 이름
//  	        pdMS_TO_TICKS(1000),  // 타이머 주기 (1초)
//  	        pdTRUE,               // 타이머 자동 반복
//  	        (void*)0,             // 타이머 ID (필요 시 사용)
//  	        timer_callback        // 타이머 콜백 함수
//         );
//  
//          if(timer_handle == NULL) {
//              ESP_LOGE(MESH_TAG, "Failed to create timer");
//          } else {
//          // 타이머 시작
//              if (xTimerStart(timer_handle, 0) != pdPASS) {
//                  ESP_LOGE(MESH_TAG, "Failed to start timer");
//              }
//          }
//  
//    //    make_console();      
	#endif
#else
//  //  	xTaskCreate(make_consoleTask, "console_task", 4096, 0, 5, &xHandle_adc);
	xTaskCreate(make_consoleTask, "console_task", 4096, 0, 5, NULL);
//  	xTaskCreate(make_consoleTask, "console_task", 2048, 0, 5, NULL);

	app_main_continuous_read_adc(NULL);
//  //  	xTaskCreate(app_main_continuous_read_adc, "adc_task", 4096, 0, 5, NULL);
//  	xTaskCreate(app_main_continuous_read_adc, "adc_task", 4096+2048, 0, 5, NULL);
#endif
//  	make_consoleTask(NULL);

	app_main_stella();
}



void check_ADE9053a(void)
{
	static int count_enter=0;
    extern  Temperature tempVal;
    ESP_LOGW("check_ADE9053a: ","\n\n\t\t check_ADE9053a(%d)\n", count_enter++);
    for(;;)
    {
		#if 0 // org
		//    ReadTemperature(&tempVal);
		ReadTemperature2(&tempVal);
		ESP_LOGE("check Temp", "Temp=%4.2f", tempVal.TemperatureVal);
		ESP_LOGE("check Temp", "Temp=%4.2f", tempVal.TemperatureVal);
		
		if((tempVal.TemperatureVal>-40)&(tempVal.TemperatureVal<81)) break;
		else  ade9153a_spi_init(); 
		#else

		//    ReadTemperature(&tempVal);
		ade9153a_spi_init(); 

		ReadTemperature2(&tempVal);
//  		ESP_LOGE("check Temp", "Temp=%4.2f", tempVal.TemperatureVal);
		
		if((tempVal.TemperatureVal>-40)&(tempVal.TemperatureVal<81)) 
		{
			ESP_LOGW("check_ADE9053a: "," // Temp는 정상인데 읽지 못하는 경우를 다시한번 Check");
			uint32_t ret, ret1;
			ret   =  read_ade9153a_wav();
			vTaskDelay(pdMS_TO_TICKS(1));      //Hold low for 1 milliseconds  
			ret1   =  read_ade9153a_wav();
			ESP_LOGW("check_ADE9053a", "1.AV_WAV:  %08lX   2.AV_WAV:  %08lX: 달라야 함", ret, ret1);

			if( ret != ret1 )
			{
				break;
			}
			else
			{
				ESP_LOGW("check_ADE9053a", "1. AV_WAV : same : ADE9153A is under malfunction--> hw_reset");
				ESP_LOGW("check_ADE9053a", "2. AV_WAV : same : ADE9153A is under malfunction--> hw_reset");
				ESP_LOGW("check_ADE9053a", "3. AV_WAV : same : ADE9153A is under malfunction--> hw_reset");
				ESP_LOGW("check_ADE9053a", "4. AV_WAV : same : ADE9153A is under malfunction--> hw_reset");
			}
		}
		ade9153a_reset();

		#endif
		
		vTaskDelay(pdMS_TO_TICKS(1000));      //Hold low for 500 milliseconds  
	}

	xTaskCreate(app_measure,  "measure_task", 8192, 0,  0, &xTaskHandle_spi);
	vTaskDelete(NULL); // Delete this task after completion
}






