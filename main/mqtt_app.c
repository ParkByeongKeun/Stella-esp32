/* Mesh IP Internal Networking Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_netif.h"
/* TLS 미사용 - ca_cert 임베딩 비활성화 */
// extern const uint8_t ca_cert_pem_start[] asm("_binary_ca_cert_pem_start");
// extern const uint8_t ca_cert_pem_end[]   asm("_binary_ca_cert_pem_end");
#include "esp_heap_caps.h"
#include "freertos/idf_additions.h"
#include "sdkconfig.h"
//shcho add
#include "esp_mac.h"
#include "esp_mesh.h"

#include "mqtt_client.h"
#include "cJSON.h"
#include "iotech_global.h"
#include "json_struct.h"
#include "meter_app.h"
#include "spifss.h"
#include "esp_timer.h"



#define  STR_MATCH	(0)

/* 재연결 시 socket() 실패(ENOMEM) 시 폭주 완화 — 기본 5초는 너무 빠름
 * 60초 간격: ENOMEM 상태에서 LWIP TIME_WAIT PCB 회수(2*MSL=30s)·heap 단편화 복구 시간을 확보.
 * 이로써 재연결 실패 시 socket 재할당을 1분에 1회로 제한 → 내부 SRAM 폭주 방지. */
#ifndef MQTT_RECONNECT_TIMEOUT_MS
#define MQTT_RECONNECT_TIMEOUT_MS  60000
#endif

#ifndef CONFIG_STELLA_MQTT_KEEPALIVE_SEC
#define STELLA_MQTT_KEEPALIVE_SEC  120
#else
#define STELLA_MQTT_KEEPALIVE_SEC  CONFIG_STELLA_MQTT_KEEPALIVE_SEC
#endif
#ifndef CONFIG_STELLA_MQTT_OUTBOX_LIMIT_KB
#define STELLA_MQTT_OUTBOX_LIMIT_B  (8 * 1024)
#else
#define STELLA_MQTT_OUTBOX_LIMIT_B  (CONFIG_STELLA_MQTT_OUTBOX_LIMIT_KB * 1024)
#endif
/* status publish 스킵: 이 값 미만이면 내부 힙으로 publish 실패(악순환)만 유발 */
#ifndef STELLA_MQTT_PUBLISH_MIN_INTERNAL_LARGEST
#define STELLA_MQTT_PUBLISH_MIN_INTERNAL_LARGEST  384u
#endif

/** NVS에 mqtt_broker_uri 없을 때 사용 */
#define MQTT_BROKER_URI_DEFAULT "mqtt://ijoon.iptime.org:26323"

extern int16_t count_no_parent;

static esp_mqtt_client_handle_t s_client = NULL;
extern uint8_t* mesh_netif_get_station_mac(void);
extern uint8_t* mesh_netif_get_rootnode_mac(void);
extern esp_err_t Write_Relay(char* str);
extern int relay_control_all_off(void);

extern int zx_gpio_irq_remove();
extern int zx_gpio_irq_add();


extern int flag_ota_run ; 
extern volatile int stella_sensors_paused_for_ota;
extern uint8_t my_mac_factory[20] ; 
extern uint8_t ota_url[256] ; 

extern char  str_cal_irms_cc[10];
extern char  str_cal_vrms_cc[10];
extern char  str_cal_energy_cc[10];
extern char  str_cal_power_cc[10]; 

extern  char   device_id[];
extern  char   fw_version[];
extern  char   avccValue[10];
extern  char   aiccValue[10];

extern uint8_t mesh_ap_ssid[];
extern uint8_t mesh_ap_passwd[];
/* mesh_main.c 정의: mesh_ap_ssid[32], mesh_ap_passwd[64] — extern 배열에 sizeof 사용 불가 */
#define MESH_AP_SSID_CAP   32
#define MESH_AP_PASSWD_CAP 64

extern  float  CAL_IRMS_CC;
extern  float  CAL_VRMS_CC;
extern  float  CAL_ENERGY_CC;
extern  float  CAL_POWER_CC; 


extern  meter write_meter;
extern  meter last_meter;

extern  int   prev_relay_state;
volatile int flag_relay_control = 0 ; 

extern int   relay_latch_flag;   


extern  int    noti_period;
char  str_noti_period[5]="15";

extern  QueueHandle_t txQue;

extern int	flag_AV_WAVE_work ;

extern esp_err_t iotech_get_nvs_str(uint8_t *key, uint8_t *value);
extern esp_err_t iotech_set_nvs_str(uint8_t *key, uint8_t *value);
char   mqtt_broker_uri[128];
char   str_freq_margin[10]="0";
float  freq_margin=0;


//char   str_over_voltage_margin[];
extern   uint16_t  swell_voltage;

//extern  char   str_under_voltage_margin[];
//extern  int    under_voltage_margin;
extern   uint16_t  dip_voltage;


//extern  char   str_over_current_margin[];
extern  float  over_current;

//extern  char   str_rated_current[];
extern  int    rated_current;

extern  int    rated_voltage;

extern  uint16_t    rated_freq;

//extern  char   str_warning_duration[];
extern  int    warning_duration;


char    str_rated_current[6]="20";
char    str_rated_voltage[6]="220";  
//char  str_over_voltage_margin[6]= "20";
char    str_swell_voltage[6] = "242";
char    str_dip_voltage[6]=    "198";


//char  str_under_voltage_margin[6]= "20";
char  str_over_current[6]= "2";
char  str_warning_duration[6]=  "30";

//org:  char  str_relay[6] = "on";      
//shcho
char  str_relay[6] = "none";      

char  str_total_energy[6];
char  str_constant[6];
char  str_total_energy_datetime[20];
char  str_rated_freq[6]  =  "60";

int   flag_mqtt_connect = 0;
int   delay2 =  1800;
//int   delay2 =  180;


static  int  idx;

static const char *TAG = "mqqt";

/* JSON/큐용 큰 블록은 PSRAM 우선 — CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL 와 무관하게
 * 내부 SRAM 단편화(= internal_largest 붕괴 → socket ENOMEM)를 줄임. */
static void *mqtt_malloc_prefer_spiram(size_t n)
{
    void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) {
        p = malloc(n);
    }
    return p;
}

static void mqtt_free_any(void *p)
{
    if (p) {
        heap_caps_free(p);
    }
}

static void mqtt_log_heap_diag(const char *where)
{
    size_t total_free         = esp_get_free_heap_size();
    size_t internal_free    = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t internal_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_LOGD(TAG, "heap [%s] total=%u internal_free=%u internal_largest=%u",
             where,
             (unsigned)total_free,
             (unsigned)internal_free,
             (unsigned)internal_largest);
    /* 기본 로그 레벨에서도 TLS/socket 실패 직전만 한 줄 */
    if (internal_largest < 2048u || internal_free < 4096u) {
        ESP_LOGW(TAG, "heap LOW [%s] if=%u il=%u (TLS needs contiguous internal)",
                 where, (unsigned)internal_free, (unsigned)internal_largest);
    }
}

#define STELLA_TOPIC_LEN  128
/* MQTT 클라이언트 내부 태스크 스택: 6144→4096 (연속 내부 SRAM 2KB 확보).
 * esp-mqtt 라이브러리 기본(평문 MQTT) 경로에서는 4KB로 충분하며,
 * OTA HTTPS는 별도 태스크를 쓰므로 영향 없음. */
#ifndef MQTT_CLIENT_TASK_STACK
#define MQTT_CLIENT_TASK_STACK  4096
#endif

/* MQTT 메시지 입·출력 버퍼 (기본 1024/1024).
 * OTA 명령 JSON은 `total_data_len`(분할 페이로드) 기반으로 별도 재조립하므로
 * 1024 바이트면 상태/설정/센서 단일 메시지 모두 수용 가능.
 * 너무 크게 잡으면 esp_mqtt_client_init()에서 내부 SRAM 연속 블록 실패 가능 → 축소 유지. */
#ifndef MQTT_CLIENT_BUFFER_SIZE
#define MQTT_CLIENT_BUFFER_SIZE      1024
#endif
#ifndef MQTT_CLIENT_OUT_BUFFER_SIZE
#define MQTT_CLIENT_OUT_BUFFER_SIZE  1024
#endif

/* OTA 명령 JSON: 긴 페이로드가 여러 DATA로 쪼개질 때 누적 후 파싱 */
#define OTA_MQTT_JSON_MAX 512
static char s_ota_mqtt_json[OTA_MQTT_JSON_MAX];

static char *stella_status_topic        = NULL;
static char *stella_ota_cmd_topic       = NULL;
char *stella_ota_progress_topic  = NULL;
char *stella_ota_result_topic    = NULL;

