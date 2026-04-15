#include <stdio.h>
#include <time.h>
#include <sys/time.h>  // 추가된 헤더 파일
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "esp_timer.h"



static const char* TAG = "RTC_TIME";



void rtc_set(int year,  int month,   int mday,  int hour,  int min, int sec)
{
    struct  tm timeinfo;
    timeinfo.tm_year =  year - 1900;
    timeinfo.tm_mon  =  month;
    timeinfo.tm_mday =  mday;
    timeinfo.tm_hour =  hour;
    timeinfo.tm_min  =  min;
    timeinfo.tm_sec  =  sec;
   
    time_t t = mktime(&timeinfo);
    struct timeval now = { .tv_sec = t };
    settimeofday(&now, NULL);

    ESP_LOGI(TAG,   "RTC  initialized to  %04d-%02d-%02d  %02d:%02d:%02d",  year,  month, mday, hour, min, sec);
}


// RTC 시간을 읽어오는 함수
int read_rtc_time(void) 
//  static int read_rtc_time(void) 
{
    time_t now;
    struct tm timeinfo;


    // Set timezone to Eastern Standard Time and print local time
    setenv("TZ", "KST-9", 1);
    tzset();
    time(&now);
    localtime_r(&now, &timeinfo);

    ESP_LOGI(TAG, "Current time: %d-%02d-%02d %02d:%02d:%02d",
             timeinfo.tm_year + 1900,
             timeinfo.tm_mon + 1,
             timeinfo.tm_mday,
             timeinfo.tm_hour,
             timeinfo.tm_min,
             timeinfo.tm_sec);
    return 0; 
}



//  RTC 시간 얻어 오는 함수 


struct tm  get_rtc_time(void)
{
       time_t now;
       struct  tm timeinfo;
       // Set timezone to Eastern Standard Time and print local time
       setenv("TZ", "KST-9", 1);
       tzset();
       time(&now);
       localtime_r(&now, &timeinfo);
       return  timeinfo;
}



#if 0
time_t  get_second(void)
{
      time_t  now;
      // Set timezone to Eastern Standard Time and print local time
      setenv("TZ", "KST-9", 1);
      tzset();
      time(&now);
      return now;
}
#endif 




void  rtc_task(void * pvParameter) 
{
    rtc_set(2024,  7,  2,  20, 51, 0);
    while(1) {
         read_rtc_time(); 
         vTaskDelay(5000/portTICK_PERIOD_MS);
    }
}










