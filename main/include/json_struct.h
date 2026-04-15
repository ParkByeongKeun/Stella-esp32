/*   2022 Espressif Systems (Shanghai) CO LTD
 
*/
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "iotech_global.h" 

#if 0
typedef struct{
      char dev_id[20];
      char time_start[20];
      char time_end[20];
      int64_t time_start_sec;
      int64_t time_end_sec;
      double watt;
      double va;
      double var;
      double volt;
      double current_avg;
      double wh_accu;
      double vah_accu;
      double varh_accu;
      double freq;
      double pf;
      double temp;     
}Device;
#else 
typedef struct{
    //  char dev_id[DEVICE_ID_LEN];
      char time_start[20];
      char time_end[20];
      int64_t time_start_sec;
      int64_t time_end_sec;
      char watt[20];
      char va[20];
      char var[20];
      char volt[20];
      char current_avg[20];
      char wh_accu[20];
      char vah_accu[20];
      char varh_accu[20];
      char freq[20];
      char pf[20];
      char temp[20];  
      char thd[20];   
}Device;
#endif 














typedef struct {
   // char dev_id[DEVICE_ID_LEN];
    char time_event[20];
    int64_t time_sec;
    char event_class[20];
    char event_value[20];
    int direction;
} Event;


typedef struct {
    char time_cont[20];
    int64_t time_sec;
    int cont_method;
} Control;


typedef struct {
    char time_regi[20];
    int64_t time_sec;
    char location[30];
    char source[20];
    char type[20];
    char regi_by_who[20];
} Regi;


/*
typedef struct {
    char time_conf[20];
    int64_t time_sec;
    char freq_margin[20];
    char over_voltage_margin[20];
    char under_voltage_margin[20];
    char over_current_warning[20];
    char over_current_event[20];
    char current_warning_duration[20];
    char current_event_duration[20];
    char noti_period[20];
} Conf;
*/


typedef  struct {
     char    time_warning[20];
     int64_t time_sec;
     char    swell[20];
     char    dip[20];
     char    over_current[20];
     char    freq_error[20];  
     char    ground_fault[20]; 
     char    power_status[20];
}Warning;




size_t serialize_device(Device *device, char **output);
//void serialize_event(Event *event, char *output, size_t output_size);
size_t serialize_event(Event *event, char **output);
size_t serialize_control(Control *control, char **output);
size_t serialize_regi(Regi *regi, char **output);
size_t serialize_variable_event(update_flags_t flags, char ** output);
size_t serialize_warning_event(update_flags_t flags);
//size_t serialize_warning_event(update_flags_t flags,  char ** output);
//size_t serialize_warning(char **output);
size_t serialize_warning();


extern   Warning  warning;


