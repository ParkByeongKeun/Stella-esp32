# BLE 히스토리(오프라인 집계) 프로토콜 — 안드로이드 연동

이 문서는 펌웨어의 `agg_buffer` 모듈이 BLE Android 앱이 연결되지 않은 동안
저장해 두었다가, BLE 구독이 활성화되면 전송하는 **5분 평균 히스토리** 전송
규격을 설명합니다.

> **디바이스 대상**: 본 기능은 **Wearable 보드(`flag_IS_WEARABLE=1`) 전용**
> 입니다. Static 보드에서는 히스토리 저장/전송이 **비활성**되며 기존처럼
> 라이브(`<id>,<value>`) 만 전송합니다.
>
> **전송 순서(중요)**: BLE 연결·구독 직후 펌웨어는
> **① 저장된 히스토리를 모두 전송 → ② 라이브(실시간) 전송 재개**
> 순서로 동작합니다. 히스토리 전송 중에는 라이브 notify 가 잠시 보류되므로
> 앱은 "먼저 히스토리가 쭉 들어오고, 끝나면 라이브가 시작"되는 패턴을 볼 수
> 있습니다. (로그 예: `flush start` → `flush end`)

---

## 1. 전송 채널

- 서비스: 기존과 동일 (Heart Rate Service, UUID `0x4001`).
- Characteristic: 기존 `heart_rate_chr`(UUID `0x4002`) **한 개**를 재사용합니다.
- 전송 방식: `Notify` (기존과 동일).

즉 **라이브 데이터와 히스토리 데이터가 동일한 Notification 채널을 통해 섞여서
도착**합니다. 앱은 수신 문자열의 첫 토큰을 보고 분기하면 됩니다.

---

## 2. 수신 문자열 포맷

모든 notification 은 ASCII 문자열(기존처럼)이며, 콤마(`,`) 로 구분됩니다.

### 2.1 라이브 (기존과 동일 · 변경 없음)

```
<id>,<value>
```

예시:
```
CO2,612
PM2.5,34
Temperature,24.8
Humidity,45
PDM_avg,41.3
TVOC,42
CH2O,31
```

### 2.2 히스토리 (한 센서 = notify 1회)

```
H,<unix_ts_sec>,<id>,<value>
```

- `H`: 히스토리 구분자 (고정).
- `<unix_ts_sec>`: 5분 집계 구간이 종료된 시각. Unix epoch(초). **0 이면 당시
  SNTP 가 아직 동기화되지 않은 상태의 데이터입니다. 앱은 0 인 레코드도 표시하되
  "시간 미상" 처리하는 것을 권장.**
- `<id>`: 센서 식별자 (`CO2`, `PM2.5`, `Temperature`, …). 라이브와 동일.
- `<value>`: 해당 5분 구간의 **산술 평균값**. 소수 3자리 문자열
  (`"%.3f"` 포맷, 예: `612.133`).

**핵심 규칙**
- 라이브와 동일하게 **“한 센서 = 한 줄(notify 1개)”** 구조입니다.
- 같은 5분 구간에서 나온 센서들은 **모두 동일한 `ts`** 를 공유합니다.
- 종료 마커(`HE`)는 **없습니다**. 앱은 `ts` 가 바뀌는 순간(또는 타임아웃) 직전
  `ts` 그룹을 "완성"으로 처리하세요.

예시(같은 5분 구간의 센서들이 연달아 도착):
```
H,1713700800,CO2,612.133
H,1713700800,PM2.5,34.266
H,1713700800,Temperature,24.850
H,1713700800,Humidity,45.100
H,1713700800,PDM_avg,41.325
H,1713701100,CO2,618.200        ← 다음 5분 구간(ts 가 바뀜)
H,1713701100,PM2.5,33.900
...
```

---

## 3. 수신 순서 (history-first)

연결·구독 직후 펌웨어는 **저장된 히스토리를 먼저 모두 전송하고, 그 다음 라이브
전송을 재개**합니다. 히스토리 전송 중에는 라이브 notify 가 보류되므로 보통의
수신 패턴은 아래와 같습니다.

```
(구독 ON)
H,1713700500,CO2,610.000         ← 히스토리 시작 (가장 오래된 것부터)
H,1713700500,PM2.5,33.900
H,1713700500,Temperature,24.700
H,1713700800,CO2,612.133         ← 다음 5분 구간 (ts 변경)
H,1713700800,PM2.5,34.266
H,1713700800,Temperature,24.850
... (pending 전부 전송) ...
(flush 종료 — 라이브 재개)
CO2,615                          ← 이제부터 라이브
PM2.5,35
Temperature,24.9
CO2,616
...
```

- **히스토리는 항상 오래된 것부터 순서대로** 전송됩니다 (FIFO).
- 같은 5분 구간의 센서들은 `ts` 가 같고, `ts` 가 바뀌는 시점이 그룹 경계입니다.
- 전송 중 BLE 가 끊기면 마지막에 진행 중이던 레코드는 다음 재연결 시 **다시**
  전송될 수 있습니다. 앱은 `(ts, id)` 유니크 키로 UPSERT 하여 중복 처리하세요.
- 드문 경우(예: flush 중 펌웨어 내부 타임아웃)에 라이브가 한두 개 끼어들 가능성
  이 0 은 아니므로, 파싱 로직은 여전히 첫 토큰(`H` vs 그 외)으로 분기해야
  안전합니다.

