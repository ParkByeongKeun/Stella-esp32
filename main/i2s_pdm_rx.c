/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdint.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include "soc/soc_caps.h"
#include "driver/i2s_pdm.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "sdkconfig.h"
#include "i2s_pdm_example.h" /* I2S_CHANNEL_DEFAULT_CONFIG, I2S_NUM_0 */

#include <math.h>
#include <stdio.h>
#include "freertos/message_buffer.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"

#include "fft.h"

#include "stella_global.h"
#include <cJSON.h>
#include <string.h>
#include <unistd.h>
#include "esp_heap_caps.h"

uint16_t strongest_k_u16[2];
uint16_t avg_u16[2];
uint16_t peak_u16[2];

#define EXAMPLE_PDM_RX_CLK_IO           (1)
#define EXAMPLE_PDM_RX_DIN_IO           (2)

#define EXAMPLE_PDM_RX_FREQ_HZ          16000

#ifndef PDM_MIC_SENS_DBFS_AT_94DB_SPL
#define PDM_MIC_SENS_DBFS_AT_94DB_SPL  (-36.0f)
#endif

#define GPIO_PDM_LR_SELECT_PIN		(21) 
#define PDM_SELECT_LEFT			(1)
#define PDM_SELECT_RIGHT		(0)

/////////////
// USER SETUP

#define DEBUG_FFT_STRONGEST
#ifndef CONFIG_I2S_PDM_DEBUG_PRINTF_HZ
#define CONFIG_I2S_PDM_DEBUG_PRINTF_HZ 2
#endif

#ifdef DEBUG_FFT_STRONGEST
static uint32_t s_pdm_strongest_last_print_ms;
#endif

#define I2S_BITS_PER_SAMPLE_16BIT (16)
#define SAMPLE_BIT_SIZE             (I2S_BITS_PER_SAMPLE_16BIT)
#define SAMPLE_RATE_HZ              (EXAMPLE_PDM_RX_FREQ_HZ)

#define FFT_BUF_SAMPLES             (2048)
#define FFT_BUFFERS                 (2)

#define DMA_BUF_BYTES               (1024)

/*
 * JSON / MQTT / BLE 발행 주기 (ms).
 * 웨어러블은 ble_send_noti_float("PDM_avg", spl_ui) 만 사용 — S_0_10 아님.
 * PDM JSON/BLE/서버 전송 주기(기본 5s). 빌드 시 -DPDM_BLE_PERIOD_MS=… 로 변경 가능.
 */
#ifndef PDM_BLE_PERIOD_MS
#define PDM_BLE_PERIOD_MS           (5000)
#endif
#define PDM_SPL_UI_TRIM_DB          (-14.0f)
/* 음수: 동일 전기 레벨에서 PDM_Avg(휴리스틱)·PDM_Avg_SPL_dB_est 모두 내려감(소음계와 맞출 때).
 * 보드별로 -DPDM_SPL_UI_EXTRA_DB=… 로 재조정. 예: 평시가 ~62인데 소음계 48~50이면 -10~-14 시도. */
#ifndef PDM_SPL_UI_EXTRA_DB
#define PDM_SPL_UI_EXTRA_DB         (-12.0f)
#endif
#ifndef PDM_RX_AMPLIFY_NUM
#define PDM_RX_AMPLIFY_NUM          (1)
#endif
#define PDM_SPL_BLEND_RMS_W         (0.55f)
#define PDM_SPL_BLEND_PEAKDB_W      (0.45f)
#define PDM_SPL_CREST_DB_GAIN       (0.15f)
#define PDM_SPL_CREST_DB_CAP        (22.0f)
#define PDM_LED_FREQ_HZ_MIN         (40.0f)

#ifndef PDM_RX_SLOT_CHOICE
#define PDM_RX_SLOT_CHOICE          (2)
#endif
#if (PDM_RX_SLOT_CHOICE < 0) || (PDM_RX_SLOT_CHOICE > 2)
#error "PDM_RX_SLOT_CHOICE must be 0 (LEFT), 1 (RIGHT), or 2 (BOTH)"
#endif

// END USER SETUP
/////////////////

#if SAMPLE_BIT_SIZE == I2S_BITS_PER_SAMPLE_16BIT
	#define FFT_BYTES_PER_SAMPLE        (2)
#else
	#error "16 bit only"
#endif

