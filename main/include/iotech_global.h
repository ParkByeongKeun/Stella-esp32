#ifndef IOTECH_GLOBAL_H
#define IOTECH_GLOBAL_H

#include <stddef.h>  // size_t 정의를 위해 필요
#include <stdint.h>  // uint8_t 정의를 위해 필요

#define  TCP_SERVER_TEST_ONLY (0)
#define  DEVICE_ID_LEN    (21)

// 가변 길이 데이터를 포함하는 구조체 정의
typedef struct {
    int id;
    size_t length;
    char data[];  // 가변 길이 배열
} tx_data_t;



//  구조체 정의 
typedef struct {
    uint8_t avcc : 1;
    uint8_t aicc : 1;
    uint8_t cal_irms_cc : 1;
    uint8_t cal_vrms_cc : 1;
    uint8_t cal_energy_cc : 1;
    uint8_t cal_power_cc : 1;
    uint8_t noti_period : 1;
    uint8_t mesh_ap_ssid : 1;
    uint8_t mesh_ap_passwd : 1;
    uint8_t mqtt_broker_uri : 1;
    uint8_t freq_margin     : 1;
    uint8_t rated_voltage   : 1;
    uint8_t rated_current   : 1;  
   // uint8_t over_voltage_margin : 1;
   // uint8_t under_voltage_margin: 1;
    uint8_t warning_duration : 1; 
    uint8_t relay            : 1; 
   // uint8_t over_voltage :1;
  //  uint8_t under_voltage:1;
    uint8_t voltage :1; 
    uint8_t over_current :1;
    uint8_t freq_error : 1;
    uint8_t ground_fault :1; 
    uint8_t total_energy :1;
    uint8_t constant     :1;
    uint8_t power_status :1;
    uint8_t arc_alarm    :1; 
    uint8_t dip          :1;
    uint8_t swell        :1;
    uint8_t rated_freq   :1;
    uint8_t restart      :1;
}update_flags_t;













/* mesh 콘솔(esp32_mesh_iotech>): stella-tools REPL 없이 fan PWM 명령 등록 + I2C1 준비 */
void stella_mesh_console_register_fan_cmd(void);

/* mesh: app_main_stella 미호출 시 CM4(UART2)·UART1·I2C 센서 → CM4 JSON 경로 */
void stella_mesh_start_cm4_sensor_bridge(void);

#endif // IOTECH_GLOBAL_H