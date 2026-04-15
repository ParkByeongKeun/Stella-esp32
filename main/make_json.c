#include <stdio.h>    // snprintf 및 sprintf 함수 사용을 위해 필요
#include  <string.h>
#include  <time.h>
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "cJSON.h"
#include "esp_log.h"
#include "rtc_time.h"
#include "iotech_global.h"
#include "json_struct.h"
#include "meter_app.h"
#include "spifss.h"

static const char *TAG = "json";

extern  meter write_meter;
extern  meter last_meter;


size_t serialize_device(Device *device, char **output)
{
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "time_start", device->time_start); 
  cJSON_AddStringToObject(root, "time_end", device->time_end);
  cJSON_AddNumberToObject(root, "time_start_sec", device->time_start_sec);
  cJSON_AddNumberToObject(root, "time_end_sec", device->time_end_sec);
  cJSON_AddStringToObject(root, "watt",   device->watt);
  cJSON_AddStringToObject(root, "var", device->var);
  cJSON_AddStringToObject(root, "va", device->va);
  cJSON_AddStringToObject(root, "volt", device->volt);
  cJSON_AddStringToObject(root, "current_avg", device->current_avg);
  cJSON_AddStringToObject(root, "freq", device->freq);
  cJSON_AddStringToObject(root, "pf",   device->pf);
  cJSON_AddStringToObject(root, "wh_accu", device->wh_accu);
  cJSON_AddStringToObject(root, "varh_accu", device->varh_accu);
  cJSON_AddStringToObject(root, "vah_accu", device->vah_accu);
  cJSON_AddStringToObject(root, "thd",       device->thd);  
  cJSON_AddStringToObject(root, "temp",      device->temp); 
  
  size_t json_len=0;
  char *json_str = cJSON_Print(root);
   if (json_str != NULL) {
        //strncpy(output, json_str, output_size - 1);
        json_len=  strlen(json_str) +1;         //  +1  for null terminator
        *output  = (char *)malloc(json_len);
       if(*output != NULL) {
           strncpy(*output, json_str, json_len);
       }
       cJSON_free(json_str);
  }
  cJSON_Delete(root);
  return json_len;
}