#define FFT_BUF_BYTES               (FFT_BUF_SAMPLES * FFT_BYTES_PER_SAMPLE)
#define PDM_BUF_BYTES               (FFT_BUF_BYTES * FFT_BUFFERS)
#define DMA_BUFFERS                 ((FFT_BUF_BYTES * FFT_BUFFERS) / DMA_BUF_BYTES)
#define BIN_WIDTH_HZ                ((float)SAMPLE_RATE_HZ / (float)FFT_BUF_SAMPLES)

extern SemaphoreHandle_t sema_tcp ;
extern SemaphoreHandle_t sema_spi_ads114s;
extern SemaphoreHandle_t sema_uart2 ;

extern char my_mac_str[32];
extern int flag_IS_WEARABLE ;
extern int fd_uart2 ;
extern int send_to_server(char *payload, int len);
extern int ble_send_noti_float(char *id, float value);
extern int ble_send_noti_int(char *id, int value);

extern void led_on(void) ;
extern void led_off(void) ;

static const char *JSON_TAG = "JSON";

uint8_t *PDMDataBuffer;

struct _pdm_msg
{
	float strongest_Hz;
	uint16_t avg;
	uint16_t peak;
	float avg_dbfs;
};

MessageBufferHandle_t buf_idx_msg_handle;
const size_t buf_idx_msg_bytes  = sizeof(size_t) + sizeof(size_t);

static portMUX_TYPE s_pdm_snap_lock = portMUX_INITIALIZER_UNLOCKED;
static struct _pdm_msg s_pdm_snap;
static volatile bool s_pdm_snap_ready;
static float s_pdm_period_peak_dbfs = -120.0f;
static float s_pdm_period_rms_sum = 0.0f;
static uint32_t s_pdm_period_rms_n = 0;

void disp_buf(uint8_t* buf, size_t length);
void disp_avg_buf(uint8_t* buf, size_t length, uint16_t *avg);


int PDM_LR_select(int pdm_select_val)
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = (1ULL << GPIO_PDM_LR_SELECT_PIN);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = 1; 
    io_conf.pull_down_en = 0;
    gpio_config(&io_conf);
	gpio_set_level(GPIO_PDM_LR_SELECT_PIN, pdm_select_val);
    return 1;
}

void disp_buf(uint8_t* buf, size_t length) {
	    uint16_t* b = (uint16_t*)buf;
    ESP_LOGW(    "disp_buf    ","length=%d\n", length);
    for (size_t i = 0; i < 32 ; i +=8 ) 
	{
		ESP_LOGW("disp_buf    ","[0] %5d [1] %5d [2] %5d [3] %5d [4] %5d [5] %5d [6] %5d [7] %5d",
			b[i+0], b[i+1], b[i+2], b[i+3], b[i+4], b[i+5], b[i+6], b[i+7]);
    }
}

void disp_avg_buf(uint8_t* buf, size_t length, uint16_t *avg_u16) {
    uint64_t acc = 0;
    uint16_t* b = (uint16_t*)buf;
    for (size_t idx = 0; idx < (length/FFT_BYTES_PER_SAMPLE); idx++) 
	{
        acc += b[idx];
    }
    ESP_LOGW("disp_avg_buf","averrage: %i(length=%d)", (int16_t)(acc/ (length/FFT_BYTES_PER_SAMPLE) ), length);
	*avg_u16 = (int16_t)(acc/ (length/FFT_BYTES_PER_SAMPLE) );
    ESP_LOGW("disp_avg_buf","avg_u16: %i (%d)", (uint16_t)*avg_u16, (int16_t)*avg_u16);
}

void calc_avg_peak_buf(uint8_t* buf, size_t length, uint16_t *avg_u16, uint16_t *peak_u16)
{
	const size_t n = length / FFT_BYTES_PER_SAMPLE;
	const int16_t *s = (const int16_t *)buf;

	int64_t acc = 0;
	uint32_t peak_abs = 0;

	for (size_t idx = 0; idx < n; idx++) {
		const int32_t v = (int32_t)s[idx];
		acc += v;
		const uint32_t a = (uint32_t)(v >= 0 ? v : -v);
		if (a > peak_abs) {
			peak_abs = a;
		}
	}
	if (n > 0) {
		int32_t mean = (int32_t)(acc / (int64_t)n);
		if (mean > 32767) {
			mean = 32767;
		} else if (mean < -32768) {
			mean = -32768;
		}
		*avg_u16 = (uint16_t)(int16_t)mean;
	} else {
		*avg_u16 = 0;
	}
	*peak_u16 = (uint16_t)peak_abs;
}

