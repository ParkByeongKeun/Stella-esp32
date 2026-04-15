/*   2022 Espressif Systems (Shanghai) CO LTD
 
*/
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
/*   2022 Espressif Systems (Shanghai) CO LTD
*/

void    rtc_set(int year,  int month,   int mday,  int hour,  int min, int sec);
int     read_rtc_time(void) ;
void    rtc_task(void * pvParameter); 
struct tm  get_rtc_time(void);
//time_t   get_second(void);

#ifdef __cplusplus
}
#endif