size_t serialize_event(Event *event, char **output)
{
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "time_event", event->time_event);
  cJSON_AddNumberToObject(root, "time_sec", event->time_sec);
 // cJSON_AddStringToObject(root, "dev_id", event->dev_id);
  cJSON_AddStringToObject(root,  event->event_class,  event->event_value);
  //cJSON_AddStringToObject(root, "event_class", event->event_class);
  //cJSON_AddStringToObject(root, "event_value", event->event_value);
  cJSON_AddNumberToObject(root, "direction", event->direction);
  size_t json_len=0;
  char *json_str = cJSON_Print(root);
  if (json_str != NULL) {
        //strncpy(output, json_str, output_size - 1);
        json_len=  strlen(json_str) +1;         //  +1  for null terminator
        *output  = (char *)malloc(json_len);
       if(*output != NULL) {
           strncpy(*output, json_str, json_len);
       }
       cJSON_free(json_str);
  }
  cJSON_Delete(root);
  return json_len;
}



 
size_t  serialize_warning_event(update_flags_t  flags)
{
   struct tm timeinfo;
   extern char  buffer[];
      

   cJSON *root  =  cJSON_CreateObject();
   timeinfo  =  get_rtc_time();
   char  time_event[40];
   snprintf(time_event, sizeof(time_event), "%04d-%02d-%02d %02d:%02d:%02d",    
             (int)timeinfo.tm_year+1900,
             (int)timeinfo.tm_mon+1,
             (int)timeinfo.tm_mday,
             (int)timeinfo.tm_hour,
             (int)timeinfo.tm_min,
             (int)timeinfo.tm_sec);


    int tm_year  = timeinfo.tm_year+1900;

    if(tm_year<2024)  
    {
         printf("Year: %d",  tm_year);
         return 0;
    }  


   ESP_LOGI(TAG,  "%04d-%02d-%02d %02d:%02d:%02d", 
                 (int)timeinfo.tm_year+1900,
                 (int)timeinfo.tm_mon+1,
                 (int)timeinfo.tm_mday,
                 (int)timeinfo.tm_hour,
                 (int)timeinfo.tm_min,
                 (int)timeinfo.tm_sec);

     

   //if((flags.over_voltage==0)&(flags.under_voltage==0)&(flags.over_current==0)&(flags.freq_error==0)&(flags.ground_fault==0)&(flags.power_status==0))  return 0;     
   cJSON_AddStringToObject(root, "time_event", time_event);




  // sprintf(time_second, "%lld", get_second());  //convert_to_seconds(char datetime[20])
  // sprintf(time_second, "%ld", convert_to_seconds(time_event));
   int  time_sec =   convert_to_seconds(time_event);   
   cJSON_AddNumberToObject(root, "time_sec",  time_sec);


    
  extern  char  swell_status[]; 
  extern  char  rms_voltage[];


  if(flags.swell== 1)
  {
     cJSON_AddStringToObject(root,  "swell",  swell_status);
     if(strcmp(swell_status, "0")!=0)
     {
       
         cJSON_AddStringToObject(root, "voltage",  rms_voltage);
     }
     flags.swell = 0; 
  }

  extern  char dip_status[]; 
  if(flags.dip==1)
  {
     cJSON_AddStringToObject(root,  "dip",    dip_status);
     if(strcmp(dip_status, "0")!=0)
     {
        cJSON_AddStringToObject(root, "voltage",  rms_voltage);        
     } 
     flags.dip=0;
  }
  
  extern  char  over_current_status[];
  extern  char  rms_current[];  

  if(flags.over_current==1)
  {
     cJSON_AddStringToObject(root, "over_current",  over_current_status);
     if(strcmp(over_current_status, "0")!=0)
     {
          cJSON_AddStringToObject(root, "current",  rms_current);          
     }
     flags.over_current=0;      
  }

  extern char freq_error_status[];
  extern char  frequency[];
  if(flags.freq_error==1)
  {
     cJSON_AddStringToObject(root,  "freq_error",    freq_error_status); 
     if(strcmp(freq_error_status, "0")!=0)
     {
          cJSON_AddStringToObject(root, "frequency",  frequency);         
     }
     flags.freq_error=0;
  }

  extern  char ground_fault[];
  if(flags.ground_fault==1)
  {
      cJSON_AddStringToObject(root, "ground_fault", ground_fault);
      flags.ground_fault=0;
    }


  extern char  power_status[];
  if(flags.power_status==1)
  {
      cJSON_AddStringToObject(root, "power_status", power_status);   
      flags.power_status=0;
  }



  extern  char  arc_status[];
  if(flags.arc_alarm==1)
  {
      cJSON_AddStringToObject(root, "arc_alarm", arc_status);   
      flags.arc_alarm=0;
  }



 


   size_t json_len=0;
   char *json_str = cJSON_Print(root);
   if (json_str != NULL) {
        //strncpy(output, json_str, output_size - 1);
        json_len=  strlen(json_str) +1;         //  +1  for null terminator

    //    *output  = (char *)malloc(json_len);
    //   if(*output != NULL) {
           strncpy(buffer, json_str, json_len);
           ESP_LOGI(TAG, "json: %s", buffer);
     //  }
       cJSON_free(json_str);
  }


  cJSON_Delete(root);
  return json_len;
}