static float pdm_rms_dbfs_ac(const int16_t *s, size_t n)
{
	if (n == 0) {
		return -120.0f;
	}
	int64_t sum = 0;
	for (size_t i = 0; i < n; i++) {
		sum += (int32_t)s[i];
	}
	const double mean = (double)sum / (double)n;
	double acc = 0.0;
	for (size_t i = 0; i < n; i++) {
		const double v = (double)s[i] - mean;
		acc += v * v;
	}
	const double rms = sqrt(acc / (double)n);
	const double full_scale = 32768.0;
	const double eps = 1e-15;
	if (rms < eps) {
		return -120.0f;
	}
	return (float)(20.0 * log10(rms / full_scale));
}

static float pdm_peak_dbfs(const int16_t *s, size_t n)
{
	if (n == 0) {
		return -120.0f;
	}
	int32_t pk = 0;
	for (size_t i = 0; i < n; i++) {
		int32_t a = (int32_t)s[i];
		if (a < 0) {
			a = -a;
		}
		if (a > pk) {
			pk = a;
		}
	}
	if (pk < 1) {
		return -120.0f;
	}
	return 20.0f * log10f((float)pk / 32768.0f);
}

#define PDM_METER_TAIL_SAMPLES 384

static float pdm_loudness_dbfs_meter(const int16_t *s, size_t n)
{
	const float r = pdm_rms_dbfs_ac(s, n);
	const float p = pdm_peak_dbfs(s, n);
	const float full = fmaxf(r, p - 3.0f);

	const size_t nt = (n > PDM_METER_TAIL_SAMPLES) ? PDM_METER_TAIL_SAMPLES : n;
	const int16_t *st = s + (n - nt);
	const float rt = pdm_rms_dbfs_ac(st, nt);
	const float pt = pdm_peak_dbfs(st, nt);
	const float tail = fmaxf(rt, pt - 2.0f);

	return fmaxf(full, tail);
}

static float pdm_heuristic_spl_ui(float dbfs_blend)
{
	const float lo = -92.0f + PDM_SPL_UI_TRIM_DB;
	const float hi = -10.0f + PDM_SPL_UI_TRIM_DB;
	const float span = hi - lo;
	float t = (span > 1e-6f) ? ((dbfs_blend - lo) / span) : 0.0f;
	if (t < 0.0f) {
		t = 0.0f;
	}
	if (t > 1.0f) {
		t = 1.0f;
	}
	return 28.0f + t * (96.0f - 28.0f);
}

