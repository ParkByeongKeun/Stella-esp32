/*
 * agg_buffer.h
 *
 * BLE 오프라인 데이터 버퍼링 + 5분 집계 모듈. (Wearable 디바이스 전용)
 *
 * 개요
 *  - 본 모듈은 flag_IS_WEARABLE == 1 일 때만 실제로 동작한다.
 *    Static 보드에서는 agg_buffer_init() 이 즉시 ESP_OK 로 no-op 반환하며,
 *    snapshot/flush 태스크를 생성하지 않고 링 파일도 만들지 않는다.
 *
 *  - ble_send_noti_*() 가 호출될 때마다 최신 센서 값을 내부 테이블에 반영(agg_latest_update).
 *    static 디바이스에서는 내부적으로 무시된다.
 *  - BLE 구독(notify_state) 이 꺼진 구간에서만: 5분 창 동안 센서 ID 마다 산술 평균을 누적해 저장.
 *    snapshot_task 가 300초마다 창을 닫아 링버퍼에 기록한다(창 중에는 EXAMPLE_AGG_SNAPSHOT_PERIOD_S
 *    마다 온라인 전환만 감시).
 *    같은 5분 구간에서 나온 레코드들은 ts_epoch 이 동일 → 앱은 ts 로 그룹화.
 *  - 링버퍼 저장소(menuconfig):
 *      FAT 백엔드: /data/agg_ring.bin (1MB 고정 파일).
 *      SQLite 백엔드: /spiffs/agg_ring.db (슬롯 수 = EXAMPLE_AGG_SQLITE_MAX_SLOTS).
 *
 * 재연결 시 전송 순서(중요: 히스토리 우선 정책)
 *  1. 앱이 BLE 를 구독(notify ON) → agg_buffer_kick_flush() 가 호출되고
 *     flush_task 가 s_flushing = true 로 진입.
 *  2. agg_buffer_is_flushing() == true 인 동안, gatt_svc.c 의 라이브 notify
 *     (ble_send_noti_float/int/str) 는 전송을 잠시 보류한다.
 *     (agg_latest_update 자체는 계속 호출되어 최신값·오프라인 창 슬롯에는 반영됨.)
 *  3. 링버퍼의 pending 레코드를 "H,<ts>,<id>,<val>" 단문 notify 로 모두 전송.
 *  4. pending == 0 이 되면 s_flushing = false → 라이브 notify 재개.
 *  5. 전송 중 구독이 끊기면 해당 레코드는 다음 재연결 때 재전송.
 *     (앱은 ts+id 로 dedupe)
 *
 * 안드로이드 프로토콜은 docs/ANDROID_BLE_HISTORY.md 참고.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "sdkconfig.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AGG_FIELD_ID_LEN        12      /* null 포함. "Temperature" 등 길이 고려. */
#define AGG_MAX_FIELDS          16      /* 한 레코드에 담을 센서 종류 최대치. */
#if CONFIG_EXAMPLE_STORE_HISTORY && defined(CONFIG_EXAMPLE_AGG_SNAPSHOT_PERIOD_S) \
    && (CONFIG_EXAMPLE_AGG_SNAPSHOT_PERIOD_S > 0)
#if ((300 % CONFIG_EXAMPLE_AGG_SNAPSHOT_PERIOD_S) != 0)
#error "CONFIG_EXAMPLE_AGG_SNAPSHOT_PERIOD_S must divide 300 (5-minute window)"
#endif
#define AGG_SNAPSHOT_PERIOD_S   CONFIG_EXAMPLE_AGG_SNAPSHOT_PERIOD_S
#define AGG_SNAPSHOTS_PER_REC   (300 / CONFIG_EXAMPLE_AGG_SNAPSHOT_PERIOD_S)
#else
#define AGG_SNAPSHOT_PERIOD_S   10
#define AGG_SNAPSHOTS_PER_REC   30
#endif
#define AGG_RING_BYTES          (1024UL * 1024UL)   /* 1 MB (FAT 백엔드 파일 크기). */
#if CONFIG_EXAMPLE_HISTORY_BACKEND_SQLITE
#define AGG_RING_PATH           "/spiffs/agg_ring.db"
#else
#define AGG_RING_PATH           "/data/agg_ring.bin"
#endif