size_t serialize_variable_event(update_flags_t flags, char ** output)
{
   struct tm timeinfo;
   cJSON *root = cJSON_CreateObject();
   timeinfo  = get_rtc_time();
   char time_event[40];    



   snprintf(time_event,sizeof(time_event) ,"%04d-%02d-%02d %02d:%02d:%02d",
             (int)timeinfo.tm_year+1900,
             (int)timeinfo.tm_mon+1,
             (int)timeinfo.tm_mday,
             (int)timeinfo.tm_hour,
             (int)timeinfo.tm_min,
             (int)timeinfo.tm_sec);
   cJSON_AddStringToObject(root, "time_event", time_event);
  
   
   int time_sec =  convert_to_seconds(time_event);
   cJSON_AddNumberToObject(root, "time_sec",  time_sec);
   
   extern  char   avccValue[];
   if(flags.avcc==1)
   {
       cJSON_AddStringToObject(root,  "avvc",  avccValue);
   }
  
   extern  char    aiccValue[];
   if(flags.aicc==1)
   {
       cJSON_AddStringToObject(root, "aicc",  aiccValue);
   }    

   extern char    str_cal_irms_cc[];
   if(flags.cal_irms_cc==1)
   {
       cJSON_AddStringToObject(root, "cal_irms_cc", str_cal_irms_cc); 
   }  
  
   extern char   str_cal_vrms_cc[];
   if(flags.cal_vrms_cc==1)
   {
       cJSON_AddStringToObject(root, "cal_vrms_cc", str_cal_vrms_cc);
   }


    extern char str_cal_power_cc[];
   if(flags.cal_power_cc==1)
   {
      cJSON_AddStringToObject(root, "cal_power_cc", str_cal_power_cc);
   } 

   extern char str_cal_energy_cc[];
   if(flags.cal_energy_cc==1)
   {
      cJSON_AddStringToObject(root, "cal_energy_cc", str_cal_energy_cc);
   }


   extern char str_noti_period[];    
   if(flags.noti_period==1)
   {
      cJSON_AddStringToObject(root, "noti_period",  str_noti_period); 
   } 

   extern uint8_t mesh_ap_ssid[];
   if(flags.mesh_ap_ssid==1)
   {        
      cJSON_AddStringToObject(root, "mesh_ap_ssid", (const char*)mesh_ap_ssid);
   }  

   extern uint8_t mesh_ap_passwd[];  
   if(flags.mesh_ap_passwd==1)
   {
      cJSON_AddStringToObject(root, "mesh_ap_passwd",  (const char*)mesh_ap_passwd);   
   }

   extern  char str_rated_voltage[];
   if(flags.rated_voltage==1)
   {
      cJSON_AddStringToObject(root, "rated_voltage",    (const char*)str_rated_voltage);
   }

   extern  char str_rated_current[];
   if(flags.rated_current==1) 
   {
      cJSON_AddStringToObject(root, "rated_current",    (const char*)str_rated_current);
   }


   extern  char str_rated_freq[];
   if(flags.rated_freq==1)
   {
     cJSON_AddStringToObject(root, "rated_freq",   (const char*)str_rated_freq);     
   }



   extern  char swell_status[];
   if(flags.swell==1)
   {
      cJSON_AddStringToObject(root, "swell", (const char*)swell_status);
   } 

   extern  char dip_status[];
   if(flags.dip==1)
   {
      cJSON_AddStringToObject(root, "dip", (const char*)dip_status);
   } 




   extern  char str_over_current[];
   if(flags.over_current==1)
   {
       cJSON_AddStringToObject(root, "over_current", (const char*)str_over_current);
   }


   extern char  str_warning_duration[];
   if(flags.warning_duration==1)
   {
       cJSON_AddStringToObject(root, "warning_duration", (const char*)str_warning_duration);
   }


   extern char  str_relay[];
   if(flags.relay==1) 
   {
       cJSON_AddStringToObject(root, "relay", (const char*)str_relay); 
   } 




   // write_meter.wh
   // write_meter.varh  
   // write_meter.vah
 
   char  str_wh[20];  
   char  str_varh[20];
   char  str_vah[20]; 

   if(flags.total_energy==1)
   {

       if(strcmp(write_meter.datetime, "Null")!=0)
       {  
           snprintf(str_wh, sizeof(str_wh),  "%.1f",  write_meter.wh);
           cJSON_AddStringToObject(root, "total_wh", (const char*)str_wh);
           snprintf(str_varh, sizeof(str_varh), "%.1f", write_meter.varh);
           cJSON_AddStringToObject(root, "total_varh",  (const char*)str_varh);
           snprintf(str_vah, sizeof(str_vah), "%.1f", write_meter.vah);
           cJSON_AddStringToObject(root, "total_vah",  (const char*)str_vah); 
           cJSON_AddStringToObject(root, "datetime",   (const char*)write_meter.datetime);
       }
       else 
       {
           cJSON_AddStringToObject(root, "total_wh",   "Null");
           cJSON_AddStringToObject(root, "total_varh", "Null");
           cJSON_AddStringToObject(root, "total_vah",  "Null");
           cJSON_AddStringToObject(root, "datetime",   "Null");
       }  
   }  


   
   if(flags.constant == 1)
   {
       cJSON_AddStringToObject(root, "avvc",  avccValue);
       cJSON_AddStringToObject(root, "aicc",  aiccValue);
       cJSON_AddStringToObject(root, "cal_irms_cc", str_cal_irms_cc);
       cJSON_AddStringToObject(root, "cal_vrms_cc", str_cal_vrms_cc);
       cJSON_AddStringToObject(root, "cal_power_cc", str_cal_power_cc);
       cJSON_AddStringToObject(root, "cal_energy_cc", str_cal_energy_cc);
   }



   size_t json_len=0;
   char *json_str = cJSON_Print(root);
   if (json_str != NULL) {
        //strncpy(output, json_str, output_size - 1);
        json_len=  strlen(json_str) +1;         //  +1  for null terminator

        *output  = (char *)malloc(json_len);
       if(*output != NULL) {
           strncpy(*output, json_str, json_len);
           ESP_LOGI(TAG, "jason: %s",*output);
       }
       cJSON_free(json_str);
  }
  cJSON_Delete(root);
  return json_len;
}