#define STELLA_STATUS_INTERVAL_MS  (10 * 60 * 1000)
#define STELLA_STATUS_FIRST_MS     (30 * 1000)   /* 재연결 직후 빠른 안전망(30초) */
static esp_timer_handle_t s_status_timer = NULL;
static char s_ui_version[32] = {0};

/* ── online 재발행 보호 상태 ─────────────────────────────────────────────
 * 이유: CONNECTED 직후 esp_mqtt_client_publish 가 순간적으로 -1 을 반환하거나
 *       QoS1 PUBACK 이 유실되면 브로커의 retained 메시지가 "offline"(LWT)
 *       상태로 남아 대시보드가 계속 offline 으로 표시된다.
 *       PUBACK 이 올 때까지 짧은 간격으로 재시도해서 반드시 "online"이
 *       retained 에 올라오도록 보장한다. */
#define ONLINE_RETRY_INTERVAL_MS   3000
#define ONLINE_RETRY_MAX           8        /* 최대 ~24초 동안 시도 */
static esp_timer_handle_t s_online_retry_timer = NULL;
static int                s_online_last_msg_id  = -1;
static volatile bool      s_online_acked        = false;
static int                s_online_retry_count  = 0;

void mqtt_set_ui_version(const char *ver)
{
    if (ver) {
        strncpy(s_ui_version, ver, sizeof(s_ui_version) - 1);
        s_ui_version[sizeof(s_ui_version) - 1] = '\0';
    }
}

/* 실제 publish 를 수행하고 msg_id (>=0) 또는 -1 을 반환 */
static int stella_publish_status_once(void)
{
    if (!s_client || !stella_status_topic || strlen(stella_status_topic) == 0) {
        return -1;
    }
    size_t il = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (il < STELLA_MQTT_PUBLISH_MIN_INTERNAL_LARGEST) {
        ESP_LOGD(TAG, "status publish skip (internal_largest=%u)", (unsigned)il);
        return -1;
    }
    char status_json[256];
    if (s_ui_version[0] != '\0') {
        snprintf(status_json, sizeof(status_json),
                 "{\"id\":\"%s\",\"status\":\"online\",\"fw_version\":\"%s\",\"ui_version\":\"%s\"}",
                 device_id, fw_version, s_ui_version);
    } else {
        snprintf(status_json, sizeof(status_json),
                 "{\"id\":\"%s\",\"status\":\"online\",\"fw_version\":\"%s\"}",
                 device_id, fw_version);
    }
    int slen = (int)strlen(status_json);
    return esp_mqtt_client_publish(s_client, stella_status_topic,
                                   status_json, slen, 1, 1);
}

static void online_retry_cb(void *arg);

static void online_retry_schedule(uint32_t delay_ms)
{
    if (!s_online_retry_timer) {
        const esp_timer_create_args_t args = {
            .callback = online_retry_cb,
            .name     = "online_retry",
        };
        esp_timer_create(&args, &s_online_retry_timer);
    }
    esp_timer_stop(s_online_retry_timer);
    esp_timer_start_once(s_online_retry_timer, (uint64_t)delay_ms * 1000);
}

static void online_retry_cancel(void)
{
    if (s_online_retry_timer) {
        esp_timer_stop(s_online_retry_timer);
    }
    s_online_retry_count = 0;
}

static void online_retry_cb(void *arg)
{
    (void)arg;
    if (!flag_mqtt_connect) {
        ESP_LOGD(TAG, "online_retry: not connected -> skip (wait next CONNECTED)");
        return;
    }
    if (s_online_acked) {
        ESP_LOGD(TAG, "online_retry: already acked -> stop");
        return;
    }
    if (s_online_retry_count >= ONLINE_RETRY_MAX) {
        ESP_LOGE(TAG, "online_retry: max retries reached (%d); rely on periodic",
                 s_online_retry_count);
        return;
    }
    s_online_retry_count++;
    int msg_id = stella_publish_status_once();
    if (msg_id >= 0) {
        s_online_last_msg_id = msg_id;
        ESP_LOGD(TAG, "online_retry #%d re-published msg_id=%d",
                 s_online_retry_count, msg_id);
    } else {
        ESP_LOGD(TAG, "online_retry #%d publish returned %d",
                 s_online_retry_count, msg_id);
    }
    online_retry_schedule(ONLINE_RETRY_INTERVAL_MS);
}

static void stella_publish_status(void)
{
    int msg_id = stella_publish_status_once();
    if (msg_id < 0) {
        ESP_LOGE(TAG, "stella status publish FAILED (ret=%d) -> retry in %d ms",
                 msg_id, ONLINE_RETRY_INTERVAL_MS);
        s_online_acked       = false;
        s_online_retry_count = 0;
        online_retry_schedule(ONLINE_RETRY_INTERVAL_MS);
    } else {
        ESP_LOGD(TAG, "stella status published msg_id=%d (await PUBACK)", msg_id);
        s_online_last_msg_id = msg_id;
        s_online_acked       = false;
        s_online_retry_count = 0;
        /* PUBACK 유실 대비 안전망: PUBACK 이 오면 즉시 취소됨 */
        online_retry_schedule(ONLINE_RETRY_INTERVAL_MS);
    }
}

/* 매 CONNECTED 마다 초기화됨: true 면 이미 빠른 first-fire 발화를 소비했음 */
static bool s_status_first_fire_done = false;

static void status_timer_cb(void *arg)
{
    (void)arg;
    if (flag_mqtt_connect) {
        if (!s_status_first_fire_done) {
            ESP_LOGD(TAG, "status first-fire publish (after %d ms of CONNECTED)",
                     STELLA_STATUS_FIRST_MS);
        } else {
            ESP_LOGD(TAG, "periodic status publish (every %d min)",
                     STELLA_STATUS_INTERVAL_MS / 60000);
            /* 주기적 heap 진단: 기본 레벨에선 LOGD */
            mqtt_log_heap_diag("PERIODIC");
        }
        stella_publish_status();
    }
    if (!s_status_first_fire_done) {
        /* 최초 빠른 발화 후 정기 주기로 전환 */
        s_status_first_fire_done = true;
        if (s_status_timer) {
            esp_timer_stop(s_status_timer);
            esp_timer_start_periodic(s_status_timer,
                                     (uint64_t)STELLA_STATUS_INTERVAL_MS * 1000);
        }
    }
}

static void status_timer_start(void)
{
    if (!s_status_timer) {
        const esp_timer_create_args_t args = {
            .callback = status_timer_cb,
            .name = "status_pub",
        };
        esp_timer_create(&args, &s_status_timer);
    }
    esp_timer_stop(s_status_timer);
    /* 매 CONNECTED 마다 first-fire 상태 초기화 */
    s_status_first_fire_done = false;
    /* 먼저 짧은 one-shot 으로 빠르게 발화시키고, 그 다음 주기 모드로 전환 */
    esp_timer_start_once(s_status_timer, (uint64_t)STELLA_STATUS_FIRST_MS * 1000);
}

static void status_timer_stop(void)
{
    if (s_status_timer) {
        esp_timer_stop(s_status_timer);
    }
    online_retry_cancel();
    s_online_acked = false;
    s_online_last_msg_id = -1;
}

void get_total_energy(void);