/* 모듈 시작. FAT 백엔드면 /data 마운트 후, SQLite 백엔드면 /spiffs 마운트 후 호출.
 * snapshot_task, flush_task 를 생성함.
 * 호출 시점에 flag_IS_WEARABLE != 1 이면 아무 리소스도 잡지 않고 ESP_ERR_NOT_SUPPORTED 반환
 * (상위에서 "웨어러블 전용 기능 비활성" 과 성공을 구분하기 위함). */
esp_err_t agg_buffer_init(void);

/* true: agg_buffer_init() 가 끝까지 성공(링 + snapshot/flush 태스크). */
bool agg_buffer_is_initialized(void);

/* 가장 최근 agg_buffer_init() 시도의 결과(성공 시 ESP_OK). agg_stats 진단용. */
esp_err_t agg_buffer_get_last_init_error(void);

/* 라이브 BLE notify wrapper 에서 매번 호출.
 *  - notify_state(구독 여부) 와 무관하게 항상 호출.
 *  - static 보드에서는 내부에서 즉시 return (no-op).
 *  - id 는 "CO2", "PM2.5", "Temperature" 등. AGG_FIELD_ID_LEN-1 까지 절삭. */
void agg_latest_update(const char *id, float value);

/* 지금 저장된 히스토리를 BLE 로 내보내는 중인지 여부.
 *   - true  : 링버퍼 flush 가 진행 중. 라이브 notify 는 보류해야 한다.
 *   - false : flush 가 끝났거나(= pending 0) 시작 전. 라이브 notify 정상 전송.
 * gatt_svc.c 의 ble_send_noti_*() 가 이 함수를 통해 라이브 전송을 게이팅한다. */
bool agg_buffer_is_flushing(void);

/* 콘솔 디버그용. */
void agg_buffer_get_stats(uint32_t *pending,
                          uint32_t *capacity,
                          uint32_t *read_idx,
                          uint32_t *write_idx);

/* 콘솔 디버그용(확장). 내부 동작이 정상인지 빠르게 확인하기 위한 카운터들. */
typedef struct {
    bool     init_done;      /* true: 링 열림 + 세마포 + snapshot/flush 태스크까지 모두 성공. */
    bool     enabled;        /* true: init_done && flag_IS_WEARABLE==1 (실제 저장/전송 경로 활성). */
    bool     flushing;       /* 현재 히스토리 flush 중 여부. */
    uint32_t updates;        /* agg_latest_update 가 성공적으로 반영된 총 횟수. */
    uint32_t drops;          /* 뮤텍스 경합으로 드롭된 갱신 수. */
    uint32_t ticks;          /* snapshot_task 가 한 번 깨어난 총 횟수. */
    uint32_t stale;          /* 예약(과거 평균 경로); 현재 미사용. */
    uint32_t writes;         /* 링에 실제로 기록된 레코드 수(누적). */
    uint32_t snap_count;     /* 현재 5분 창에서 샘플이 1회 이상 누적된 센서 슬롯 수. */
    uint32_t registered_ids; /* s_latest 에 등록된(=한 번이라도 갱신된) 센서 ID 수. */
} agg_diag_t;
void agg_buffer_get_diag(agg_diag_t *out);

/* 링버퍼 초기화(디스크 포함). 테스트/디버그용. */
void agg_buffer_reset(void);

/* BLE notify 구독이 새로 활성화됐을 때 호출.
 * flush_task 를 즉시 깨워 대기 중인 히스토리를 전송하도록 트리거함.
 * 상시 폴링을 없애기 위한 이벤트-주도 설계. */
void agg_buffer_kick_flush(void);

#ifdef __cplusplus
}
#endif
