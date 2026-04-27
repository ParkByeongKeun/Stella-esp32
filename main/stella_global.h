//  Static Main과 Wearable Main을 동시에 고려해야 하기때문에
//  변수로 처리함
//  #define WEARABLE_USE_W5500 (1)

#ifndef STELLA_GLOBAL_H
#define STELLA_GLOBAL_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/** OTA 구간에서 1 — 센서 태스크가 루프에서 대기(내부 힙·TLS 경쟁 완화). 실패 시 mesh_main에서 0. 성공 시 재부팅. */
extern volatile int stella_sensors_paused_for_ota;
void stella_wait_while_ota_sensors_paused(void);

/** 메시 부모(또는 루트→공유기) 연결 후 호출 — OLED/BLE/UART/I2C/PDM/ADC 등 부하 시작 */
void stella_start_heavy_sensor_workloads(void);
bool stella_heavy_workloads_started(void);
/** app_main_stella 기본 초기화(세마포 등) 완료 후 + mesh 부모 연결 시에만 부하 시작 */
void stella_maybe_start_heavy_if_ready(void);
extern volatile bool g_mesh_parent_connected_flag;

/* CONFIG_EXAMPLE_STORE_HISTORY=y 일 때 heavy 안의 early FAT+agg 블록 진단용 */
bool stella_early_agg_block_ran(void);
esp_err_t stella_early_agg_last_fs_err(void);
esp_err_t stella_early_agg_last_agg_err(void);

#endif /* STELLA_GLOBAL_H */