void handle_config_json(const char *json_data, int json_data_len) {
     
    cJSON *root = cJSON_ParseWithLength(json_data, json_data_len);
    if (root == NULL) {
        ESP_LOGE(TAG, "Invalid JSON data");
        return;
    }
    
    update_flags_t flags = {0}; 

    cJSON *avcc = cJSON_GetObjectItem(root, "avcc");
    if (cJSON_IsString(avcc)) {
        flags.avcc  = 1; 
        if(strcmp(avccValue,avcc->valuestring)!=0)
        {
            strncpy(avccValue, avcc->valuestring, sizeof(avccValue) - 1);
            avccValue[sizeof(avccValue) - 1] = '\0';
            iotech_set_nvs_str((uint8_t*)"avcc", (uint8_t *)avccValue);
            ESP_LOGD(TAG, "Set avccValue to %s", avccValue);
           
        }
    }

    cJSON *aicc = cJSON_GetObjectItem(root, "aicc");
    if (cJSON_IsString(aicc)) {
       flags.aicc = 1;
       if(strcmp(aiccValue, aicc->valuestring)!=0)
       { 
            strncpy(aiccValue, aicc->valuestring, sizeof(aiccValue) - 1);
            aiccValue[sizeof(aiccValue) - 1] = '\0';
            iotech_set_nvs_str((uint8_t*)"aicc", (uint8_t *)aiccValue);
            ESP_LOGD(TAG, "Set aiccValue to %s", aiccValue);            
       }
    }

    cJSON *cal_irms_cc = cJSON_GetObjectItem(root, "cal_irms_cc");
    if (cJSON_IsString(cal_irms_cc)) {
        flags.cal_irms_cc =1;
        if(strcmp(str_cal_irms_cc,cal_irms_cc->valuestring)!=0)
        {
            strncpy(str_cal_irms_cc, cal_irms_cc->valuestring, sizeof(str_cal_irms_cc) - 1);
            str_cal_irms_cc[sizeof(str_cal_irms_cc) - 1] = '\0';
            iotech_set_nvs_str((uint8_t*)"cal_irms_cc", (uint8_t *)str_cal_irms_cc);
            CAL_IRMS_CC = strtof(str_cal_irms_cc, NULL);
            ESP_LOGD(TAG, "Set CAL_IRMS_CC to %f", CAL_IRMS_CC);
        }     
     }

    cJSON *cal_vrms_cc = cJSON_GetObjectItem(root, "cal_vrms_cc");
    if (cJSON_IsString(cal_vrms_cc)) {
        flags.cal_vrms_cc = 1;
        if(strcmp(str_cal_vrms_cc,cal_vrms_cc->valuestring)!=0)
        {
             strncpy(str_cal_vrms_cc, cal_vrms_cc->valuestring, sizeof(str_cal_vrms_cc) - 1);
             str_cal_vrms_cc[sizeof(str_cal_vrms_cc) - 1] = '\0';
             iotech_set_nvs_str((uint8_t*)"cal_vrms_cc", (uint8_t*)str_cal_vrms_cc);
             CAL_VRMS_CC = strtof(str_cal_vrms_cc, NULL);
             ESP_LOGD(TAG, "Set CAL_VRMS_CC to %f", CAL_VRMS_CC);
         }
    }

   

    cJSON *cal_power_cc = cJSON_GetObjectItem(root, "cal_power_cc");
    if (cJSON_IsString(cal_power_cc)) {
        flags.cal_power_cc = 1;
        if(strcmp(str_cal_power_cc,cal_power_cc->valuestring)!=0)
        { 
            strncpy(str_cal_power_cc, cal_power_cc->valuestring, sizeof(str_cal_power_cc) - 1);
            str_cal_power_cc[sizeof(str_cal_power_cc) - 1] = '\0';
            iotech_set_nvs_str((uint8_t*)"cal_power_cc", (uint8_t*)str_cal_power_cc);
            CAL_POWER_CC = strtof(str_cal_power_cc, NULL);
            ESP_LOGD(TAG, "Set CAL_POWER_CC to %f", CAL_POWER_CC);           
        }
    }


    cJSON *cal_energy_cc = cJSON_GetObjectItem(root, "cal_energy_cc");
    if (cJSON_IsString(cal_energy_cc)) {
        flags.cal_energy_cc = 1;
        if(strcmp(str_cal_energy_cc,cal_energy_cc->valuestring)!=0)
        {
            strncpy(str_cal_energy_cc, cal_energy_cc->valuestring, sizeof(str_cal_energy_cc) - 1);
            str_cal_energy_cc[sizeof(str_cal_energy_cc) - 1] = '\0';
            iotech_set_nvs_str((uint8_t*)"cal_energy_cc", (uint8_t*)str_cal_energy_cc);
            CAL_ENERGY_CC = strtof(str_cal_energy_cc, NULL);
            ESP_LOGD(TAG, "Set CAL_ENERGY_CC to %f", CAL_ENERGY_CC);            
        }
    } 



   cJSON *c_noti_period  = cJSON_GetObjectItem(root,  "noti_period");
   if (cJSON_IsString(c_noti_period))  {
     //    ESP_LOGI(TAG, "str_noti_period:%s  c_noti_period:%s",  str_noti_period, c_noti_period->valuestring);    
         flags.noti_period = 1;
         if(strcmp(str_noti_period,  c_noti_period->valuestring)!=0)
         {
           strncpy(str_noti_period,  c_noti_period->valuestring, sizeof(str_noti_period)-1);
           str_noti_period[sizeof(str_noti_period) -1] = '\0';
           iotech_set_nvs_str((uint8_t*)"noti_period", (uint8_t*)str_noti_period);
           noti_period = atoi(str_noti_period);
           ESP_LOGD(TAG, "Set noti_period  to %d",  noti_period);            
         }    
   }

   cJSON  *c_mesh_ap_ssid   = cJSON_GetObjectItem(root, "mesh_ap_ssid");  
   if(cJSON_IsString(c_mesh_ap_ssid))  {
         flags.mesh_ap_ssid = 1;  
         if(strcmp((char*)mesh_ap_ssid, c_mesh_ap_ssid->valuestring)!=0)
         {
             strncpy((char*)mesh_ap_ssid,   c_mesh_ap_ssid->valuestring,  MESH_AP_SSID_CAP - 1);
             mesh_ap_ssid[MESH_AP_SSID_CAP - 1]  = '\0';
             iotech_set_nvs_str((uint8_t*)"mesh_ap_ssid",  (uint8_t*)mesh_ap_ssid);
             ESP_LOGD(TAG, "Set ap ssid:  %s", mesh_ap_ssid);  
                     
         }   
        
   }
    
   cJSON *c_mesh_ap_passwd =  cJSON_GetObjectItem(root, "mesh_ap_passwd");
   if(cJSON_IsString(c_mesh_ap_passwd))  {
         flags.mesh_ap_passwd = 1;
         if(strcmp((char*)mesh_ap_passwd, c_mesh_ap_passwd->valuestring)!=0)
         {
             strncpy((char*)mesh_ap_passwd,   c_mesh_ap_passwd->valuestring,  MESH_AP_PASSWD_CAP - 1);
             mesh_ap_passwd[MESH_AP_PASSWD_CAP - 1]  = '\0';
             iotech_set_nvs_str((uint8_t*)"mesh_ap_passwd", (uint8_t*)mesh_ap_passwd);
             ESP_LOGD(TAG, "Set ap passwd: %s", mesh_ap_passwd);
         } 
        
   }

   cJSON *c_mqtt_broker_uri  = cJSON_GetObjectItem(root, "mqtt_broker_uri");
   if(cJSON_IsString(c_mqtt_broker_uri))  {
         flags.mqtt_broker_uri  = 1; 
         if(strcmp(mqtt_broker_uri, c_mqtt_broker_uri->valuestring)!=0)
         {
            strncpy(mqtt_broker_uri, c_mqtt_broker_uri->valuestring, sizeof(mqtt_broker_uri)-1);
            mqtt_broker_uri[sizeof(mqtt_broker_uri) -1]  = '\0'; 
            iotech_set_nvs_str((uint8_t*)"mqtt_broker_uri", (uint8_t*)mqtt_broker_uri);
            ESP_LOGD(TAG, "Set mqtt broker: %s", mqtt_broker_uri);        
         }
    }

   cJSON * c_freq_margin  =  cJSON_GetObjectItem(root, "freq_margin");
   if(cJSON_IsString(c_freq_margin))  {
            flags.freq_margin  = 1;
            if(strcmp(str_freq_margin,  c_freq_margin->valuestring)!=0)
            {
               strncpy(str_freq_margin, c_freq_margin->valuestring,  sizeof(str_freq_margin)-1);              
               str_freq_margin[sizeof(str_freq_margin)-1] = '\0';
               iotech_set_nvs_str((uint8_t*)"freq_margin",  (uint8_t*)str_freq_margin);
               
               char *endptr;
               freq_margin =  strtof(str_freq_margin, &endptr);
             
                // 변환 결과 출력
                if(*endptr == '\0') {
                   ESP_LOGD(TAG, "Converted value: %f", freq_margin);
                } else {
                   ESP_LOGE(TAG, "Conversion error,  non-convertible part: %s\n", endptr);                      
                } 
            }
    }

     
  //  cJSON *  c_over_voltage_margin  =  cJSON_GetObjectItem(root, "over_voltage_margin");

    cJSON *  c_swell_voltage   =  cJSON_GetObjectItem(root, "swell_voltage"); 
   

  #if 0
    if(cJSON_IsString(c_over_voltage_margin)) {
              flags.over_voltage_margin = 1;
              if(strcmp(str_over_voltage_margin, c_over_voltage_margin->valuestring)!=0)
              {
                    strncpy(str_over_voltage_margin,  c_over_voltage_margin->valuestring,  sizeof(str_over_voltage_margin)-1);
                    str_over_voltage_margin[sizeof(str_over_voltage_margin)-1]  = '\0';
                    iotech_set_nvs_str((uint8_t*)"over_voltage_margin", (uint8_t*)str_over_voltage_margin);

                    over_voltage_margin = atoi(str_over_voltage_margin);
                 // 변환 결과 출력
              }
    }
 #else 
    if(cJSON_IsString(c_swell_voltage)) {
             flags.swell=1;
             if(strcmp(str_swell_voltage, c_swell_voltage->valuestring)!=0)
             {
                   strncpy(str_swell_voltage,  c_swell_voltage->valuestring,  sizeof(str_swell_voltage)-1);
                   str_swell_voltage[sizeof(str_swell_voltage)-1]  = '\0';
                   iotech_set_nvs_str((uint8_t*)"swell_voltage",  (uint8_t*)str_swell_voltage);          
                   swell_voltage = atoi(str_swell_voltage);
             }
    }
#endif  




    
//  cJSON *  c_under_voltage_margin  =  cJSON_GetObjectItem(root, "under_voltage_margin");
    cJSON * c_dip_voltage  = cJSON_GetObjectItem(root, "dip_voltage");
    if(cJSON_IsString(c_dip_voltage)) {
              flags.dip = 1;
              if(strcmp(str_dip_voltage, c_dip_voltage->valuestring)!=0)
              {
                    strncpy(str_dip_voltage,  c_dip_voltage->valuestring,  sizeof(str_dip_voltage)-1);
                    str_dip_voltage[sizeof(str_dip_voltage)-1]  = '\0';
                    iotech_set_nvs_str((uint8_t*)"dip_voltage", (uint8_t*)str_dip_voltage);
                    dip_voltage = atoi(str_dip_voltage);
              }
    }   

    

    cJSON *  c_over_current  =  cJSON_GetObjectItem(root, "over_current");
    if(cJSON_IsString(c_over_current)) {
              flags.over_current = 1;
              if(strcmp(str_over_current, c_over_current->valuestring)!=0)
              {
                    strncpy(str_over_current,  c_over_current->valuestring,  sizeof(str_over_current)-1);
                    str_over_current[sizeof(str_over_current)-1]  = '\0';
                    iotech_set_nvs_str((uint8_t*)"over_current", (uint8_t*)str_over_current);

                    char * endptr;                                        
                    over_current = strtof(str_over_current,  &endptr);
                 // 변환 결과 출력
                 if(*endptr == '\0') {
                       ESP_LOGD(TAG,  "Converted value: %f", over_current);               
                 } else {
                       ESP_LOGE(TAG,  "Conversion error,  non-convertible part: %s\n", endptr);
                 }
              }
    }  
   

    cJSON *  c_rated_voltage = cJSON_GetObjectItem(root,  "rated_voltage"); 
    if(cJSON_IsString(c_rated_voltage))  {
           flags.rated_voltage  =1;
           if(strcmp(str_rated_voltage, c_rated_voltage->valuestring)!=0)
           {
                 strncpy(str_rated_voltage,  c_rated_voltage->valuestring,    sizeof(str_rated_voltage)-1);           
                 str_rated_voltage[sizeof(str_rated_voltage)-1]=  '\0';  
                 iotech_set_nvs_str((uint8_t*)"rated_voltage",  (uint8_t*)str_rated_voltage);
                 rated_voltage =  atoi(str_rated_voltage);
                 ESP_LOGD(TAG,  "rated_voltage: %d",  rated_voltage); 
           }
    }

    

    cJSON *  c_rated_current  =  cJSON_GetObjectItem(root,  "rated_current");
    if(cJSON_IsString(c_rated_current))  {
              flags.rated_current  = 1;
              if(strcmp(str_rated_current,  c_rated_current->valuestring)!=0)
              {
                    strncpy(str_rated_current,  c_rated_current->valuestring,  sizeof(str_rated_current)-1);
                    str_rated_current[sizeof(str_rated_current)-1]= '\0';
                    iotech_set_nvs_str((uint8_t*)"rated_current",  (uint8_t*)str_rated_current);
                    rated_current =  atoi(str_rated_current);   
                    ESP_LOGD(TAG,  "rated_current: %d", rated_current);  
              }
    }


     cJSON * c_rated_freq =  cJSON_GetObjectItem(root,  "rated_freq");
     if(cJSON_IsString(c_rated_freq))    {
            flags.rated_freq  = 1;
            if(strcmp(str_rated_freq,  c_rated_freq->valuestring)!=0)
            {
                    strncpy(str_rated_freq,  c_rated_current->valuestring,  sizeof(str_rated_current)-1);
                    str_rated_freq[sizeof(str_rated_freq)-1]= '\0';
                    iotech_set_nvs_str((uint8_t*)"rated_freq",    (uint8_t*)str_rated_freq);
                    rated_freq  = atoi(str_rated_freq);
                    ESP_LOGD(TAG, "rated_freq: %d",   rated_freq);  
            }
     }



    cJSON *  c_warning_duration  =  cJSON_GetObjectItem(root, "warning_duration");
    if(cJSON_IsString(c_warning_duration))  {
              flags.relay  = 1;
              if(strcmp(str_warning_duration,  c_warning_duration->valuestring)!=0)
              {
                     strncpy(str_warning_duration,  c_warning_duration->valuestring, sizeof(str_warning_duration)-1);
                     str_warning_duration[sizeof(str_warning_duration)-1]  = '\0';
                     iotech_set_nvs_str((uint8_t*)"warning_duration", (uint8_t*)str_warning_duration);
                     warning_duration  =  atoi(str_warning_duration);
                     ESP_LOGD(TAG, "warning_duration: %d",  warning_duration); 
                     delay2  =   warning_duration*60;
              }
    }
  
    char  *buffer=NULL;
    int  json_len= serialize_variable_event(flags , &buffer);
    ESP_LOGD(TAG, "event len=%zu", json_len); 
    tx_data_t  *tx_data =(tx_data_t *)mqtt_malloc_prefer_spiram(sizeof(tx_data_t) + (size_t)json_len);
    tx_data->id = idx; 
    tx_data->length = json_len;
    ESP_LOGD(TAG, "event=%s",  buffer); 
    
    memcpy(&tx_data->data,buffer,json_len);
           
    if(xQueueSend(txQue, &tx_data,  pdMS_TO_TICKS(1000)) == pdPASS)  {
       ESP_LOGD(TAG,  "Send: %s",   tx_data->data);
       idx++; 
    }  else {
       ESP_LOGW(TAG,  "Failed to send data");   
    }
     cJSON_Delete(root);   
}



