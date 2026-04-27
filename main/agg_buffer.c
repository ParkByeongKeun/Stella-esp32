/*
 * agg_buffer.c : 오프라인 BLE 데이터 버퍼링 + 5분 창(센서별 산술 평균, 상세는 agg_buffer.h)
 */

#include "agg_buffer.h"

#include "sdkconfig.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/idf_additions.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#if CONFIG_EXAMPLE_STORE_HISTORY && CONFIG_EXAMPLE_HISTORY_BACKEND_SQLITE
#include "sqlite3.h"
#endif

static const char *TAG = "agg_buffer";

/* 5분 창 닫을 때 SQLite/VFS 가 한꺼번에 돌면 MQTT/TCP 와 힙·CPU 경쟁 → 끊김 완화용 */
#ifndef AGG_FINALIZE_COOP_MS
#define AGG_FINALIZE_COOP_MS  20
#endif
#ifndef AGG_SQLITE_STEP_YIELD_INTERVAL
#define AGG_SQLITE_STEP_YIELD_INTERVAL  4
#endif
#ifndef AGG_FILE_WRITE_INTER_MS
#define AGG_FILE_WRITE_INTER_MS  8
#endif

/* 가장 마지막 init 시도 오류(성공 시 ESP_OK). stella early 의 s_early_agg_err 와 별개. */
static esp_err_t s_last_init_err = ESP_ERR_INVALID_STATE; /* init 시도 전 */

#if CONFIG_EXAMPLE_STORE_HISTORY && CONFIG_EXAMPLE_HISTORY_BACKEND_SQLITE
static sqlite3 *s_agg_db;
#endif

/* 외부 참조 */
extern bool notify_state;              /* gatt_svc.c / nimble_security.c */
extern int  ble_send_raw(const char *s); /* gatt_svc.c (아래 파일에서 추가) */
extern int  flag_IS_WEARABLE;          /* stella_main.c: 1=Wearable, 0=Static.
                                        * stella_early_mux_and_refresh_device_id() 가
                                        * 매우 이른 시점에 확정하므로 agg_buffer_init
                                        * 시점에는 이미 유효하다. */
extern int  flag_mqtt_connect;         /* mqtt_app.c — finalize 직전 I/O 스파이크가
                                        * lwIP/MQTT 와 내부 힙을 경쟁할 때 짧게 양보. */

/* ====================== 레코드/헤더 정의 ====================== */

/* v2: 레코드 = "센서 1개" 단위.
 * 같은 5분 구간에서 나온 레코드들은 ts_epoch 이 같다 (앱이 ts 로 그룹화). */
#define AGG_MAGIC           0x41474732u   /* 'AGG2' - v1(264B 레코드)과 구분. */
#define AGG_VERSION         2u
#define AGG_HEADER_BYTES    64u

typedef struct __attribute__((packed)) {
    uint32_t ts_epoch;                 /* Unix time(sec). 0 이면 시간 미동기 상태. */
    char     id[AGG_FIELD_ID_LEN];     /* null 종단 문자열 (최대 11자). */
    float    avg;                      /* 5분 창 동안 해당 id 의 산술 평균. */
} agg_record_t;                        /* 4 + 12 + 4 = 20 bytes */

_Static_assert(sizeof(agg_record_t) == 20, "agg_record_t size unexpected");

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint32_t capacity;       /* 슬롯 수 */
    uint32_t record_size;    /* sizeof(agg_record_t) */
    uint32_t read_idx;       /* 다음 읽을 슬롯 */
    uint32_t write_idx;      /* 다음 쓸 슬롯 */
    uint32_t count;          /* 미전송 레코드 수 */
    uint32_t crc32;          /* 상기 7개 필드의 단순 XOR 합(간이 검증) */
    uint8_t  pad[AGG_HEADER_BYTES - 32];
} agg_header_t;

_Static_assert(sizeof(agg_header_t) == AGG_HEADER_BYTES, "agg_header_t size unexpected");

/* ====================== 내부 상태 ====================== */

typedef struct {
    char    id[AGG_FIELD_ID_LEN];
    float   latest;
    int64_t last_update_us;
    bool    valid;
} agg_latest_entry_t;

/* 5분 오프라인 창: 센서별 산술 평균(sum/n). agg_latest_update 가 올 때마다 누적. */
typedef struct {
    char     id[AGG_FIELD_ID_LEN];
    double   sum;
    uint32_t n;
} agg_window_entry_t;

static SemaphoreHandle_t  s_latest_mtx;
static SemaphoreHandle_t  s_ring_mtx;
/* flush_task 기상 트리거. 구독 ON / 새 레코드 기록 시 give. */
static SemaphoreHandle_t  s_flush_trig;

static agg_latest_entry_t s_latest[AGG_MAX_FIELDS];
static agg_window_entry_t s_window[AGG_MAX_FIELDS];

static agg_header_t       s_hdr;                /* 메모리 캐시. 디스크와 동기화. */
static bool               s_initialized = false;

/* 진단용 통계. 콘솔에서 agg_stats 출력을 풍부하게 하려고 수집.
 *   s_stat_updates: agg_latest_update() 가 s_latest 에 실제로 반영된 횟수.
 *   s_stat_drops  : 뮤텍스 경합으로 드롭된 갱신 횟수.
 *   s_stat_ticks  : snapshot_task 가 5분 창을 한 번 닫은 횟수.
 *   s_stat_stale  : 예약(미사용).
 *   s_stat_writes : finalize_and_write_record() 가 실제로 링에 쓴 레코드 수. */
static uint32_t           s_stat_updates = 0;
static uint32_t           s_stat_drops   = 0;
static uint32_t           s_stat_ticks   = 0;
static uint32_t           s_stat_stale   = 0;
static uint32_t           s_stat_writes  = 0;

/* init 중간 실패 시 잡은 리소스 정리(부분 init 으로 s_initialized 가 true 가 되는 버그 방지). */
static void agg_abort_init_cleanup(void) {
    s_initialized = false;
    memset(&s_hdr, 0, sizeof(s_hdr));
    memset(s_latest, 0, sizeof(s_latest));
    memset(s_window, 0, sizeof(s_window));
    if (s_flush_trig) {
        vSemaphoreDelete(s_flush_trig);
        s_flush_trig = NULL;
    }
    if (s_ring_mtx) {
        vSemaphoreDelete(s_ring_mtx);
        s_ring_mtx = NULL;
    }
    if (s_latest_mtx) {
        vSemaphoreDelete(s_latest_mtx);
        s_latest_mtx = NULL;
    }
#if CONFIG_EXAMPLE_STORE_HISTORY && CONFIG_EXAMPLE_HISTORY_BACKEND_SQLITE
    if (s_agg_db) {
        sqlite3_close(s_agg_db);
        s_agg_db = NULL;
    }
#endif
}