---

## 4. 파싱 의사코드 (안드로이드)

```kotlin
fun onNotification(payload: String) {
    val parts = payload.split(",")
    when (parts[0]) {
        "H" -> {
            // 히스토리: H, ts, id, value
            if (parts.size < 4) return
            val ts = parts[1].toLongOrNull() ?: return
            val id = parts[2]
            val value = parts[3].toFloatOrNull() ?: return
            // (ts, id) 를 유니크 키로 UPSERT. 재전송/중복 안전.
            saveHistoryPoint(ts, id, value)
        }
        else -> {
            // 라이브: id, value
            if (parts.size < 2) return
            val id = parts[0]
            val value = parts[1]
            updateLiveUi(id, value)
        }
    }
}
```

주요 고려 사항
- 더 이상 `HE` 종료 마커가 없습니다. **각 notify 가 독립적**이므로 수신 즉시
  DB 에 `UPSERT (ts, id) → value` 하면 됩니다.
- "5분 그룹 완성" 시점이 필요하면 **ts 변경** 또는 **일정 시간(예: 3초) 동안

  같은 ts 추가 수신 없음** 을 트리거로 쓰세요.
- 펌웨어가 동일 레코드를 다시 보낼 가능성(연결 중간에 끊어진 경우)이 있으므로,
  **`(ts, id)` 튜플을 DB 의 유니크 키**로 두고 `UPSERT` 하세요. 중복이면 덮어쓰기.

---

## 5. 동작 타이밍 / 용량

| 항목 | 값 |
|---|---|
| 스냅샷 주기 | 10초 |
| 집계 창 | 5분 (= 10s × 30 샘플) |
| 한 구간이 만들 수 있는 센서 레코드 | 최대 16개 (그 시점의 활성 센서만큼) |
| 레코드 하나 크기(플래시) | 20 B |
| 링버퍼 총 크기 | 1 MB (`/data/agg_ring.bin`) |
| 링버퍼 용량 | 약 **52,425 레코드** |
| BLE 전송 간격 (레코드당) | 약 30~50ms (세마포어 + 백오프) |

한 레코드 = notify 1 개.
10종 센서 * 5분 = 한 창당 10개 notify, 전송 ~0.5초.
하루치 백로그(288 창 × 10센서 ≈ 2,880 레코드) ≈ **2~3분** 내에 업로드 완료됩니다.

참고: v1(264B/레코드, HE 포함) 에서 v2(20B/레코드, HE 없음) 로 파일 포맷이 바뀌
었기 때문에, 기존 링 파일은 펌웨어가 **자동으로 재생성**합니다(헤더 magic 불일치).
이 시점에 v1 미전송분은 소실됩니다(1회성).

---

## 6. 링버퍼 동작 규칙

1. 버퍼는 원형(circular). `write_idx` 가 `read_idx` 를 따라잡으면 **가장 오래된
   레코드를 덮어씁니다** (버퍼 가득 찬 경우에도 펌웨어는 계속 동작). 경고 로그
   `"ring FULL - overwriting oldest record"` 출력.
2. 전송 중 BLE 가 끊기면, **방금 notify 를 시도하던 레코드의 `read_idx` 는
   전진하지 않습니다**. 다음 재연결 시 해당 레코드를 다시 전송합니다.
   → 앱은 `(ts, id)` UPSERT 로 중복 제거.
3. 레코드 1건의 notify 가 성공하는 순간 펌웨어는 `read_idx` 를 전진시키고
   플래시 헤더를 fsync 합니다. = "전송 완료분은 삭제됨".
4. 재부팅 시 `read_idx`, `write_idx`, `count` 는 파일 헤더에서 복구되므로
   **전원 재시작에도 미전송 데이터는 보존**됩니다.

---
## 7. 펌웨어 디버그 로그 (예)

```
I agg_buffer: ring opened: read=12 write=37 pending=25 cap=52425
I agg_buffer: snapshot_task started (period=10s, samples/record=30, wearable-only, save-only-when-BLE-offline)
I agg_buffer: flush_task started (event-driven, history-first policy)
I agg_buffer: wrote records ts=1713700800 ok=8 fail=0 pending=33
I agg_buffer: flush start: pending=25 (live notify paused)
I agg_tx:   notify=H,1713700500,CO2,610.000
I agg_tx:   notify=H,1713700500,PM2.5,33.900
I agg_tx:   notify=H,1713700800,CO2,612.133
...
I agg_buffer: flush end: pending=0 (live notify resumed)
```

Static 보드에서는 대신 아래 로그 한 줄만 뜨고 이후 저장/전송은 일어나지 않습니다.

```
W agg_buffer: static board (flag_IS_WEARABLE=0): history buffering disabled
```

---

## 8. 권장 앱 구현 순서

1. 위 파싱 로직으로 라이브 / 히스토리 분기.
2. DB 에 `(ts, id)` 유니크 제약으로 `UPSERT`.
3. UI: 라이브 뷰는 기존대로, 히스토리 뷰는 `SELECT * FROM history ORDER BY ts`.
4. (선택) 업로드 진행률: `ts` 가 바뀌는 횟수로 "완성된 5분 창"을 카운트.