void handle_control_json(const char *json_data, int json_data_len) {
      
    cJSON *root = cJSON_ParseWithLength(json_data, json_data_len);
    if (root == NULL) {
        ESP_LOGE(TAG, "Invalid JSON data");
        return;
    }

    update_flags_t flags = {0}; 
     cJSON *  c_relay  =  cJSON_GetObjectItem(root, "relay");

   

	if(cJSON_IsString(c_relay))  
	{
		flags.relay  = 1;

        ESP_LOGD(TAG, "relay json: str=%s cmd=%s state=%d prev=%d",
                 str_relay, c_relay->valuestring, relay_state, prev_relay_state);

		while( flag_AV_WAVE_work != 0 )
		{
			ESP_LOGD("mqtt_shcho", "wait for flag_AV_WAVE_work == 0");
			vTaskDelay(pdMS_TO_TICKS(1000));
		}

				
//  //  //		if(strcmp(str_relay,  c_relay->valuestring) != 0)
//  //		if(strcmp(str_relay,  c_relay->valuestring) != STR_MATCH)
//		if( relay_state != prev_relay_state )
        {
	        strncpy(str_relay,  c_relay->valuestring, sizeof(str_relay)-1);
	        str_relay[sizeof(str_relay)-1] ='\0';
	        ESP_LOGD(TAG, "update str_relay to (%s)", str_relay);

	        if(   ( strcmp(str_relay, "on" ) == STR_MATCH ) 
			   || ( strcmp(str_relay, "off") == STR_MATCH ) )
			{
			    if(strcmp(str_relay, "on")==0)
			    {
				    ESP_LOGD(TAG, "relay on");
				    relay_on();
			    }
			    else if(strcmp(str_relay, "off")==0)
			    {
				    ESP_LOGD(TAG, "relay off");
				    relay_off();
			    }

//  			    Write_Relay(); // loop()에서 완전히 처리한 후에 하는 것으로 변경 


				if( flag_relay_control == 1 ) 
				{
					relay_latch_flag = 0;
					zx_gpio_irq_remove();
				}
					

				zx_gpio_irq_add();
				flag_relay_control = 1;

				relay_control_all_off();


				// Relay를 동작시키기 전에 전력량을 읽어서 보내고(Relay의 Spark로 ADE9153A가 죽는경우가 많기 때문에)
				// Relay를 동작시켜서 ADE9153A의 온도등을 Check헤서 app_measure를 다시 실행(ADE9153A를 Reset시켜야 함.)
				// SPI에서 응답이 없으니까 CPU에서는 Interrupt wdt timeout on CPU0 :Error로 Reboot홤)
				// 
				ESP_LOGD("Important", "watch metering IC / flags after relay change");
			}
			else
			{
				ESP_LOGE(TAG, "cont/relay is not [ on | off ]");
			}
		}
    }



  
    cJSON * c_total_energy  = cJSON_GetObjectItem(root, "total_energy");
    if(cJSON_IsString(c_total_energy))  {
                
                
                ESP_LOGD(TAG, "total_energy cmd len=%d", json_data_len);
                strncpy(str_total_energy,  c_total_energy->valuestring, sizeof(str_total_energy)-1); 
                if(strcmp(str_total_energy, "get")==0)
                {
                        write_meter.id   =  last_meter.id;
                        write_meter.wh   =  last_meter.wh;
                        write_meter.varh =  last_meter.varh;
                        write_meter.vah  =  last_meter.vah;
                        strcpy(write_meter.datetime,  last_meter.datetime);
                        flags.total_energy=1;
                } 
                else if(strcmp(str_total_energy, "delete")==0) 
                {
                      int rc= delete_records();
                       if(rc==0)
                       {
                           last_meter.wh=0;
                           last_meter.varh=0;
                           last_meter.vah=0; 
                       }
                }

    }  
    

    struct tm tm; 
    cJSON * c_total_energy_datetime = cJSON_GetObjectItem(root,  "total_energy_datetime");
    if(cJSON_IsString(c_total_energy_datetime)) {
                 strncpy(str_total_energy_datetime,  c_total_energy_datetime->valuestring, sizeof(str_total_energy_datetime)-1);
                 
                  ESP_LOGE(TAG, "%s", str_total_energy_datetime);
                 if(strptime(str_total_energy_datetime, "%Y-%m-%d %H:%M:%S",  &tm) == NULL)
                 {
                       return;
                 }
                 get_nearest_data(str_total_energy_datetime,   noti_period);
                 flags.total_energy =1;  
    }





    cJSON * c_constant  = cJSON_GetObjectItem(root, "constant");
    if(cJSON_IsString(c_constant))  {
                strncpy(str_constant,  c_constant->valuestring, sizeof(str_constant)-1); 
                if(strcmp(str_constant, "get")==0)
                {
                        flags.constant=1;
                } 
    }  


    cJSON * c_restart =  cJSON_GetObjectItem(root,  "restart");
    if(cJSON_IsString(c_restart))   {
                  // flags.restart= 1;
               	ESP_LOGE(TAG, "force restart() after 2sec");
	            vTaskDelay(2000 / portTICK_PERIOD_MS);
                esp_restart();
     }



    char  *buffer=NULL;
    int  json_len= serialize_variable_event(flags , &buffer);
   
    tx_data_t  *tx_data =(tx_data_t *)mqtt_malloc_prefer_spiram(sizeof(tx_data_t) + (size_t)json_len);
    tx_data->id = idx; 
    tx_data->length = json_len;
    
    memcpy(&tx_data->data,buffer,json_len);
           
    if(xQueueSend(txQue, &tx_data,  pdMS_TO_TICKS(1000)) == pdPASS)  {
       idx++; 
    }  else {
       ESP_LOGW(TAG,  "Failed to send data");   
    }
     cJSON_Delete(root);   
}