/* 히스토리 flush 상태:
 *   true  = flush_task 가 pending 레코드를 BLE 로 내보내는 중.
 *           이 구간에는 gatt_svc.c 의 라이브 notify 를 잠시 보류해야
 *           앱이 "저장된 히스토리 먼저 → 그 다음 라이브" 순서로 받을 수 있다.
 *   false = flush 종료(또는 시작 전, pending==0, notify 해제).
 *
 * volatile: flush_task 가 쓰고, 다른 태스크(라이브 notify) 가 읽음. */
static volatile bool      s_flushing    = false;

/* ====================== 유틸 ====================== */

static uint32_t hdr_crc_calc(const agg_header_t *h) {
    return h->magic ^ h->version ^ h->capacity ^ h->record_size
         ^ h->read_idx ^ h->write_idx ^ h->count ^ 0xA5A5A5A5u;
}

#if CONFIG_EXAMPLE_STORE_HISTORY && CONFIG_EXAMPLE_HISTORY_BACKEND_FAT

static off_t slot_offset(uint32_t idx) {
    return (off_t)AGG_HEADER_BYTES + (off_t)idx * (off_t)sizeof(agg_record_t);
}

static uint32_t capacity_for(uint32_t bytes) {
    return (bytes - AGG_HEADER_BYTES) / sizeof(agg_record_t);
}

/* 헤더를 디스크에 동기화. 호출자가 s_ring_mtx 를 보유한 상태여야 함. */
static esp_err_t hdr_write(void) {
    s_hdr.crc32 = hdr_crc_calc(&s_hdr);
    FILE *f = fopen(AGG_RING_PATH, "r+b");
    if (!f) {
        ESP_LOGE(TAG, "hdr_write: fopen r+b failed errno=%d", errno);
        return ESP_FAIL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return ESP_FAIL; }
    size_t n = fwrite(&s_hdr, 1, sizeof(s_hdr), f);
    fflush(f);
    fclose(f);
    return (n == sizeof(s_hdr)) ? ESP_OK : ESP_FAIL;
}

/* 파일을 새로 만들고 capacity 만큼 슬롯 공간 확보. */
static esp_err_t ring_create(uint32_t bytes) {
    FILE *f = fopen(AGG_RING_PATH, "wb");
    if (!f) {
        ESP_LOGE(TAG, "ring_create: fopen wb failed errno=%d", errno);
        return ESP_FAIL;
    }

    memset(&s_hdr, 0, sizeof(s_hdr));
    s_hdr.magic       = AGG_MAGIC;
    s_hdr.version     = AGG_VERSION;
    s_hdr.capacity    = capacity_for(bytes);
    s_hdr.record_size = sizeof(agg_record_t);
    s_hdr.read_idx    = 0;
    s_hdr.write_idx   = 0;
    s_hdr.count       = 0;
    s_hdr.crc32       = hdr_crc_calc(&s_hdr);

    if (fwrite(&s_hdr, 1, sizeof(s_hdr), f) != sizeof(s_hdr)) {
        ESP_LOGE(TAG, "ring_create: write header failed");
        fclose(f);
        return ESP_FAIL;
    }

    /* 슬롯 영역은 굳이 0 으로 채우지 않음(FAT 파일은 sparse 가 아니므로 필요 시 extend). */
    /* 파일 크기를 확장하여 write 시 seek 실패 방지. */
    if (fseek(f, slot_offset(s_hdr.capacity) - 1, SEEK_SET) != 0) {
        ESP_LOGE(TAG, "ring_create: seek extend failed");
        fclose(f);
        return ESP_FAIL;
    }
    uint8_t zero = 0;
    fwrite(&zero, 1, 1, f);
    fflush(f);
    fclose(f);

    ESP_LOGD(TAG, "ring created: capacity=%lu (%.1f days @5min)",
             (unsigned long)s_hdr.capacity,
             (s_hdr.capacity * 5.0f) / (60.0f * 24.0f));
    return ESP_OK;
}

/* 기존 파일 열어서 헤더 로드. 손상/사이즈 불일치면 재생성. */
static esp_err_t ring_open_or_create(void) {
    struct stat st;
    bool need_create = false;

    if (stat(AGG_RING_PATH, &st) != 0) {
        need_create = true;
    } else if ((uint32_t)st.st_size != AGG_RING_BYTES) {
        ESP_LOGW(TAG, "ring size mismatch (%ld vs %lu) -> recreate",
                 (long)st.st_size, (unsigned long)AGG_RING_BYTES);
        need_create = true;
    } else {
        FILE *f = fopen(AGG_RING_PATH, "rb");
        if (!f) {
            ESP_LOGW(TAG, "ring open failed, recreating");
            need_create = true;
        } else {
            size_t n = fread(&s_hdr, 1, sizeof(s_hdr), f);
            fclose(f);
            if (n != sizeof(s_hdr)
                || s_hdr.magic != AGG_MAGIC
                || s_hdr.version != AGG_VERSION
                || s_hdr.record_size != sizeof(agg_record_t)
                || s_hdr.capacity != capacity_for(AGG_RING_BYTES)
                || s_hdr.crc32 != hdr_crc_calc(&s_hdr)
                || s_hdr.read_idx >= s_hdr.capacity
                || s_hdr.write_idx >= s_hdr.capacity
                || s_hdr.count > s_hdr.capacity) {
                ESP_LOGW(TAG, "ring header invalid -> recreate");
                need_create = true;
            }
        }
    }

    if (need_create) {
        return ring_create(AGG_RING_BYTES);
    }

    ESP_LOGD(TAG, "ring opened: read=%lu write=%lu pending=%lu cap=%lu",
             (unsigned long)s_hdr.read_idx,
             (unsigned long)s_hdr.write_idx,
             (unsigned long)s_hdr.count,
             (unsigned long)s_hdr.capacity);
    return ESP_OK;
}