void task_send_JSON (void* arg) 
{
    while (1) 
	{
		vTaskDelay(pdMS_TO_TICKS(PDM_BLE_PERIOD_MS));

		struct _pdm_msg pdm_msg;
		float rms_mean_dbfs = -120.0f;
		portENTER_CRITICAL(&s_pdm_snap_lock);
		const bool have = s_pdm_snap_ready;
		if (have) {
			pdm_msg = s_pdm_snap;
			s_pdm_period_peak_dbfs = -120.0f;
			if (s_pdm_period_rms_n > 0) {
				rms_mean_dbfs = s_pdm_period_rms_sum / (float)s_pdm_period_rms_n;
			} else {
				rms_mean_dbfs = pdm_msg.avg_dbfs;
			}
			s_pdm_period_rms_sum = 0.0f;
			s_pdm_period_rms_n = 0;
		}
		portEXIT_CRITICAL(&s_pdm_snap_lock);
		if (!have) {
			continue;
		}

		float crest_db = pdm_msg.avg_dbfs - rms_mean_dbfs;
		if (crest_db < 0.0f) {
			crest_db = 0.0f;
		}
		if (crest_db > PDM_SPL_CREST_DB_CAP) {
			crest_db = PDM_SPL_CREST_DB_CAP;
		}
		const float dbfs_for_ui = PDM_SPL_BLEND_RMS_W * rms_mean_dbfs
			+ PDM_SPL_BLEND_PEAKDB_W * pdm_msg.avg_dbfs
			+ PDM_SPL_CREST_DB_GAIN * crest_db;
		const float dbfs_cal = dbfs_for_ui + PDM_SPL_UI_EXTRA_DB;
		const float spl_ui = pdm_heuristic_spl_ui(dbfs_cal);
		const float spl_db_est = 94.0f + dbfs_cal - PDM_MIC_SENS_DBFS_AT_94DB_SPL;

		char pdm_avg_str[16];
		memset(pdm_avg_str, 0, sizeof(pdm_avg_str));
		snprintf(pdm_avg_str, sizeof(pdm_avg_str), "%.3f", (double)spl_ui);

		{
		    ESP_LOGD(JSON_TAG, "Serialize.....PDM_Result");
		    cJSON *root = cJSON_CreateObject();
	    	cJSON_AddStringToObject(root, "Board_Serial_Num", my_mac_str);
		   	cJSON_AddNumberToObject(root, "PDM_BIN_WIDTH_HZ", BIN_WIDTH_HZ);
		   	cJSON_AddNumberToObject(root, "PDM_Strongest_Hz", pdm_msg.strongest_Hz);
		   	cJSON_AddNumberToObject(root, "PDM_Avg(raw)", pdm_msg.avg);
		   	cJSON_AddStringToObject(root, "PDM_Avg", pdm_avg_str);
		   	cJSON_AddNumberToObject(root, "PDM_Avg_SPL_ui", spl_ui);
		   	cJSON_AddNumberToObject(root, "PDM_Avg_dBFS", dbfs_for_ui);
		   	cJSON_AddNumberToObject(root, "PDM_Avg_SPL_dB_est", spl_db_est);
		   	cJSON_AddNumberToObject(root, "PDM_Peak", pdm_msg.peak);

		    char *my_json_string = cJSON_Print(root);
		   	ESP_LOGD("FAN", "my_json_string\n%s", my_json_string);

			ble_send_noti_float("PDM_Hz", pdm_msg.strongest_Hz);
			ble_send_noti_float("PDM_avg", spl_ui);
			ble_send_noti_int("PDM_peak", (int)pdm_msg.peak);

			if (flag_IS_WEARABLE == 0) {
				xSemaphoreTake(sema_uart2, portMAX_DELAY);
				write(fd_uart2, my_json_string, strlen(my_json_string));
				xSemaphoreGive(sema_uart2);
			} else {
				xSemaphoreTake(sema_tcp, portMAX_DELAY);
				send_to_server(my_json_string, strlen(my_json_string));
				xSemaphoreGive(sema_tcp);
			}
		   	cJSON_Delete(root);
			free(my_json_string);
		}
	}
}

