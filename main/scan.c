/* Scan Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*
    This example shows how to scan for available set of APs.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
//  #include "nvs_flash.h"

//shcho add
#include "esp_mac.h"


//  #define DEFAULT_SCAN_LIST_SIZE CONFIG_EXAMPLE_SCAN_LIST_SIZE
#define DEFAULT_SCAN_LIST_SIZE 10   // 20-->10

static const char *TAG = "scan";

#if 0
static void print_auth_mode(int authmode)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OPEN");
        break;
    case WIFI_AUTH_OWE:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OWE");
        break;
    case WIFI_AUTH_WEP:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WEP");
        break;
    case WIFI_AUTH_WPA_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_PSK");
        break;
    case WIFI_AUTH_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
        break;
    case WIFI_AUTH_ENTERPRISE:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_ENTERPRISE");
        break;
    case WIFI_AUTH_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_PSK");
        break;
    case WIFI_AUTH_WPA2_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_PSK");
        break;
    case WIFI_AUTH_WPA3_ENT_192:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_ENT_192");
        break;
    case WIFI_AUTH_WPA3_EXT_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_EXT_PSK");
        break;
    case WIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE");
        break;
    default:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_UNKNOWN");
        break;
    }
}

static void print_cipher_type(int pairwise_cipher, int group_cipher)
{
    switch (pairwise_cipher) {
    case WIFI_CIPHER_TYPE_NONE:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    case WIFI_CIPHER_TYPE_AES_CMAC128:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_AES_CMAC128");
        break;
    case WIFI_CIPHER_TYPE_SMS4:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_SMS4");
        break;
    case WIFI_CIPHER_TYPE_GCMP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_GCMP");
        break;
    case WIFI_CIPHER_TYPE_GCMP256:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_GCMP256");
        break;
    default:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }

    switch (group_cipher) {
    case WIFI_CIPHER_TYPE_NONE:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    case WIFI_CIPHER_TYPE_SMS4:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_SMS4");
        break;
    case WIFI_CIPHER_TYPE_GCMP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_GCMP");
        break;
    case WIFI_CIPHER_TYPE_GCMP256:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_GCMP256");
        break;
    default:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }
}

#endif

/* Initialize Wi-Fi as sta and set scan method */
//  static void wifi_scan(void)
static wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
void wifi_scan_best_rssi_among_Iotech_Router(char *ssid)
{
//      ESP_ERROR_CHECK(esp_netif_init());
//      ESP_ERROR_CHECK(esp_event_loop_create_default());
//      esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
//      assert(sta_netif);
//  
//      wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//      ESP_ERROR_CHECK(esp_wifi_init(&cfg));
//  

    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_scan_start(NULL, true);
    ESP_LOGI(TAG, "Max AP number ap_info can hold = %u", number);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);
	int rssi_max  = -200 ;
	int rssi_max_index  = 0 ;
    for (int i = 0; i < number; i++) {
//  		if(    ( strcasestr((char *)ap_info[i].ssid, "IoTech-Router-") !=NULL ) 
//  		    || ( strcasestr((char *)ap_info[i].ssid, "iotech") !=NULL ) )
		if(    ( strcasestr((char *)ap_info[i].ssid, "IoTech-Router-") !=NULL ) ) 
		{
			ESP_LOGI(TAG, "-----------------------------------------------");
	        ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
	        ESP_LOGI(TAG, "BSSID \t\t%02x%02x%02x_%02x%02x%02x", MAC2STR(ap_info[i].bssid) );
	        ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
			if( ap_info[i].rssi >= rssi_max  )
			{
				rssi_max = ap_info[i].rssi;
				rssi_max_index = i;
			}

//  	        print_auth_mode(ap_info[i].authmode);
//  	        if (ap_info[i].authmode != WIFI_AUTH_WEP) {
//  	            print_cipher_type(ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
//  	        }
//  	        ESP_LOGI(TAG, "Channel \t\t%d", ap_info[i].primary);
			ESP_LOGI(TAG, "-----------------------------------------------");
		}
    }

	ESP_LOGI(TAG, "--------Selected-------------------------------");
	ESP_LOGI(TAG, "SSID \t\t%s", ap_info[rssi_max_index].ssid);
	ESP_LOGI(TAG, "BSSID \t\t%02x%02x%02x_%02x%02x%02x", MAC2STR(ap_info[rssi_max_index].bssid) );
	ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[rssi_max_index].rssi);
	ESP_LOGI(TAG, "-----------------------------------------------");
	memcpy(ssid, (char *)ap_info[rssi_max_index].ssid, strlen((char *)ap_info[rssi_max_index].ssid));

}

//  void app_main(void)
//  {
//      // Initialize NVS
//      esp_err_t ret = nvs_flash_init();
//      if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
//          ESP_ERROR_CHECK(nvs_flash_erase());
//          ret = nvs_flash_init();
//      }
//      ESP_ERROR_CHECK( ret );
//  
//      wifi_scan();
//  }
