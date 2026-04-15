idf_build_get_property(target IDF_TARGET)

if(${target} STREQUAL "linux")
    set(requires esp_event esp_stubs protocol_examples_common nvs_flash esp_idf_lib_helpers )
endif()

if(CONFIG_SOC_I2S_SUPPORTS_PDM_TX AND CONFIG_EXAMPLE_PDM_TX)
    list(APPEND srcs_i2s_pdm "i2s_pdm_tx.c")
endif()

if(CONFIG_SOC_I2S_SUPPORTS_PDM_RX AND CONFIG_EXAMPLE_PDM_RX)
    list(APPEND srcs_i2s_pdm "i2s_pdm_rx.c")
endif()

file(GLOB_RECURSE srcs_nimble_sec "nimble_security.c" "src_nimble_sec/*.c")

idf_component_register(SRCS "stella_main.c"
                            "stella_uart.c"
                            "cmd_i2ctools.c"
							"kiss_fftr.c" "kfc.c" "kiss_fft.c" "mesh_main.c"
                            "mesh_netif.c"
                            "mqtt_app.c"	
							"hexdump3.c"
							"led_strip_ctrl.c"
							"led_strip_encoder.c"
							"tcp_client_v4.c"
							"oled_ssd1306.c"
							"i2c2_sensors.c"
							"tcp_server.c"
							"spi_ads114S_test.c"
							"./esp32-fft/components/fft/fft.c"
							"continuous_read_main.c"
							"sntp_example_main.c"
							"light_sleep_example_main.c"
	          				"gpio_wakeup.c"
	          				"timer_wakeup.c"
	          				"uart_wakeup.c"
							"scan.c"
							"ade9153a_spi.c" 
							"make_json.c" 
							"rtc_time.c" 
							"meter_app.c"
							"ade9153a_spi.c"
							"meter_app.c"
							"spifss.c"
							"${srcs_i2s_pdm}"
							"${srcs_nimble_sec}"
                       INCLUDE_DIRS "."
					   		 "./include"
					         "./esp32-fft/components/fft/include"
                       EMBED_TXTFILES "${CMAKE_SOURCE_DIR}/server_certs/ca_cert.pem"
					   REQUIRES wifi_provisioning mqtt json spiffs sqlite3 fatfs esp_wifi esp_coex esp_adc esp_eth esp_http_client esp_https_ota app_update soft_i2c_master cmd_router driver ssd1306 sht4x sgp40 eeprom ${requires}  )

# PRIV_REQUIRES driver soft_i2c_master)
#
#							"ESP32_SoftWire.cpp"
#

#include_directories(${JSON_LIB_DIR})
#)