#if 0
//  Function  to handle  JSON  configuration 
void  handle_config_json(const char * json_data,  int  json_data_len,  const char *cmd)
//void  handle_config_json(const char * json_data)
{
    ESP_LOGW(TAG, "cmd: %s", cmd );  
 / if(strcmp(cmd, "conf") == 0)
 	{
        cJSON  * root = cJSON_ParseWithLength(json_data, json_data_len);
 //        cJSON *root = cJSON_Parse(json_data);

          ESP_LOGI(TAG, "1");
		if(root  == NULL) {
             ESP_LOGE(TAG, "Invalid JSON data");
		     return;
		}    


        cJSON  *avcc =  cJSON_GetObjectItem(root, "avcc"); 
        
         if(cJSON_IsString(avcc)) {

			     ESP_LOGD(TAG, "Set avccValue to %s", avccValue);
                strncpy(avccValue, avcc->valuestring, sizeof(avccValue)-1);  
                avccValue[sizeof(avccValue)-1] = '\0';
               
		 }

        cJSON *aicc = cJSON_GetObjectItem(root, "aicc");
        if(cJSON_IsString(aicc)) {
               strncpy(aiccValue, aicc->valuestring, sizeof(aiccValue)-1);
			   aiccValue[sizeof(aiccValue)-1] = '\0';
               ESP_LOGD(TAG,  "Set aiccValue to %s", aiccValue);
		}


        cJSON *cal_irms_cc = cJSON_GetObjectItem(root, "cal_irms_cc");
		if(cJSON_IsString(cal_irms_cc))   {
            strncpy(str_cal_irms_cc, cal_irms_cc->valuestring,  sizeof(str_cal_irms_cc)-1);
            str_cal_irms_cc[sizeof(str_cal_irms_cc)-1]='\0';
			ESP_LOGD(TAG,  "Set str_cal_irms_cc to %s", str_cal_irms_cc);
		}


        cJSON *cal_vrms_cc = cJSON_GetObjectItem(root, "cal_vrms_cc");
        if(cJSON_IsString(cal_vrms_cc))  {
            strncpy(str_cal_vrms_cc, cal_vrms_cc->valuestring,  sizeof(str_cal_vrms_cc)-1);
			str_cal_vrms_cc[sizeof(str_cal_vrms_cc)-1]='\0'; 
			ESP_LOGD(TAG, "Set str_cal_vrms_cc to %s", str_cal_vrms_cc);
		}

        cJSON *cal_energy_cc  = cJSON_GetObjectItem(root, "cal_energy_cc"); 
        if(cJSON_IsString(cal_energy_cc))  {
            strncpy(str_cal_energy_cc, cal_energy_cc->valuestring, sizeof(str_cal_energy_cc)-1);
			str_cal_energy_cc[sizeof(str_cal_energy_cc)-1]='\0'; 
            ESP_LOGD(TAG, "Set str_cal_energy_cc to %s", str_cal_energy_cc); 
		}

        cJSON  *cal_power_cc  =  cJSON_GetObjectItem(root, "cal_power_cc");
		if(cJSON_IsString(cal_power_cc))    {
            strncpy(str_cal_power_cc,  cal_power_cc->valuestring, sizeof(str_cal_power_cc)-1);
			str_cal_power_cc[sizeof(str_cal_power_cc)-1]='\0';
			ESP_LOGD(TAG, "Set str_cal_power_cc to %s", str_cal_power_cc);
		} 
	}
}
#endif  

char* extract_prefix_before_brace(const char *str) {
    // Find the position of the first '{'
    const char *brace_pos = strchr(str, '{');
    if (brace_pos != NULL) {
        // Calculate the length of the prefix before '{'
        size_t prefix_len = brace_pos - str;
        
        // Allocate memory for the prefix string
        char *prefix = (char *)mqtt_malloc_prefer_spiram(prefix_len + 1);
        if (prefix == NULL) {
            printf("Memory allocation failed\n");
            return NULL;
        }
        
        // Copy the prefix to the new string
    

        strncpy(prefix, str, prefix_len);
        prefix[prefix_len] = '\0';  // Null-terminate the string
        
        // Return the extracted prefix
        return prefix;
    } else {
        printf("No '{' found in the string\n");
        printf("str: %s\n", str);
        return (char *)(uintptr_t)str;
    }
}





static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
	static int error_count = 0;
    static char json_buffer[512]; // Buffer to store the JSON data
    static int json_buffer_len = 0;
    static int64_t s_disc_heap_log_us;
    static uint32_t s_mqtt_before_connect_n;
    extern char    mqtt_topic_prefix[];
    extern uint8_t mesh_ap_ssid[];   



    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
			error_count = 0 ;
            s_mqtt_before_connect_n = 0;
            ESP_LOGD(TAG, "MQTT_EVENT_CONNECTED");
            char print[6*3+1]; // MAC addr size + terminator
//  //  //              if (esp_mqtt_client_subscribe(s_client, "/topic/ip_mesh/key_pressed", 0) < 0)
//  //              uint8_t *my_mac = mesh_netif_get_station_mac(); //두번하지 말고 한번만
//              uint8_t *my_mac ;
//  //  //              snprintf(print, sizeof(print),MACSTR, MAC2STR(my_mac));
//  			if (esp_mesh_is_root())
//  			{
//  		        my_mac = mesh_netif_get_rootnode_mac();
//  			}
//  			else
//  			{
//  		        my_mac = mesh_netif_get_station_mac();
//  			}
			memset(print, 0, sizeof(print));
//              snprintf(print, sizeof(print),"%02x%02x%02x_%02x%02x%02x", MAC2STR(my_mac));
            snprintf(print, sizeof(print),"%02x%02x%02x_%02x%02x%02x", MAC2STR(my_mac_factory));

            char topic[200]; // MAC addr size + terminator

			memset(topic, 0, sizeof(topic));
 //           snprintf(topic, sizeof(topic),"ijoon_iotech/ip_mesh/key_pressed_%s_ctrl", print);
            snprintf(topic, sizeof(topic),    "%s/key_pressed_%s_ctrl", mqtt_topic_prefix, print);
         