void task_process (void* arg) 
{
    size_t buf_idx = 0;
    uint8_t* buf = NULL;

    while (1) 
	{
		stella_wait_while_ota_sensors_paused();
        size_t rx_bytes = xMessageBufferReceive( buf_idx_msg_handle, (void*)(&buf_idx), sizeof(buf_idx), portMAX_DELAY );
        assert(rx_bytes == sizeof(buf_idx));

        buf = &PDMDataBuffer[buf_idx * FFT_BUF_BYTES];
#ifdef DEBUG_PRINT_RAW
        disp_buf(buf, FFT_BUF_BYTES);
#endif
#ifdef DEBUG_PRINT_AVG
        disp_avg_buf(buf, FFT_BUF_BYTES, &avg_u16[buf_idx]);
#endif
        calc_avg_peak_buf(buf, FFT_BUF_BYTES, &avg_u16[buf_idx], &peak_u16[buf_idx]);

		const float inst_dbfs = pdm_loudness_dbfs_meter((const int16_t *)buf, FFT_BUF_SAMPLES);
		const float frame_rms_dbfs = pdm_rms_dbfs_ac((const int16_t *)buf, FFT_BUF_SAMPLES);
		portENTER_CRITICAL(&s_pdm_snap_lock);
		s_pdm_period_peak_dbfs = fmaxf(s_pdm_period_peak_dbfs, inst_dbfs);
		s_pdm_period_rms_sum += frame_rms_dbfs;
		s_pdm_period_rms_n++;
		portEXIT_CRITICAL(&s_pdm_snap_lock);

        fft_config_t *real_fft_plan = fft_init(FFT_BUF_SAMPLES, FFT_REAL, FFT_FORWARD, NULL, NULL);
        if (real_fft_plan == NULL) {
            ESP_LOGE("i2s_pdm", "fft_init failed");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        int16_t *i16buf = (int16_t *)buf;
        double sum_dc = 0.0;
        for (size_t k = 0; k < real_fft_plan->size; k++) {
            sum_dc += (double)i16buf[k];
        }
        const float mean_f = (float)(sum_dc / (double)real_fft_plan->size);
        for (size_t k = 0; k < real_fft_plan->size; k++) {
            real_fft_plan->input[k] = (float)i16buf[k] - mean_f;
        }

        fft_execute(real_fft_plan);

        size_t strongest_k = 1;
        float strongest_mag2 = -1.0f;
        for (size_t k = 1; k < real_fft_plan->size / 2; k++) {
            const float re = real_fft_plan->output[2 * k];
            const float im = real_fft_plan->output[2 * k + 1];
            const float mag2 = re * re + im * im;
            if (mag2 > strongest_mag2) {
                strongest_mag2 = mag2;
                strongest_k = k;
            }
        }

#ifdef DEBUG_FFT_STRONGEST
#if CONFIG_I2S_PDM_DEBUG_PRINTF_HZ > 0
			{
				uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
				uint32_t period_ms = 1000U / (unsigned)CONFIG_I2S_PDM_DEBUG_PRINTF_HZ;
				if (period_ms == 0) {
					period_ms = 1;
				}
				if ((now_ms - s_pdm_strongest_last_print_ms) >= period_ms) {
					s_pdm_strongest_last_print_ms = now_ms;
					printf("i2s_pdm: strongest(k=%4u) %5.3f(Hz) avg=%5d peak=%5u\n",
					       (unsigned)strongest_k, (float)strongest_k * BIN_WIDTH_HZ,
					       avg_u16[buf_idx], peak_u16[buf_idx]);
				}
			}
#endif
#endif
		const size_t k_min = (size_t)(PDM_LED_FREQ_HZ_MIN / BIN_WIDTH_HZ);
		if (strongest_k > k_min) {
			led_on();
		} else {
			led_off();
		}
		strongest_k_u16[buf_idx] = (uint16_t)strongest_k;

		portENTER_CRITICAL(&s_pdm_snap_lock);
		s_pdm_snap.strongest_Hz = (float)strongest_k * BIN_WIDTH_HZ;
		s_pdm_snap.avg = avg_u16[buf_idx];
		s_pdm_snap.peak = peak_u16[buf_idx];
		s_pdm_snap.avg_dbfs = s_pdm_period_peak_dbfs;
		s_pdm_snap_ready = true;
		portEXIT_CRITICAL(&s_pdm_snap_lock);

        fft_destroy(real_fft_plan);
	}
}


static i2s_chan_handle_t i2s_example_init_pdm_rx(void)
{
    i2s_chan_handle_t rx_chan;
    i2s_chan_config_t rx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
	#define I2S_CHANNEL_DEFAULT_CONFIG(i2s_num, i2s_role) { \
	    .id = i2s_num, \
	    .role = i2s_role, \
	    .dma_desc_num = 6, \
	    .dma_frame_num = 240, \
	    .auto_clear_after_cb = false, \
	    .auto_clear_before_cb = false, \
	    .intr_priority = 0, \
	}
	rx_chan_cfg.dma_desc_num = 2;
	rx_chan_cfg.dma_frame_num = 64;

    esp_err_t ret = i2s_new_channel(&rx_chan_cfg, NULL, &rx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE("I2S_PDM", "i2s_new_channel failed: %s", esp_err_to_name(ret));
        return NULL;
    }

    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(EXAMPLE_PDM_RX_FREQ_HZ),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = EXAMPLE_PDM_RX_CLK_IO,
            .din = EXAMPLE_PDM_RX_DIN_IO,
            .invert_flags = { .clk_inv = false, },
        },
    };

	PDM_LR_select(PDM_SELECT_LEFT);

#if CONFIG_IDF_TARGET_ESP32S3
    pdm_rx_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
#if PDM_RX_SLOT_CHOICE == 0
    pdm_rx_cfg.slot_cfg.slot_mask = I2S_PDM_SLOT_LEFT;
#elif PDM_RX_SLOT_CHOICE == 1
    pdm_rx_cfg.slot_cfg.slot_mask = I2S_PDM_SLOT_RIGHT;
#else
    pdm_rx_cfg.slot_cfg.slot_mask = I2S_PDM_SLOT_BOTH;
#endif
#if SOC_I2S_SUPPORTS_PDM_RX_HP_FILTER
    {
        uint32_t amp = (uint32_t)PDM_RX_AMPLIFY_NUM;
        if (amp < 1U) amp = 1U;
        else if (amp > 15U) amp = 15U;
        pdm_rx_cfg.slot_cfg.amplify_num = amp;
    }
#endif
#endif
    ret = i2s_channel_init_pdm_rx_mode(rx_chan, &pdm_rx_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE("I2S_PDM", "i2s_channel_init_pdm_rx_mode failed: %s", esp_err_to_name(ret));
        i2s_del_channel(rx_chan);
        return NULL;
    }