#if 0
size_t serialize_warning(char  **output){
    
            
    Warning *pWarning =  &warning;
    cJSON  *root  =  cJSON_CreateObject();
    cJSON_AddStringToObject(root,  "time_warning",  pWarning->time_warning);
    cJSON_AddNumberToObject(root,  "time_sec",      pWarning->time_sec);
    cJSON_AddStringToObject(root,  "over_voltage",  pWarning->over_voltage);
    cJSON_AddStringToObject(root,  "under_voltage", pWarning->under_voltage);  
    cJSON_AddStringToObject(root,  "over_current",  pWarning->over_current);               
    cJSON_AddStringToObject(root,  "freq_error",    pWarning->freq_error);                     
    cJSON_AddStringToObject(root,  "ground_fault",  pWarning->ground_fault);
    size_t  json_len=0;
    char  *json_str = cJSON_Print(root);
     

   // ESP_LOGI(TAG, "jason_str: %s", json_str);

    if(json_str != NULL)  {
       //strncpy(output,  json_str,  output_size  -1);
       json_len  = strlen(json_str)  +1;      //  +1  for null terminator 
       *output   = (char *)malloc(json_len);
       if(*output  != NULL)  {
            strncpy(*output, json_str, json_len);
                ESP_LOGI(TAG, "jason: %s",*output);
       }
       cJSON_free(json_str); 
    } 
    cJSON_Delete(root);
    return json_len;
}
#else 
size_t serialize_warning(){
    extern char  buffer[];
    extern  int power_in;            
    Warning *pWarning =  &warning;
    cJSON  *root  =  cJSON_CreateObject();
    cJSON_AddStringToObject(root,  "time_warning",  pWarning->time_warning);
    cJSON_AddNumberToObject(root,  "time_sec",      pWarning->time_sec);
    cJSON_AddStringToObject(root,  "swell",         pWarning->swell);
    cJSON_AddStringToObject(root,  "dip",           pWarning->dip);  
    cJSON_AddStringToObject(root,  "over_current",  pWarning->over_current);               
    cJSON_AddStringToObject(root,  "freq_error",    pWarning->freq_error);                     
   
   
    //cJSON_AddStringToObject(root,  "ground_fault",  pWarning->ground_fault);
    extern  char  ground_fault[10];
    power_in  = ground_fault_status();


     extern char  str_relay[]; 

    
     int ret =  strcmp(str_relay ,  "on");
     
     if((ret==0) && !power_in)
     {
        snprintf(ground_fault, sizeof(ground_fault), "%d", 1);
     }
     else   snprintf(ground_fault, sizeof(ground_fault), "%d", 0);

    strncpy(pWarning->ground_fault, ground_fault, 10);

    cJSON_AddStringToObject(root,  "ground_fault",  pWarning->ground_fault);
      
    extern char power_status[];
    strncpy(pWarning->power_status, power_status, 10);
    cJSON_AddStringToObject(root, "power_status", pWarning->power_status); 
    
    size_t  json_len=0;
    char  *json_str = cJSON_Print(root);
     

   // ESP_LOGI(TAG, "jason_str: %s", json_str);

    if(json_str != NULL)  {
       //strncpy(output,  json_str,  output_size  -1);
       json_len  = strlen(json_str)  +1;      //  +1  for null terminator 
       //*output   = (char *)malloc(json_len);
     // if(*output  != NULL)  {
       //     strncpy(*output, json_str, json_len);
              strncpy(buffer, json_str, json_len);
              ESP_LOGI(TAG, "jason: %s",buffer);
      // }
       cJSON_free(json_str); 
    } 
    cJSON_Delete(root);
    return json_len;
}
#endif 




size_t serialize_control(Control *control, char **output) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "time_cont", control->time_cont);
    cJSON_AddNumberToObject(root, "time_sec", control->time_sec);
    cJSON_AddNumberToObject(root, "cont_method", control->cont_method);
    size_t json_len=0;
    char *json_str = cJSON_Print(root);
    if (json_str != NULL) {
        //strncpy(output, json_str, output_size - 1);
        json_len=  strlen(json_str) +1;         //  +1  for null terminator
        *output  = (char *)malloc(json_len);
       if(*output != NULL) {
           strncpy(*output, json_str, json_len);
       }
       cJSON_free(json_str);
    }
    cJSON_Delete(root);
    return json_len;
}




size_t serialize_regi(Regi *regi, char **output) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "time_regi",   regi->time_regi);
    cJSON_AddNumberToObject(root, "time_sec",    regi->time_sec);
    cJSON_AddStringToObject(root, "location",    regi->location);
    cJSON_AddStringToObject(root, "source",      regi->source);
    cJSON_AddStringToObject(root, "type",        regi->type);
    cJSON_AddStringToObject(root, "regi_by_who", regi->regi_by_who);
    size_t json_len=0;
    char *json_str = cJSON_Print(root);
    if (json_str != NULL) {
        //strncpy(output, json_str, output_size - 1);
        json_len=  strlen(json_str) +1;         //  +1  for null terminator
        *output  = (char *)malloc(json_len);
       if(*output != NULL) {
           strncpy(*output, json_str, json_len);
       }
       cJSON_free(json_str);
    }
    cJSON_Delete(root);
    return json_len;
}