//              if (esp_mqtt_client_subscribe(s_client, "ijoon_iotech/ip_mesh/key_pressed", 0) < 0)

           	// ===========================================================
			// connected이면 mqtt_broker_uri에 접속했었기 때문에
			count_no_parent = 0;
			char buf[10];
			memset(buf,0,sizeof(buf));
			sprintf(buf,"%d",count_no_parent);
			iotech_set_nvs_str((uint8_t*)"count_no_parent", (uint8_t*)buf);
			// =========================================================== 

            /* ── stella status: online 발행 (disconnect 유발 subscribe 보다 먼저!) ── */
            if (stella_status_topic && strlen(stella_status_topic) > 0) {
                ESP_LOGD(TAG, "stella status topic=[%s]", stella_status_topic);
                stella_publish_status();
                status_timer_start();
            } else {
                ESP_LOGE(TAG, "stella_status_topic is NULL or empty! device_id=[%s]", device_id);
            }

            /* 브로커 TCP 연결되면 송신 가능 (CONNECTED 직후 flag=0 이었음 → SUBACK 누락 시 영구 미연결) */
            flag_mqtt_connect = 1;

            /* OTA 실패/이상 종료 등으로 stella_sensors_paused_for_ota가 1에 남으면 센서 태스크가 영구 대기 → 재연결 시 해제 */
            if (stella_sensors_paused_for_ota) {
                ESP_LOGD(TAG, "MQTT connected: clearing stella_sensors_paused_for_ota (was stuck)");
                stella_sensors_paused_for_ota = 0;
            }

            /* ── stella OTA 명령 토픽 구독 ── */
            if (stella_ota_cmd_topic && strlen(stella_ota_cmd_topic) > 0) {
                if (esp_mqtt_client_subscribe(s_client, stella_ota_cmd_topic, 1) < 0) {
                    ESP_LOGE(TAG, "stella OTA cmd subscribe failed");
                } else {
                    ESP_LOGD(TAG, "stella OTA cmd subscribed: %s", stella_ota_cmd_topic);
                }
            }

            /* 레거시 ijoon/iotech 토픽 구독 비활성화 (stella/device/... 로 통합됨) */
            // esp_mqtt_client_subscribe(s_client, topic, 0);           // key_pressed_ctrl
            // iotech/SEMS/.../conf  및  iotech/SEMS/.../cont 구독 제거

            iotech_get_nvs_str((uint8_t*)"noti_period", (uint8_t*)str_noti_period);
            noti_period = atoi(str_noti_period); 
            ESP_LOGD(TAG, "noti_period=%s, %d", str_noti_period, noti_period);

            break;
        case MQTT_EVENT_DISCONNECTED: {
            int64_t now_us = esp_timer_get_time();
            ESP_LOGD(TAG, "MQTT_EVENT_DISCONNECTED");
            /* 소켓 생성 실패(32770)·errno12 와 함께 올 때 내부 RAM 단편화 진단 */
            if (s_disc_heap_log_us == 0 || (now_us - s_disc_heap_log_us) >= 15 * 1000000LL) {
                mqtt_log_heap_diag("DISCONNECTED");
                s_disc_heap_log_us = now_us;
            }
			flag_mqtt_connect = 0;
            status_timer_stop();
            break;
        }
        case MQTT_EVENT_BEFORE_CONNECT:
            /* 로그의 예전 "Other event id:7" — 재시도 직전. socket ENOMEM 과 함께 자주 발생 */
            s_mqtt_before_connect_n++;
            if (s_mqtt_before_connect_n == 1u || (s_mqtt_before_connect_n % 5u) == 0u) {
                mqtt_log_heap_diag("BEFORE_CONNECT");
            }
            break;

        case MQTT_EVENT_SUBSCRIBED:
			error_count = 0 ;
            ESP_LOGD(TAG, "MQTT_EVENT_SUBSCRIBED msg_id=%d len=%d", event->msg_id, event->data_len);
		    // Check if the data is in JSON format