/* 레코드를 링의 write_idx 에 기록. 가득찬 경우 read_idx 도 밀어서 overwrite. */
static esp_err_t ring_write_record(const agg_record_t *rec) {
    esp_err_t err = ESP_FAIL;
    if (xSemaphoreTake(s_ring_mtx, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }

    FILE *f = fopen(AGG_RING_PATH, "r+b");
    if (!f) {
        ESP_LOGE(TAG, "ring_write: fopen failed errno=%d", errno);
        goto out;
    }

    if (fseek(f, slot_offset(s_hdr.write_idx), SEEK_SET) != 0) {
        ESP_LOGE(TAG, "ring_write: seek failed");
        fclose(f);
        goto out;
    }
    size_t n = fwrite(rec, 1, sizeof(*rec), f);
    fflush(f);
    fclose(f);
    if (n != sizeof(*rec)) {
        ESP_LOGE(TAG, "ring_write: wrote %u/%u", (unsigned)n, (unsigned)sizeof(*rec));
        goto out;
    }

    s_hdr.write_idx = (s_hdr.write_idx + 1) % s_hdr.capacity;
    if (s_hdr.count < s_hdr.capacity) {
        s_hdr.count++;
    } else {
        /* 가득 찬 상태에서 쓴 것 -> 가장 오래된 하나를 덮어씀. */
        s_hdr.read_idx = (s_hdr.read_idx + 1) % s_hdr.capacity;
        ESP_LOGW(TAG, "ring FULL - overwriting oldest record");
    }
    err = hdr_write();

out:
    xSemaphoreGive(s_ring_mtx);
    /* 구독자가 붙어있으면 즉시 flush 태스크를 깨움(라이브 세션에도 기록을 전달). */
    if (err == ESP_OK && s_flush_trig && notify_state) {
        xSemaphoreGive(s_flush_trig);
    }
    return err;
}

/* read_idx 위치의 레코드를 읽음(전진은 하지 않음). */
static esp_err_t ring_peek_record(agg_record_t *rec) {
    esp_err_t err = ESP_FAIL;
    if (xSemaphoreTake(s_ring_mtx, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    if (s_hdr.count == 0) {
        xSemaphoreGive(s_ring_mtx);
        return ESP_ERR_NOT_FOUND;
    }
    FILE *f = fopen(AGG_RING_PATH, "rb");
    if (!f) goto out;
    if (fseek(f, slot_offset(s_hdr.read_idx), SEEK_SET) != 0) {
        fclose(f);
        goto out;
    }
    size_t n = fread(rec, 1, sizeof(*rec), f);
    fclose(f);
    err = (n == sizeof(*rec)) ? ESP_OK : ESP_FAIL;
out:
    xSemaphoreGive(s_ring_mtx);
    return err;
}

/* read_idx 전진(=전송 완료 처리). */
static esp_err_t ring_advance_read(void) {
    esp_err_t err = ESP_FAIL;
    if (xSemaphoreTake(s_ring_mtx, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    if (s_hdr.count == 0) {
        xSemaphoreGive(s_ring_mtx);
        return ESP_OK;
    }
    s_hdr.read_idx = (s_hdr.read_idx + 1) % s_hdr.capacity;
    s_hdr.count--;
    err = hdr_write();
    xSemaphoreGive(s_ring_mtx);
    return err;
}

#elif CONFIG_EXAMPLE_STORE_HISTORY && CONFIG_EXAMPLE_HISTORY_BACKEND_SQLITE

#define AGG_DB_PATH      "/spiffs/agg_ring.db"
#define AGG_SQLITE_CAP   ((uint32_t)CONFIG_EXAMPLE_AGG_SQLITE_MAX_SLOTS)

/* agg_sql_exec / prepare 실패 시 마지막 SQLite rc (CORRUPT 등 복구 판단용) */
static int s_agg_last_sqlite_rc = SQLITE_OK;

static esp_err_t agg_sql_exec(const char *sql)
{
    char *errm = NULL;
    int rc = sqlite3_exec(s_agg_db, sql, NULL, NULL, &errm);
    s_agg_last_sqlite_rc = rc;
    if (rc != SQLITE_OK) {
        ESP_LOGE(TAG, "SQL rc=%d: %s | %s", rc, sql, errm ? errm : "");
        if (errm) {
            sqlite3_free(errm);
        }
        return ESP_FAIL;
    }
    if (errm) {
        sqlite3_free(errm);
    }
    return ESP_OK;
}

static bool agg_sqlite_corrupt_indicated(void)
{
    if (s_agg_last_sqlite_rc == SQLITE_CORRUPT || s_agg_last_sqlite_rc == SQLITE_NOTADB) {
        return true;
    }
    if (s_agg_db) {
        int c = sqlite3_errcode(s_agg_db);
        if (c == SQLITE_CORRUPT || c == SQLITE_NOTADB) {
            return true;
        }
    }
    return false;
}

static esp_err_t agg_sqlite_close_unlink(void)
{
    if (s_agg_db) {
        sqlite3_close(s_agg_db);
        s_agg_db = NULL;
    }
    if (unlink(AGG_DB_PATH) != 0 && errno != ENOENT) {
        ESP_LOGE(TAG, "unlink(%s) failed errno=%d", AGG_DB_PATH, errno);
        return ESP_FAIL;
    }
    ESP_LOGW(TAG, "removed %s (corrupt or forced recreate)", AGG_DB_PATH);
    s_agg_last_sqlite_rc = SQLITE_OK;
    return ESP_OK;
}

static esp_err_t agg_sqlite_ensure_open(void)
{
    if (s_agg_db) {
        return ESP_OK;
    }
    sqlite3_initialize();
    int rc = sqlite3_open_v2(AGG_DB_PATH, &s_agg_db,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK) {
        ESP_LOGE(TAG, "sqlite3_open %s failed rc=%d %s", AGG_DB_PATH, rc,
                 s_agg_db ? sqlite3_errmsg(s_agg_db) : "");
        if (s_agg_db) {
            sqlite3_close(s_agg_db);
            s_agg_db = NULL;
        }
        return ESP_FAIL;
    }
    /* power_data.db 등 다른 연결과 SPIFFS 상에서 경합 시 BUSY 가 잦음 */
    sqlite3_busy_timeout(s_agg_db, 30000);
    (void)sqlite3_exec(s_agg_db, "PRAGMA journal_mode=DELETE;", NULL, NULL, NULL);
    /* FULL 대비 플래시 fsync 횟수 감소(정전 시 손상 위험은 약간 증가 — 웨어러블 히스토리 용도) */
    (void)sqlite3_exec(s_agg_db, "PRAGMA synchronous=NORMAL;", NULL, NULL, NULL);
    return ESP_OK;
}

static esp_err_t meta_sqlite_load(void)
{
    sqlite3_stmt *st = NULL;
    const char *q = "SELECT magic,version,capacity,record_size,read_idx,write_idx,count,crc32 "
                    "FROM agg_meta WHERE id=1;";
    int rc = sqlite3_prepare_v2(s_agg_db, q, -1, &st, NULL);
    s_agg_last_sqlite_rc = rc;
    if (rc != SQLITE_OK) {
        return ESP_FAIL;
    }
    rc = sqlite3_step(st);
    if (rc != SQLITE_ROW) {
        s_agg_last_sqlite_rc = rc;
        sqlite3_finalize(st);
        return ESP_ERR_NOT_FOUND;
    }
    s_hdr.magic       = (uint32_t)sqlite3_column_int64(st, 0);
    s_hdr.version     = (uint32_t)sqlite3_column_int64(st, 1);
    s_hdr.capacity    = (uint32_t)sqlite3_column_int64(st, 2);
    s_hdr.record_size = (uint32_t)sqlite3_column_int64(st, 3);
    s_hdr.read_idx    = (uint32_t)sqlite3_column_int64(st, 4);
    s_hdr.write_idx   = (uint32_t)sqlite3_column_int64(st, 5);
    s_hdr.count       = (uint32_t)sqlite3_column_int64(st, 6);
    s_hdr.crc32       = (uint32_t)sqlite3_column_int64(st, 7);
    sqlite3_finalize(st);
    return ESP_OK;
}

static esp_err_t hdr_write(void)
{
    s_hdr.crc32 = hdr_crc_calc(&s_hdr);
    sqlite3_stmt *st = NULL;
    const char *q = "UPDATE agg_meta SET magic=?,version=?,capacity=?,record_size=?,read_idx=?,write_idx=?,count=?,crc32=? WHERE id=1;";
    int rc = sqlite3_prepare_v2(s_agg_db, q, -1, &st, NULL);
    if (rc != SQLITE_OK) {
        ESP_LOGE(TAG, "hdr_write prepare rc=%d %s", rc, sqlite3_errmsg(s_agg_db));
        return ESP_FAIL;
    }
    sqlite3_bind_int64(st, 1, (sqlite3_int64)s_hdr.magic);
    sqlite3_bind_int64(st, 2, (sqlite3_int64)s_hdr.version);
    sqlite3_bind_int64(st, 3, (sqlite3_int64)s_hdr.capacity);
    sqlite3_bind_int64(st, 4, (sqlite3_int64)s_hdr.record_size);
    sqlite3_bind_int64(st, 5, (sqlite3_int64)s_hdr.read_idx);
    sqlite3_bind_int64(st, 6, (sqlite3_int64)s_hdr.write_idx);
    sqlite3_bind_int64(st, 7, (sqlite3_int64)s_hdr.count);
    sqlite3_bind_int64(st, 8, (sqlite3_int64)s_hdr.crc32);
    rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) {
        ESP_LOGE(TAG, "hdr_write step rc=%d %s", rc, sqlite3_errmsg(s_agg_db));
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t ring_create(uint32_t bytes)
{
    (void)bytes;
    if (agg_sql_exec("DELETE FROM agg_slots;") != ESP_OK) {
        return ESP_FAIL;
    }
    if (agg_sql_exec("DELETE FROM agg_meta;") != ESP_OK) {
        return ESP_FAIL;
    }
    memset(&s_hdr, 0, sizeof(s_hdr));
    s_hdr.magic       = AGG_MAGIC;
    s_hdr.version     = AGG_VERSION;
    s_hdr.capacity    = AGG_SQLITE_CAP;
    s_hdr.record_size = sizeof(agg_record_t);
    s_hdr.read_idx    = 0;
    s_hdr.write_idx   = 0;
    s_hdr.count       = 0;
    s_hdr.crc32       = hdr_crc_calc(&s_hdr);

    char buf[288];
    snprintf(buf, sizeof(buf),
             "INSERT INTO agg_meta(id,magic,version,capacity,record_size,read_idx,write_idx,count,crc32) "
             "VALUES(1,%lu,%lu,%lu,%lu,0,0,0,%lu);",
             (unsigned long)s_hdr.magic, (unsigned long)s_hdr.version,
             (unsigned long)s_hdr.capacity, (unsigned long)s_hdr.record_size,
             (unsigned long)s_hdr.crc32);
    if (agg_sql_exec(buf) != ESP_OK) {
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "ring(SQLite) created: capacity=%lu (menuconfig max_slots)",
             (unsigned long)s_hdr.capacity);
    return ESP_OK;
}

static esp_err_t ring_open_or_create_once(void)
{
    if (agg_sqlite_ensure_open() != ESP_OK) {
        return ESP_FAIL;
    }
    const char *c1 = "CREATE TABLE IF NOT EXISTS agg_meta(id INTEGER PRIMARY KEY CHECK(id=1),"
                     "magic INTEGER,version INTEGER,capacity INTEGER,record_size INTEGER,"
                     "read_idx INTEGER,write_idx INTEGER,count INTEGER,crc32 INTEGER);";
    const char *c2 = "CREATE TABLE IF NOT EXISTS agg_slots(slot_idx INTEGER PRIMARY KEY,"
                     "ts_epoch INTEGER,id TEXT,avg REAL);";
    if (agg_sql_exec(c1) != ESP_OK || agg_sql_exec(c2) != ESP_OK) {
        return ESP_FAIL;
    }

    bool need_create = false;
    if (meta_sqlite_load() != ESP_OK) {
        need_create = true;
    } else if (s_hdr.magic != AGG_MAGIC
               || s_hdr.version != AGG_VERSION
               || s_hdr.record_size != sizeof(agg_record_t)
               || s_hdr.capacity != AGG_SQLITE_CAP
               || s_hdr.crc32 != hdr_crc_calc(&s_hdr)
               || s_hdr.read_idx >= s_hdr.capacity
               || s_hdr.write_idx >= s_hdr.capacity
               || s_hdr.count > s_hdr.capacity) {
        ESP_LOGW(TAG, "ring(SQLite) meta invalid -> recreate");
        need_create = true;
    }

    if (need_create) {
        return ring_create(AGG_RING_BYTES);
    }
    ESP_LOGD(TAG, "ring(SQLite) opened: read=%lu write=%lu pending=%lu cap=%lu",
             (unsigned long)s_hdr.read_idx,
             (unsigned long)s_hdr.write_idx,
             (unsigned long)s_hdr.count,
             (unsigned long)s_hdr.capacity);
    return ESP_OK;
}

static esp_err_t ring_open_or_create(void)
{
    for (unsigned pass = 0; pass < 2u; ++pass) {
        esp_err_t e = ring_open_or_create_once();
        if (e == ESP_OK) {
            return ESP_OK;
        }
        if (pass == 0 && agg_sqlite_corrupt_indicated()) {
            ESP_LOGW(TAG, "agg_ring SQLite damaged -> unlink and retry once");
            if (agg_sqlite_close_unlink() != ESP_OK) {
                return e;
            }
            continue;
        }
        return e;
    }
    return ESP_FAIL;
}

/* MQTT 연결 중일 때만 짧게 블록: TCP/IP·mqtt 태스크에 스케줄 슬롯 확보(힙 단편화 완화 시간). */
static void agg_coop_before_finalize_disk(void)
{
    if (flag_mqtt_connect) {
        vTaskDelay(pdMS_TO_TICKS(AGG_FINALIZE_COOP_MS));
    } else {
        taskYIELD();
    }
}

/* 5분 창 종료 시 센서별 레코드를 한 트랜잭션으로 묶어 SPIFFS/SQLite 부하·락 시간을 줄임 */
static esp_err_t sqlite_ring_write_batch(const agg_record_t *recs, int n)
{
    if (!recs || n <= 0) {
        return ESP_OK;
    }
    esp_err_t err = ESP_FAIL;
    bool in_tx = false;
    agg_header_t hdr_saved;
    sqlite3_stmt *st = NULL;

    if (xSemaphoreTake(s_ring_mtx, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }

    hdr_saved = s_hdr;

    if (agg_sql_exec("BEGIN IMMEDIATE;") != ESP_OK) {
        ESP_LOGE(TAG, "ring_write_batch BEGIN: %s", sqlite3_errmsg(s_agg_db));
        goto out;
    }
    in_tx = true;

    const char *ins = "REPLACE INTO agg_slots(slot_idx,ts_epoch,id,avg) VALUES(?,?,?,?);";
    int rc = sqlite3_prepare_v2(s_agg_db, ins, -1, &st, NULL);
    if (rc != SQLITE_OK) {
        ESP_LOGE(TAG, "ring_write_batch prepare rc=%d %s", rc, sqlite3_errmsg(s_agg_db));
        goto rollback;
    }

    agg_header_t h = hdr_saved;
    bool logged_full = false;
    for (int k = 0; k < n; k++) {
        const agg_record_t *rec = &recs[k];
        sqlite3_reset(st);
        sqlite3_clear_bindings(st);
        sqlite3_bind_int64(st, 1, (sqlite3_int64)h.write_idx);
        sqlite3_bind_int64(st, 2, (sqlite3_int64)rec->ts_epoch);
        sqlite3_bind_text(st, 3, rec->id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(st, 4, (double)rec->avg);
        rc = sqlite3_step(st);
        if (rc != SQLITE_DONE) {
            ESP_LOGE(TAG, "ring_write_batch step k=%d rc=%d %s", k, rc, sqlite3_errmsg(s_agg_db));
            goto rollback;
        }
        if (flag_mqtt_connect && AGG_SQLITE_STEP_YIELD_INTERVAL > 0
            && (k + 1) % AGG_SQLITE_STEP_YIELD_INTERVAL == 0) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        h.write_idx = (h.write_idx + 1) % h.capacity;
        if (h.count < h.capacity) {
            h.count++;
        } else {
            h.read_idx = (h.read_idx + 1) % h.capacity;
            if (!logged_full) {
                ESP_LOGW(TAG, "ring FULL - overwriting oldest record(s)");
                logged_full = true;
            }
        }
    }
    sqlite3_finalize(st);
    st = NULL;

    s_hdr = h;
    err = hdr_write();
    if (err != ESP_OK) {
        s_hdr = hdr_saved;
        goto rollback;
    }

    if (agg_sql_exec("COMMIT;") != ESP_OK) {
        ESP_LOGE(TAG, "ring_write_batch COMMIT: %s", sqlite3_errmsg(s_agg_db));
        s_hdr = hdr_saved;
        goto rollback;
    }
    in_tx = false;
    err = ESP_OK;
    goto out;

rollback:
    if (st) {
        sqlite3_finalize(st);
        st = NULL;
    }
    if (in_tx) {
        (void)agg_sql_exec("ROLLBACK;");
        in_tx = false;
    }

out:
    xSemaphoreGive(s_ring_mtx);
    if (err == ESP_OK && s_flush_trig && notify_state) {
        xSemaphoreGive(s_flush_trig);
    }
    return err;
}

static esp_err_t ring_peek_record(agg_record_t *rec)
{
    esp_err_t err = ESP_FAIL;
    if (xSemaphoreTake(s_ring_mtx, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    if (s_hdr.count == 0) {
        xSemaphoreGive(s_ring_mtx);
        return ESP_ERR_NOT_FOUND;
    }

    sqlite3_stmt *st = NULL;
    const char *q = "SELECT ts_epoch,id,avg FROM agg_slots WHERE slot_idx=?;";
    int rc = sqlite3_prepare_v2(s_agg_db, q, -1, &st, NULL);
    if (rc != SQLITE_OK) {
        goto out;
    }
    sqlite3_bind_int(st, 1, (int)s_hdr.read_idx);
    rc = sqlite3_step(st);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(st);
        if (rc == SQLITE_DONE) {
            memset(rec, 0, sizeof(*rec));
            err = ESP_OK;
        } else {
            err = ESP_FAIL;
        }
        goto out;
    }
    memset(rec, 0, sizeof(*rec));
    rec->ts_epoch = (uint32_t)sqlite3_column_int64(st, 0);
    const char *cid = (const char *)sqlite3_column_text(st, 1);
    if (cid) {
        strncpy(rec->id, cid, AGG_FIELD_ID_LEN - 1);
        rec->id[AGG_FIELD_ID_LEN - 1] = '\0';
    }
    rec->avg = (float)sqlite3_column_double(st, 2);
    sqlite3_finalize(st);
    err = ESP_OK;
out:
    xSemaphoreGive(s_ring_mtx);
    return err;
}

static esp_err_t ring_advance_read(void)
{
    esp_err_t err = ESP_FAIL;
    if (xSemaphoreTake(s_ring_mtx, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    if (s_hdr.count == 0) {
        xSemaphoreGive(s_ring_mtx);
        return ESP_OK;
    }
    s_hdr.read_idx = (s_hdr.read_idx + 1) % s_hdr.capacity;
    s_hdr.count--;
    err = hdr_write();
    xSemaphoreGive(s_ring_mtx);
    return err;
}

#elif !CONFIG_EXAMPLE_STORE_HISTORY

static esp_err_t hdr_write(void)
{
    return ESP_FAIL;
}
static esp_err_t ring_create(uint32_t bytes)
{
    (void)bytes;
    return ESP_FAIL;
}
static esp_err_t ring_open_or_create(void)
{
    return ESP_FAIL;
}
static esp_err_t ring_write_record(const agg_record_t *rec)
{
    (void)rec;
    return ESP_FAIL;
}
static esp_err_t ring_peek_record(agg_record_t *rec)
{
    (void)rec;
    return ESP_FAIL;
}
static esp_err_t ring_advance_read(void)
{
    return ESP_FAIL;
}

#else
#error "Enable CONFIG_EXAMPLE_STORE_HISTORY and pick FAT or SQLite in menuconfig"
#endif

/* ====================== 최신값 테이블 ====================== */

static int latest_find_or_alloc(const char *id) {
    int first_empty = -1;
    for (int i = 0; i < AGG_MAX_FIELDS; ++i) {
        if (s_latest[i].valid) {
            if (strncmp(s_latest[i].id, id, AGG_FIELD_ID_LEN) == 0) {
                return i;
            }
        } else if (first_empty < 0) {
            first_empty = i;
        }
    }
    if (first_empty >= 0) {
        strncpy(s_latest[first_empty].id, id, AGG_FIELD_ID_LEN - 1);
        s_latest[first_empty].id[AGG_FIELD_ID_LEN - 1] = '\0';
        s_latest[first_empty].valid = true;
    }
    return first_empty;
}

static int window_find_or_alloc(const char *id);
static void window_try_fill_locked(const char *id, float value);
static void window_reset(void);

void agg_latest_update(const char *id, float value) {
    if (!s_initialized || !id || !*id) return;
    /* Static 보드에서는 히스토리를 저장/전송하지 않는다.
     * (init 단계에서 이미 리소스를 잡지 않지만, 방어적으로 한 번 더 확인.) */
    if (flag_IS_WEARABLE != 1) return;

    /* 센서 태스크에서 매우 짧게 경합할 수 있으므로 약간의 대기(50ms) 허용.
     * 0-tick(=즉시 실패) 로 두면 경합 시 갱신이 통째로 사라져 "센서가 조용한"
     * 것처럼 보이는 증상이 생긴다. 진단을 위해 드롭 카운터도 증가. */
    if (xSemaphoreTake(s_latest_mtx, pdMS_TO_TICKS(50)) != pdTRUE) {
        s_stat_drops++;
        return;
    }
    int idx = latest_find_or_alloc(id);
    if (idx >= 0) {
        bool first = !(s_latest[idx].latest != 0.0f || s_latest[idx].last_update_us != 0);
        s_latest[idx].latest = value;
        s_latest[idx].last_update_us = esp_timer_get_time();
        s_stat_updates++;
        if (first) {
            /* 각 센서 ID 가 처음 등록될 때 한 번 로그 — 배선 확인용. */
            ESP_LOGD(TAG, "latest register: id=\"%s\" (first value=%.3f)", id, value);
        }
        /* BLE 미구독(오프라인 저장) 구간: 5분 창에 센서별 평균 누적. */
        if (flag_IS_WEARABLE == 1) {
            window_try_fill_locked(id, value);
        }
    }
    xSemaphoreGive(s_latest_mtx);
}

/* 오프라인 5분 창 테이블: id 슬롯 반환(없으면 할당). 호출자가 s_latest_mtx 보유. */
static int window_find_or_alloc(const char *id) {
    int first_empty = -1;
    for (int i = 0; i < AGG_MAX_FIELDS; ++i) {
        if (s_window[i].id[0] != '\0') {
            if (strncmp(s_window[i].id, id, AGG_FIELD_ID_LEN) == 0) {
                return i;
            }
        } else if (first_empty < 0) {
            first_empty = i;
        }
    }
    if (first_empty >= 0) {
        memset(&s_window[first_empty], 0, sizeof(s_window[first_empty]));
        strncpy(s_window[first_empty].id, id, AGG_FIELD_ID_LEN - 1);
        s_window[first_empty].id[AGG_FIELD_ID_LEN - 1] = '\0';
    }
    return first_empty;
}

/* BLE 오프라인 창에서 해당 id 의 샘플을 평균 누적에 반영. */
static void window_try_fill_locked(const char *id, float value) {
    if (notify_state) {
        return;
    }
    int wi = window_find_or_alloc(id);
    if (wi < 0) {
        return;
    }
    s_window[wi].sum += (double)value;
    s_window[wi].n++;
}

static void window_reset(void) {
    memset(s_window, 0, sizeof(s_window));
}

/* finalize_and_write_record() 가 snapshot_task 에서만 호출 → BSS 로 두어 agg_snap 스택 여유 확보 */
static agg_record_t s_finalize_win_batch[AGG_MAX_FIELDS];

/* 현재 5분 창(센서별 산술 평균)을 '센서 1개당 레코드 1건'으로 링버퍼에 기록.
 * 같은 5분 구간의 레코드들은 ts_epoch 이 동일 → 앱이 ts 로 그룹화. */
static void finalize_and_write_record(void) {
    time_t now_sec = time(NULL);
    uint32_t ts = (now_sec > 1700000000) ? (uint32_t)now_sec : 0; /* 2023-11 이후만 유효로 간주. */

    int active  = 0;   /* n>0 인 슬롯 수 */
    int idle    = 0;   /* id 는 있는데 n==0 (비정상) */
    int bn = 0;

    if (xSemaphoreTake(s_latest_mtx, pdMS_TO_TICKS(2000)) != pdTRUE) {
        ESP_LOGW(TAG, "finalize: latest_mtx timeout — skip write");
        return;
    }
    for (int i = 0; i < AGG_MAX_FIELDS; ++i) {
        if (s_window[i].id[0] == '\0') {
            continue;
        }
        if (s_window[i].n == 0) {
            idle++;
            continue;
        }
        active++;

        agg_record_t rec;
        memset(&rec, 0, sizeof(rec));
        rec.ts_epoch = ts;
        strncpy(rec.id, s_window[i].id, AGG_FIELD_ID_LEN - 1);
        rec.id[AGG_FIELD_ID_LEN - 1] = '\0';
        rec.avg = (float)(s_window[i].sum / (double)s_window[i].n);

        if (bn < AGG_MAX_FIELDS) {
            s_finalize_win_batch[bn++] = rec;
        }
    }
    window_reset();
    xSemaphoreGive(s_latest_mtx);

    int written = 0;
    int failed  = 0;

#if CONFIG_EXAMPLE_STORE_HISTORY && CONFIG_EXAMPLE_HISTORY_BACKEND_SQLITE
    if (bn > 0) {
        agg_coop_before_finalize_disk();
        if (sqlite_ring_write_batch(s_finalize_win_batch, bn) == ESP_OK) {
            written = bn;
            s_stat_writes += (uint32_t)bn;
        } else {
            failed = bn;
            ESP_LOGE(TAG, "sqlite_ring_write_batch failed (%d sensors)", bn);
        }
    }
#else
    for (int j = 0; j < bn; ++j) {
        if (j == 0) {
            agg_coop_before_finalize_disk();
        } else if (flag_mqtt_connect && AGG_FILE_WRITE_INTER_MS > 0) {
            vTaskDelay(pdMS_TO_TICKS(AGG_FILE_WRITE_INTER_MS));
        }
        if (ring_write_record(&s_finalize_win_batch[j]) == ESP_OK) {
            written++;
            s_stat_writes++;
        } else {
            failed++;
            ESP_LOGE(TAG, "ring_write_record failed id=%s", s_finalize_win_batch[j].id);
        }
    }
#endif

    if (written == 0 && failed == 0) {
        /* 진단을 돕기 위해 왜 비었는지 같이 찍는다. */
        ESP_LOGW(TAG, "no samples in window (active=0, idle=%d) — sensors not feeding agg_latest_update?"
                      " updates=%lu drops=%lu stale=%lu",
                 idle,
                 (unsigned long)s_stat_updates,
                 (unsigned long)s_stat_drops,
                 (unsigned long)s_stat_stale);
        return;
    }
    ESP_LOGD(TAG, "wrote records ts=%lu ok=%d fail=%d pending=%lu (active=%d idle=%d)",
             (unsigned long)ts, written, failed, (unsigned long)s_hdr.count, active, idle);
}

/* ====================== snapshot_task ====================== */

static void snapshot_task(void *arg) {
    (void)arg;
    ESP_LOGD(TAG, "snapshot_task: 5min window, per-sensor mean (poll=%ds, chunks=%u, wearable, BLE-offline-only)",
             AGG_SNAPSHOT_PERIOD_S, (unsigned)AGG_SNAPSHOTS_PER_REC);

    bool prev_online = false;

    while (1) {
        /* Wearable 전용. Static 보드에서는 task 를 아예 만들지 않지만
         * 런타임에 flag 가 바뀌는 극단적인 경우에도 저장을 막는다. */
        if (flag_IS_WEARABLE != 1) {
            if (xSemaphoreTake(s_latest_mtx, pdMS_TO_TICKS(200)) == pdTRUE) {
                window_reset();
                xSemaphoreGive(s_latest_mtx);
            }
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }

        bool online_now = notify_state;

        if (online_now) {
            if (!prev_online) {
                if (xSemaphoreTake(s_latest_mtx, pdMS_TO_TICKS(1000)) == pdTRUE) {
                    window_reset();
                    xSemaphoreGive(s_latest_mtx);
                }
                ESP_LOGD(TAG, "BLE online: offline window cleared (no ring write)");
            }
            prev_online = true;
            vTaskDelay(pdMS_TO_TICKS(AGG_SNAPSHOT_PERIOD_S * 1000));
            continue;
        }

        if (prev_online) {
            if (xSemaphoreTake(s_latest_mtx, pdMS_TO_TICKS(1000)) == pdTRUE) {
                window_reset();
                xSemaphoreGive(s_latest_mtx);
            }
            ESP_LOGD(TAG, "BLE offline: new 5min window (per-id mean accumulation)");
        }
        prev_online = false;

        /* 300초 = N × poll: 중간에 구독 ON 되면 창 폐기. */
        for (uint32_t ch = 0; ch < AGG_SNAPSHOTS_PER_REC; ch++) {
            vTaskDelay(pdMS_TO_TICKS(AGG_SNAPSHOT_PERIOD_S * 1000));
            if (flag_IS_WEARABLE != 1) {
                break;
            }
            if (notify_state) {
                break;
            }
        }

        if (flag_IS_WEARABLE != 1) {
            continue;
        }
        if (notify_state) {
            prev_online = true;
            if (xSemaphoreTake(s_latest_mtx, pdMS_TO_TICKS(1000)) == pdTRUE) {
                window_reset();
                xSemaphoreGive(s_latest_mtx);
            }
            ESP_LOGD(TAG, "BLE went online during 5min window — partial window discarded");
            continue;
        }

        s_stat_ticks++;
        finalize_and_write_record();
    }
}

/* ====================== flush_task ====================== */

/* "H,<ts>,<id>,<val>" 한 번의 notify. 같은 ts 끼리는 앱이 그룹화한다. */
static int history_send_one(uint32_t ts, const char *id, float val) {
    char buf[64];
    snprintf(buf, sizeof(buf), "H,%lu,%s,%.3f", (unsigned long)ts, id, val);
    return ble_send_raw(buf);
}

/* 이벤트-주도 flush_task:
 *  - 평소엔 세마포어로 대기하여 CPU/스택 소모 없음.
 *  - agg_buffer_kick_flush() 또는 ring_write_record 성공 시 깨어남.
 *  - 깨어나면 notify_state 가 유지되는 동안 pending 레코드(센서 1건)를
 *    하나씩 전송하고, 성공하면 read_idx 를 전진시킨다.
 *  - 전송 실패/구독 해제 시엔 짧게 대기 후 재확인(재시도용).
 *
 * s_flushing 은 이 함수가 "지금 히스토리를 내보내는 중" 임을 외부(gatt_svc.c)
 * 에 알린다. 라이브 notify 는 이 플래그가 켜진 동안 전송을 보류하여, 앱이
 * "저장된 히스토리 → 그 다음 라이브" 순서로 수신하게 된다. */
static void flush_task(void *arg) {
    (void)arg;
    ESP_LOGD(TAG, "flush_task started (event-driven, history-first policy)");

    while (1) {
        /* 무기한 대기 — kick 이 들어올 때만 깨어남. */
        if (xSemaphoreTake(s_flush_trig, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        /* 히스토리가 있는 경우에만 "flushing" 모드로 진입.
         * (pending 이 0 이면 라이브를 막을 이유가 없다.) */
        if (notify_state && s_hdr.count > 0) {
            s_flushing = true;
            ESP_LOGD(TAG, "flush start: pending=%lu (live notify paused)",
                     (unsigned long)s_hdr.count);
        }

        /* pending 이 모두 비거나 구독이 끊길 때까지 연속 플러시. */
        while (notify_state && s_hdr.count > 0) {
            agg_record_t rec;
            esp_err_t err = ring_peek_record(&rec);
            if (err != ESP_OK) break;

            if (rec.id[0] == '\0') {
                /* 손상 or 빈 레코드: 건너뛰기 위해 전진만. */
                ring_advance_read();
                continue;
            }

            int rc = history_send_one(rec.ts_epoch, rec.id, rec.avg);

            if (!notify_state) {
                ESP_LOGW(TAG, "BLE unsubscribed during flush, will retry later");
                break;
            }
            if (rc != 0) {
                ESP_LOGW(TAG, "history notify failed rc=%d, retry later", rc);
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
            }

            ring_advance_read();
            /* 다음 레코드까지 짧은 간격(BLE ACK / 스택 여유). 라이브는 아직 보류. */
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        /* flush 종료 상태 정리.
         *  - pending == 0: 히스토리 전송 완료 → 라이브 재개.
         *  - notify_state == false: 구독 해제됨 → 어차피 라이브도 못 나감.
         *  - send 실패 break: 다음 kick(=재구독 또는 다음 record 기록)에서 재시도.
         * 어느 경우든 플래그는 내려 라이브를 불필요하게 막지 않는다. */
        if (s_flushing) {
            s_flushing = false;
            ESP_LOGD(TAG, "flush end: pending=%lu (live notify resumed)",
                     (unsigned long)s_hdr.count);
        }
    }
}

/* BLE notify 구독이 방금 활성화됐을 때 외부에서 호출. */
void agg_buffer_kick_flush(void) {
    if (!s_initialized || !s_flush_trig) return;
    /* Static 보드에서는 flush 할 것이 없으므로 ignore. */
    if (flag_IS_WEARABLE != 1) return;
    xSemaphoreGive(s_flush_trig);
}

bool agg_buffer_is_flushing(void) {
    /* init 전이거나 wearable 이 아니면 flush 개념 자체가 없다 → false. */
    if (!s_initialized || flag_IS_WEARABLE != 1) return false;
    return s_flushing;
}

/* ====================== 공개 API ====================== */

bool agg_buffer_is_initialized(void) {
    return s_initialized;
}

esp_err_t agg_buffer_get_last_init_error(void)
{
    return s_last_init_err;
}

esp_err_t agg_buffer_init(void) {
    if (s_initialized) {
        s_last_init_err = ESP_OK;
        return ESP_OK;
    }

    /* Wearable 전용 기능. Static 보드에서는 링 파일도, 태스크도 만들지 않는다.
     * flag_IS_WEARABLE 는 stella_early_mux_and_refresh_device_id() 에서
     * app_main_stella() 보다 훨씬 이전에 확정되므로 이 시점에 이미 유효하다. */
    if (flag_IS_WEARABLE != 1) {
        ESP_LOGW(TAG, "static board (flag_IS_WEARABLE=%d): history buffering disabled",
                 flag_IS_WEARABLE);
        /* 이전 부팅/실패 흔적이 s_hdr 에 남아 agg_stats 가 오해를 일으키지 않도록 비운다. */
        memset(&s_hdr, 0, sizeof(s_hdr));
        memset(s_latest, 0, sizeof(s_latest));
        memset(s_window, 0, sizeof(s_window));
        s_stat_updates = s_stat_drops = s_stat_ticks = s_stat_stale = s_stat_writes = 0;
        s_initialized = false;
        /* ESP_OK 가 아니어야 상위(stella_main)가 "집계 모듈 가동 완료"로 오인하지 않는다. */
        s_last_init_err = ESP_ERR_NOT_SUPPORTED;
        return ESP_ERR_NOT_SUPPORTED;
    }

    s_latest_mtx = xSemaphoreCreateMutex();
    s_ring_mtx   = xSemaphoreCreateMutex();
    /* Binary semaphore: give/take 방식의 이벤트 트리거. 생성 직후 take 가능 상태이므로
     * 아래에서 초기 상태를 0(빈 상태)으로 맞추기 위해 한 번 take 한다. */
    s_flush_trig = xSemaphoreCreateBinary();
    if (!s_latest_mtx || !s_ring_mtx || !s_flush_trig) {
        ESP_LOGE(TAG, "semaphore create failed (free_heap=%u)",
                 (unsigned)esp_get_free_heap_size());
        s_last_init_err = ESP_ERR_NO_MEM;
        agg_abort_init_cleanup();
        return ESP_ERR_NO_MEM;
    }

    memset(s_latest, 0, sizeof(s_latest));
    memset(s_window, 0, sizeof(s_window));

    esp_err_t err = ring_open_or_create();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ring_open_or_create failed: %s", esp_err_to_name(err));
        s_last_init_err = err;
        agg_abort_init_cleanup();
        return err;
    }

    TaskHandle_t snap_h = NULL;
    TaskHandle_t flush_h = NULL;
    BaseType_t ok;
    /* snapshot_task: vTaskDelay 루프 + finalize → SQLite 가 스택을 많이 씀(8KB 필요).
     * 스택을 내부 SRAM에 두면 WiFi/MQTT와 internal heap 경쟁 → STA 끊김 유발 가능.
     * mesh_main 의 mqtt_wear 와 동일하게 PSRAM 스택으로 분리. */
    const uint32_t caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    ok = xTaskCreateWithCaps(snapshot_task, "agg_snap", 8192, NULL, 2, &snap_h, caps);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "snapshot_task create failed (free_heap=%u, try PSRAM caps)",
                 (unsigned)esp_get_free_heap_size());
        s_last_init_err = ESP_ERR_NO_MEM;
        agg_abort_init_cleanup();
        return ESP_ERR_NO_MEM;
    }

    ok = xTaskCreateWithCaps(flush_task, "agg_flush", 3072, NULL, 2, &flush_h, caps);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "flush_task create failed (free_heap=%u)",
                 (unsigned)esp_get_free_heap_size());
        if (snap_h) {
            vTaskDelete(snap_h);
        }
        s_last_init_err = ESP_ERR_NO_MEM;
        agg_abort_init_cleanup();
        return ESP_ERR_NO_MEM;
    }

    /* 태스크까지 모두 성공한 뒤에만 true — 부분 성공 상태에서 agg_stats 가
     * "capacity=52425 인데 enabled=NO" 처럼 보이는 문제를 없앤다. */
    s_initialized = true;

    /* 부팅 시 기존 pending 이 있고 이미 구독자가 붙은 상태라면 즉시 플러시. */
    if (s_hdr.count > 0) {
        xSemaphoreGive(s_flush_trig);
    }

    ESP_LOGI(TAG, "agg init cap=%lu pending=%lu",
             (unsigned long)s_hdr.capacity, (unsigned long)s_hdr.count);
    s_last_init_err = ESP_OK;
    return ESP_OK;
}

void agg_buffer_get_stats(uint32_t *pending, uint32_t *capacity,
                          uint32_t *read_idx, uint32_t *write_idx) {
    /* init 이 끝나기 전엔 s_hdr 이 유효하지 않을 수 있다(예: 태스크 생성 실패 직후).
     * 콘솔에 유효하지 않은 capacity 가 찍히면 사용자가 오해하므로 미초기화 시 전부 0. */
    if (!s_initialized) {
        if (pending)   *pending   = 0;
        if (capacity)  *capacity  = 0;
        if (read_idx)  *read_idx  = 0;
        if (write_idx) *write_idx = 0;
        return;
    }
    if (pending)   *pending   = s_hdr.count;
    if (capacity)  *capacity  = s_hdr.capacity;
    if (read_idx)  *read_idx  = s_hdr.read_idx;
    if (write_idx) *write_idx = s_hdr.write_idx;
}

void agg_buffer_get_diag(agg_diag_t *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->init_done  = s_initialized;
    out->enabled    = s_initialized && (flag_IS_WEARABLE == 1);
    out->flushing   = s_flushing;
    out->updates    = s_stat_updates;
    out->drops      = s_stat_drops;
    out->ticks      = s_stat_ticks;
    out->stale      = s_stat_stale;
    out->writes     = s_stat_writes;

    uint32_t registered = 0;
    for (int i = 0; i < AGG_MAX_FIELDS; ++i) {
        if (s_latest[i].valid) registered++;
    }
    out->registered_ids = registered;

    uint32_t with_samples = 0;
    if (s_latest_mtx && xSemaphoreTake(s_latest_mtx, pdMS_TO_TICKS(50)) == pdTRUE) {
        for (int i = 0; i < AGG_MAX_FIELDS; ++i) {
            if (s_window[i].n > 0) {
                with_samples++;
            }
        }
        xSemaphoreGive(s_latest_mtx);
    }
    out->snap_count = with_samples;
}

void agg_buffer_reset(void) {
    if (!s_initialized || !s_ring_mtx) return;
    if (xSemaphoreTake(s_ring_mtx, portMAX_DELAY) != pdTRUE) return;
    ring_create(AGG_RING_BYTES);
    xSemaphoreGive(s_ring_mtx);
    if (s_latest_mtx && xSemaphoreTake(s_latest_mtx, pdMS_TO_TICKS(1000)) == pdTRUE) {
        window_reset();
        xSemaphoreGive(s_latest_mtx);
    }
}