#if CONFIG_IDF_TARGET_ESP32S3
    ESP_LOGI("i2s_pdm", "PDM_RX_SLOT_CHOICE=%d", PDM_RX_SLOT_CHOICE);
#endif

    ret = i2s_channel_enable(rx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE("I2S_PDM", "i2s_channel_enable failed: %s", esp_err_to_name(ret));
        i2s_del_channel(rx_chan);
        return NULL;
    }
    return rx_chan;
}

void i2s_example_pdm_rx_task(void *args)
{
    size_t buf_idx = 0;
    uint8_t* buf = NULL;

    buf_idx_msg_handle = xMessageBufferCreate( buf_idx_msg_bytes );
    assert(buf_idx_msg_handle);

    PDMDataBuffer = (uint8_t *)calloc(1, PDM_BUF_BYTES);
    if (PDMDataBuffer == NULL) {
        ESP_LOGE("I2S_PDM", "Failed to allocate PDMDataBuffer (%d bytes)", PDM_BUF_BYTES);
        vTaskSuspend(NULL);
    }

	TaskHandle_t pdm_send_json_task;
    xTaskCreatePinnedToCore(task_send_JSON, "pdm_JSON", 1024 * 4, NULL, 5, &pdm_send_json_task, 1);

	TaskHandle_t pdm_process_task;
    xTaskCreatePinnedToCore(task_process, "pdm process", 1024 * 4, NULL, 5, &pdm_process_task, 1);

	int count_loop = 0 ;

    ESP_LOGW("I2S_PDM", "Waiting 15s for WiFi/Mesh init to settle before I2S DMA alloc...");
    vTaskDelay(pdMS_TO_TICKS(15000));

    i2s_chan_handle_t rx_chan = NULL;
    for (int retry = 0; retry < 5; retry++) {
        size_t free_dma = heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        size_t largest_dma = heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        ESP_LOGW("I2S_PDM", "[retry %d] Free DMA memory: %u bytes, largest block: %u bytes", retry, free_dma, largest_dma);

        rx_chan = i2s_example_init_pdm_rx();
        if (rx_chan != NULL) {
            ESP_LOGW("I2S_PDM", "PDM RX init succeeded on retry %d", retry);
            break;
        }
        ESP_LOGE("I2S_PDM", "PDM RX init failed (retry %d/4), waiting 10s...", retry);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
    if (rx_chan == NULL) {
        ESP_LOGE("I2S_PDM", "PDM RX init failed after all retries, task suspended");
        vTaskSuspend(NULL);
    }

    size_t r_bytes = 0;
    while (1) 
	{
        buf = &PDMDataBuffer[buf_idx * FFT_BUF_BYTES];
    	uint16_t *r_buf = (uint16_t *)buf;
        if (i2s_channel_read(rx_chan, buf, FFT_BUF_BYTES, &r_bytes, portMAX_DELAY) == ESP_OK) 
		{
			if (r_bytes != FFT_BUF_BYTES) {
				ESP_LOGW("PDM Read Task", "short read: got %u bytes", (unsigned)r_bytes);
				count_loop++;
				continue;
			}
			#ifdef DEBUG_PRINT_RAW
			if( count_loop % 10 == 0 ) 
			{
				ESP_LOGI("task rx loop","Read Task: i2s read %d bytes\n", r_bytes);
			    for (size_t i = 0; i < 32 ; i +=8 ) 
				{
		            ESP_LOGI("task rx loop","[0] %5d [1] %5d [2] %5d [3] %5d [4] %5d [5] %5d [6] %5d [7] %5d",
		                   r_buf[i+0], r_buf[i+1], r_buf[i+2], r_buf[i+3], r_buf[i+4], r_buf[i+5], r_buf[i+6], r_buf[i+7]);
			    }
			}
			#endif

	        size_t tx_bytes = xMessageBufferSend(buf_idx_msg_handle, &buf_idx, sizeof(size_t), portMAX_DELAY);
			if( tx_bytes != sizeof(size_t))
			{
				ESP_LOGE("xMessageBufferSend", "failed to send");
			}
	        buf_idx++;
	        if(buf_idx >= FFT_BUFFERS)
			{
	            buf_idx = 0;
	        }
        } 
		else {
            ESP_LOGE("PDM Read Task","i2s read failed\n");
        }
		count_loop++;
    }

    vTaskDelete(NULL);
}