/*
         
 */			

			flag_mqtt_connect = 1;
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGD(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            /* online 재시도 중이었다면, 실제 브로커에 도달(PUBACK) 확인 → 재시도 중단 */
            if (s_online_last_msg_id >= 0 && event->msg_id == s_online_last_msg_id) {
                s_online_acked = true;
                online_retry_cancel();
                ESP_LOGD(TAG, "online PUBACK msg_id=%d -> retry stopped", event->msg_id);
            }
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGD(TAG, "DATA topic_len=%d data_len=%d", event->topic_len, event->data_len);
            int  topic_len =  event->topic_len;

            /* stella OTA 명령 처리 */
            if (stella_ota_cmd_topic &&
                event->topic_len == (int)strlen(stella_ota_cmd_topic) &&
                strncmp(event->topic, stella_ota_cmd_topic, event->topic_len) == 0)
            {
                if (event->total_data_len <= 0 || event->total_data_len >= OTA_MQTT_JSON_MAX) {
                    ESP_LOGE(TAG, "stella OTA cmd: total_data_len=%d invalid (max %d)",
                             event->total_data_len, OTA_MQTT_JSON_MAX - 1);
                    break;
                }
                if (event->current_data_offset + event->data_len > OTA_MQTT_JSON_MAX - 1) {
                    ESP_LOGE(TAG, "stella OTA cmd: fragment overflow off=%d + len=%d",
                             event->current_data_offset, event->data_len);
                    break;
                }
                memcpy(s_ota_mqtt_json + event->current_data_offset, event->data, event->data_len);
                if (event->current_data_offset + event->data_len < event->total_data_len) {
                    ESP_LOGD(TAG, "stella OTA cmd: fragment %d/%d",
                             event->current_data_offset + event->data_len, event->total_data_len);
                    break;
                }
                int plen = event->total_data_len;
                s_ota_mqtt_json[plen] = '\0';
                cJSON *ota_root = cJSON_ParseWithLength(s_ota_mqtt_json, plen);
                if (!ota_root) {
                    ESP_LOGE(TAG, "stella OTA cmd: JSON parse fail len=%d data=%.*s",
                             plen, plen > 120 ? 120 : plen, s_ota_mqtt_json);
                    break;
                }
                cJSON *url_item = cJSON_GetObjectItem(ota_root, "url");
                if (cJSON_IsString(url_item) && url_item->valuestring) {
                    strncpy((char *)ota_url, url_item->valuestring, 255);
                    ota_url[255] = '\0';
                    flag_ota_run = 1;
                    /* OTA 태스크의 "started" 발행·지연보다 먼저 — CO2/UART/ADC가 heap 경쟁 */
                    stella_sensors_paused_for_ota = 1;
                    ESP_LOGI(TAG, "stella OTA cmd (sensors paused)");
                } else {
                    ESP_LOGE(TAG, "stella OTA cmd: JSON에 문자열 \"url\" 없음");
                }
                cJSON_Delete(ota_root);
                break;
            }

            if (!stella_ota_cmd_topic) {
                static int s_logged_ota_topic_null;
                if (!s_logged_ota_topic_null) {
                    s_logged_ota_topic_null = 1;
                    ESP_LOGE(TAG, "stella_ota_cmd_topic==NULL OTA 명령 불가 device_id=[%s]", device_id);
                }
            }

            if( strncmp(event->data, "ota_run=1", event->data_len) == STR_MATCH)
            {
                 ESP_LOGD(TAG, "ota_run=1");
                 flag_ota_run = 1;
                 stella_sensors_paused_for_ota = 1;
            }
            else if(strncmp(event->data, "ota_run=0", event->data_len) == STR_MATCH)
            {
               	ESP_LOGD(TAG, "ota_run=0");
				flag_ota_run = 0;
            }
          
                // Ensure we do not overflow the buffer

		        if (json_buffer_len + event->data_len < sizeof(json_buffer)) {

                    memcpy(json_buffer + json_buffer_len, event->data, event->data_len);
                    json_buffer_len += event->data_len;

               //     ESP_LOGI(TAG, "json_len : %d, event data_len: %d", json_buffer_len, event->data_len); 
                   
                    // Null-terminate the buffer
                    json_buffer[json_buffer_len] = '\0';


                    char *Topic = (char *)mqtt_malloc_prefer_spiram((size_t)topic_len + 1u);
                    if (Topic == NULL) {
                             ESP_LOGE(TAG, "Memory allocation failed for Topic");
                            return  -1; 
                    }
                    strncpy(Topic, event->topic ,  topic_len);
                    Topic[topic_len] = '\0'; // Null-terminate the string

             //       ESP_LOGI(TAG, "Topic : %s", Topic);

                    
                    // Check if the JSON data is complete
                    if(event->data_len == event->total_data_len) {
                         if(event->data_len >0  && event->data[0] == '{') {
                         const char *str = strrchr(Topic, '/');
                         if (str != NULL) {
                             str++; // Move past the '/'
                        } else {
                             str = ""; // Fallback if '/' not found
                    }

                  ESP_LOGD(TAG, "cmd suffix:%s", str); 
                  if(str!=NULL)
                  {
                        if(strcmp(str, "conf") == 0) {
                          
                              //    ESP_LOGI(TAG, "conf" );
                                  handle_config_json(json_buffer,  json_buffer_len);
                        } 
                        else if(strcmp(str, "cont") == 0){
                               //  ESP_LOGI(TAG, "cont: %s", json_buffer);
                                 handle_control_json(json_buffer,  json_buffer_len);
                        }
                        else {
                               //  ESP_LOGE(TAG, "The command is %s.\n", str);
#if 0
                        	cJSON *root2 = cJSON_Parse(json_str);
			             	char *p;
			            	int flag;

		            		if( esp_mesh_is_root() == 1 ) // Root-Node만 설정함
				            {
					            if ( cJSON_GetObjectItem(root2, "mesh_ap_ssid"))
					            {
						            p = cJSON_GetObjectItem(root2,"mesh_ap_ssid")->valuestring;	
						            ESP_LOGI("shchoJSON", "mesh_ap_ssid=%s", p);
					            	iotech_set_nvs_str((uint8_t*)"mesh_ap_ssid", (uint8_t*)p);
				                }
					            // else if --> if
					            if ( cJSON_GetObjectItem(root2, "mesh_ap_passwd"))
					            {
					                	p = cJSON_GetObjectItem(root2,"mesh_ap_passwd")->valuestring;	
						                ESP_LOGI("shchoJSON", "mesh_ap_passwd=%s", p);
						                iotech_set_nvs_str((uint8_t*)"mesh_ap_passwd", (uint8_t*)p);
					            }
					            // else if --> if
				            	if ( cJSON_GetObjectItem(root2, "ntpserver"))
				            	{
						            p = cJSON_GetObjectItem(root2,"ntpserver")->valuestring;	
				            		ESP_LOGI("shchoJSON", "ntpserver=%s", p);
					            	iotech_set_nvs_str((uint8_t*)"ntpserver", (uint8_t*)p);
					            }
					            // else if --> if
				                if ( cJSON_GetObjectItem(root2, "mqtt_broker_uri"))
					            {
						            p = cJSON_GetObjectItem(root2,"mqtt_broker_uri")->valuestring;	
						            ESP_LOGI("shchoJSON", "mqtt_broker_uri=%s", p);
						            iotech_set_nvs_str((uint8_t*)"mqtt_broker_uri", (uint8_t*)p);
				  	            }
					            // else if --> if
					            if ( cJSON_GetObjectItem(root2, "ota_url"))
					            {
						            p = cJSON_GetObjectItem(root2,"ota_url")->valuestring;	
						            ESP_LOGI("shchoJSON", "ota_url=%s", (uint8_t*)p);
						            iotech_set_nvs_str((uint8_t*)"ota_url", (uint8_t*)p);
					            }
					            if ( cJSON_GetObjectItem(root2, "reboot_flag"))
				            	{
					            	flag = cJSON_GetObjectItem(root2,"reboot_flag")->valueint;	
						            ESP_LOGI("shchoJSON", "reboot_flag=%d", flag);
					            }
				            }
				        else
				        {
					            ESP_LOGW("shchoJSON", "I am not Root-node: Do not need to Apply this message");
				        }
#endif 
                      }
                  } 
                  json_buffer_len = 0;
                  mqtt_free_any(Topic);
  			 
		    }
		   }
         }
         break;
        case MQTT_EVENT_ERROR:
        {
            time_t now = 0;
            struct tm timeinfo = {0};
            time(&now);
            localtime_r(&now, &timeinfo);
            int cur_year = timeinfo.tm_year + 1900;

            if (cur_year < 2024) {
                ESP_LOGW(TAG, "MQTT_EVENT_ERROR: year=%d (SNTP not synced yet, TLS cert verify expected fail - not counting)", cur_year);
            } else {
                error_count++;
                /* 장기 장애는 mesh_main.c 의 [WDG] 5분 타이머가 처리. 여기서 esp_restart() 하지 않음 —
                 * BLE 연결/저장 재전송 세션이 리부팅으로 끊기는 문제 방지.
                 * 로그 폭주 방지: 1/5/20/이후 50회마다만 출력. */
                bool should_log =
                    (error_count == 1) ||
                    (error_count == 5) ||
                    (error_count == 20) ||
                    (error_count > 20 && (error_count % 50) == 0);
                if (should_log) {
                    ESP_LOGE(TAG, "MQTT_EVENT_ERROR: error_count=%d (auto-reconnect continues; WDG handles long outage)",
                             error_count);
                }
            }
        }
            break;
        default:
            ESP_LOGD(TAG, "Other mqtt event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, ">>>>>>>>>>>>>>> Event dispatched from event loop base=%s, event_id=%" PRId32 "", base, event_id);
    mqtt_event_handler_cb(event_data);
}

void mqtt_app_publish(char* topic, char *publish_string)
{
    if (!s_client) {
        return;
    }
    if (!topic || !publish_string) {
        ESP_LOGD(TAG, "mqtt_app_publish: NULL topic or payload");
        return;
    }
    int plen = (int)strlen(publish_string);
    int msg_id = esp_mqtt_client_publish(s_client, topic, publish_string, plen, 1, 0);
    if (msg_id < 0) {
        ESP_LOGD(TAG, "mqtt_app_publish FAILED topic=%s ret=%d", topic, msg_id);
    } else {
        ESP_LOGD(TAG, "mqtt_app_publish OK msg_id=%d len=%d", msg_id, plen);
    }
}

static void mqtt_apply_broker_uri_from_nvs(void)
{
    uint8_t buf[128];
    memset(buf, 0, sizeof(buf));
    esp_err_t e = iotech_get_nvs_str((uint8_t *)"mqtt_broker_uri", buf);
    if (e == ESP_OK && buf[0] != '\0') {
        strncpy(mqtt_broker_uri, (char *)buf, sizeof(mqtt_broker_uri) - 1);
        mqtt_broker_uri[sizeof(mqtt_broker_uri) - 1] = '\0';
        ESP_LOGD(TAG, "broker URI from NVS: %s", mqtt_broker_uri);
    } else {
        strncpy(mqtt_broker_uri, MQTT_BROKER_URI_DEFAULT, sizeof(mqtt_broker_uri) - 1);
        mqtt_broker_uri[sizeof(mqtt_broker_uri) - 1] = '\0';
        ESP_LOGD(TAG, "broker URI default (NVS err %s): %s",
                 esp_err_to_name(e), mqtt_broker_uri);
    }
}

void mqtt_app_start(void)
{
    ESP_LOGI(TAG, "mqtt_app_start");
    /* OTA 아님인데 mqtt 재시작만 된 경우(OTA 이상 종료 등) pause 래치가 남아 I2C/UART가 멈출 수 있음 */
    if (flag_ota_run == 0) {
        stella_sensors_paused_for_ota = 0;
    }
    mqtt_apply_broker_uri_from_nvs();
    /* SNTP 대기 루프 제거: MQTT를 즉시 시작하고 TLS 실패 시 auto-reconnect으로 SNTP 동기화 후 자동 연결 */
    {
        time_t now = 0;
        struct tm timeinfo = {0};
        time(&now);
        localtime_r(&now, &timeinfo);
        ESP_LOGD(TAG, "mqtt_app_start: year=%d (SNTP %s)",
                 timeinfo.tm_year + 1900,
                 (timeinfo.tm_year >= (2024 - 1900)) ? "synced" : "pending");
    }

    /* stella MQTT 토픽: PSRAM 우선, 실패 시 INTERNAL */
    if (stella_status_topic == NULL) {
        stella_status_topic       = heap_caps_calloc(1, STELLA_TOPIC_LEN, MALLOC_CAP_SPIRAM);
        stella_ota_cmd_topic      = heap_caps_calloc(1, STELLA_TOPIC_LEN, MALLOC_CAP_SPIRAM);
        stella_ota_progress_topic = heap_caps_calloc(1, STELLA_TOPIC_LEN, MALLOC_CAP_SPIRAM);
        stella_ota_result_topic   = heap_caps_calloc(1, STELLA_TOPIC_LEN, MALLOC_CAP_SPIRAM);

        if (!stella_status_topic || !stella_ota_cmd_topic ||
            !stella_ota_progress_topic || !stella_ota_result_topic) {
            ESP_LOGD(TAG, "stella topic SPIRAM alloc failed, retry INTERNAL");
            if (stella_status_topic) { heap_caps_free(stella_status_topic); stella_status_topic = NULL; }
            if (stella_ota_cmd_topic) { heap_caps_free(stella_ota_cmd_topic); stella_ota_cmd_topic = NULL; }
            if (stella_ota_progress_topic) { heap_caps_free(stella_ota_progress_topic); stella_ota_progress_topic = NULL; }
            if (stella_ota_result_topic) { heap_caps_free(stella_ota_result_topic); stella_ota_result_topic = NULL; }
            stella_status_topic       = heap_caps_calloc(1, STELLA_TOPIC_LEN, MALLOC_CAP_INTERNAL);
            stella_ota_cmd_topic      = heap_caps_calloc(1, STELLA_TOPIC_LEN, MALLOC_CAP_INTERNAL);
            stella_ota_progress_topic = heap_caps_calloc(1, STELLA_TOPIC_LEN, MALLOC_CAP_INTERNAL);
            stella_ota_result_topic   = heap_caps_calloc(1, STELLA_TOPIC_LEN, MALLOC_CAP_INTERNAL);
        }
        if (!(stella_status_topic && stella_ota_cmd_topic &&
              stella_ota_progress_topic && stella_ota_result_topic)) {
            ESP_LOGE(TAG, "stella topic alloc failed (SPIRAM+INTERNAL)");
        }
    }
    if (stella_status_topic && stella_ota_cmd_topic &&
        stella_ota_progress_topic && stella_ota_result_topic) {
        snprintf(stella_status_topic,       STELLA_TOPIC_LEN, "stella/device/%s/status",      device_id);
        snprintf(stella_ota_cmd_topic,      STELLA_TOPIC_LEN, "stella/device/%s/ota/command", device_id);
        snprintf(stella_ota_progress_topic, STELLA_TOPIC_LEN, "stella/device/%s/ota/progress",device_id);
        snprintf(stella_ota_result_topic,   STELLA_TOPIC_LEN, "stella/device/%s/ota/result",  device_id);
        ESP_LOGD(TAG, "stella topics ready: %s", stella_status_topic);
    }

#if !CONFIG_STELLA_MQTT_DISABLE_LWT
    /* LWT payload — 비정상 끊김 시 브로커가 한 번 전달 (retain=0: retained offline 방지) */
    static char lwt_payload[64];
    snprintf(lwt_payload, sizeof(lwt_payload), "{\"id\":\"%s\",\"status\":\"offline\"}", device_id);
#endif

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = mqtt_broker_uri,
        },
        .credentials = {
            .username = "ijoon",
            .authentication.password = "vXH5iVMqTfXB",
        },
        .task = {
            .stack_size = MQTT_CLIENT_TASK_STACK,
            .priority   = 5,
        },
        .buffer = {
            /* 입력·출력 버퍼를 기본 1024로 명시 — 내부 SRAM 연속 블록 2개만 차지
             * (과거 SDK 일부는 숨은 4096 기본값을 사용; 명시로 예측 가능성 확보). */
            .size     = MQTT_CLIENT_BUFFER_SIZE,
            .out_size = MQTT_CLIENT_OUT_BUFFER_SIZE,
        },
        .outbox = {
            /* 브로커 끊김 시 QoS1 적재 상한 — 내부 SRAM을 WiFi/MQTT 소켓에 남김 (menuconfig). */
            .limit = STELLA_MQTT_OUTBOX_LIMIT_B,
        },
        .network = {
            .timeout_ms = 10000,
            /* 5초마다 재시도 시 ENOMEM 상태에서 socket/TLS 할당만 반복해 힙·와이파이 상태를 악화시킴 */
            .reconnect_timeout_ms = MQTT_RECONNECT_TIMEOUT_MS,
        },
    };

    mqtt_cfg.session.keepalive = STELLA_MQTT_KEEPALIVE_SEC;
#if !CONFIG_STELLA_MQTT_DISABLE_LWT
    if (stella_status_topic && strlen(stella_status_topic) > 0) {
        mqtt_cfg.session.last_will.topic   = stella_status_topic;
        mqtt_cfg.session.last_will.msg       = lwt_payload;
        mqtt_cfg.session.last_will.msg_len   = (int)strlen(lwt_payload);
        mqtt_cfg.session.last_will.qos       = 1;
        /* retain=1 필수: 기존 retained "online"을 비정상 끊김 시 "offline"으로 덮어 대시보드·OTA 구독 상태를 맞춤 */
        mqtt_cfg.session.last_will.retain    = 1;
    }
#endif

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_client == NULL) {
        ESP_LOGE(TAG, "esp_mqtt_client_init FAILED");
        return;
    }
    ESP_LOGD(TAG, "esp_mqtt_client_init OK");

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, s_client);

    /* MQTT 라이브러리 내부 태스크 TCB는 내부 SRAM 연속 블록 필요.
     * 예약해 두었던 8KB 블록을 해제하여 연속 공간 확보.             */
    extern uint8_t *g_mqtt_internal_sram_reserve;
    if (g_mqtt_internal_sram_reserve) {
        heap_caps_free(g_mqtt_internal_sram_reserve);
        g_mqtt_internal_sram_reserve = NULL;
        ESP_LOGD(TAG, "internal SRAM reserve released: free=%lu",
                 (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    }

    esp_err_t err = esp_mqtt_client_start(s_client);
    if (err != ESP_OK) {
        size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        ESP_LOGD(TAG, "esp_mqtt_client_start 1st try FAILED: %s (if=%lu il=%lu)",
                 esp_err_to_name(err),
                 (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned long)largest);
        vTaskDelay(pdMS_TO_TICKS(400));
        err = esp_mqtt_client_start(s_client);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_mqtt_client_start FAILED: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
    } else {
        ESP_LOGI(TAG, "mqtt connecting");
    }
}

bool mqtt_app_has_client(void)
{
    return s_client != NULL;
}



void get_total_energy(void)
{
    ESP_LOGD(TAG, "get total_energy");
}

/* device_id가 이미 새 값으로 업데이트된 상태에서 호출.
 * 기존 토픽으로 offline 발행 → 클라이언트 정지/해제 → 새 device_id로 재시작. */
void mqtt_app_reconnect(void)
{
    ESP_LOGI(TAG, "mqtt_app_reconnect id=%s", device_id);

#if !CONFIG_STELLA_MQTT_DISABLE_LWT
    /* 기존 토픽에 offline 발행 (서버가 이전 ID를 offline으로 처리하도록) */
    if (s_client && stella_status_topic && strlen(stella_status_topic) > 0) {
        char offline_json[128];
        snprintf(offline_json, sizeof(offline_json),
                 "{\"id\":\"%s\",\"status\":\"offline\"}", device_id);
        {
            int olen = (int)strlen(offline_json);
            esp_mqtt_client_publish(s_client, stella_status_topic, offline_json, olen, 1, 1);
        }
        vTaskDelay(pdMS_TO_TICKS(400));
    }
#endif

    /* MQTT 클라이언트 정지 및 해제 */
    if (s_client) {
        esp_mqtt_client_stop(s_client);
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
    }

    /* 토픽 문자열 해제 (mqtt_app_start에서 새 device_id로 재할당됨) */
    if (stella_status_topic)       { heap_caps_free(stella_status_topic);       stella_status_topic       = NULL; }
    if (stella_ota_cmd_topic)      { heap_caps_free(stella_ota_cmd_topic);      stella_ota_cmd_topic      = NULL; }
    if (stella_ota_progress_topic) { heap_caps_free(stella_ota_progress_topic); stella_ota_progress_topic = NULL; }
    if (stella_ota_result_topic)   { heap_caps_free(stella_ota_result_topic);   stella_ota_result_topic   = NULL; }

    /* 새 device_id로 MQTT 재시작 */
    mqtt_app_start();
}

/* OTA 시작 전 호출: MQTT 소켓/SRAM을 해제하여 OTA HTTP에 메모리 확보 */
void mqtt_app_stop(void)
{
    ESP_LOGI(TAG, "mqtt_app_stop (OTA)");
    status_timer_stop();
    if (s_client && stella_status_topic && stella_status_topic[0] && flag_mqtt_connect) {
        char ota_off[128];
        int n = snprintf(ota_off, sizeof(ota_off),
                         "{\"id\":\"%s\",\"status\":\"offline\",\"reason\":\"ota_prepare\"}",
                         device_id);
        if (n > 0 && n < (int)sizeof(ota_off)) {
            (void)esp_mqtt_client_publish(s_client, stella_status_topic, ota_off, n, 1, 1);
            vTaskDelay(pdMS_TO_TICKS(300));
        }
    }
    if (s_client) {
        esp_mqtt_client_stop(s_client);
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
    }
    flag_mqtt_connect = 0;
    ESP_LOGD(TAG, "mqtt_app_stop done if=%lu",
             (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}
