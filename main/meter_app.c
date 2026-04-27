/* Declarations of command registration functions.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include  "freertos/FreeRTOS.h"
#include  "freertos/task.h"
#include  "freertos/queue.h"
#include  "freertos/semphr.h"
#include  "driver/spi_master.h"
#include  "driver/gpio.h"
#include "esp_system.h"
#include  "esp_log.h"
#include  "esp_err.h"
#include  <stdbool.h>
#include  <stdint.h>
#include  <string.h>
#include  <unistd.h> // Include this header for usleep
#include  <time.h>
#include  <sys/time.h>  // 추가된 헤더 파일
#include "portmacro.h" // for portMUX_TYPE

#include  "meter_app.h"
#include  "json_struct.h"
#include  "ade9153a_spi.h"
#include  "iotech_global.h"
#include  "spifss.h"
#include  "kiss_fft.h"
#include "esp_heap_caps.h"


#define  BUFFER_SIZE 600
#define  blinkInterval  500

static void *meter_malloc_spiram(size_t n)
{
    void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p ? p : malloc(n);
}

static const char *TAG = "meter";
static const char *TAG_GPIO = "gpio";
static const char *TAG_AUTOCAL =  "autocal";


EnergyRegs  energyVals;        //Energy register values are read and stored in EnergyRegs structure      
PowerRegs   powerVals;    //Metrology data can be accessed from these structures
RMSRegs     rmsVals;
PQRegs      pqVals;
AcalRegs    acalVals;
Temperature tempVal;

strMeterData    refMeterData;
strAccumEnergy  refAccumEnergy;

//static strEnergy energyArray[15];        //Energy register values are read and stored in EnergyRegs structure
static strEnergy energyArray[60];        //Energy register values are read and stored in EnergyRegs structure
char  topic[200];

const int  cmd_data_noti    =1;
const int  cmd_event        =2;
const int  cmd_control_noti =3;
const int  cmd_conf_noti    =4;
const int  cmd_debug_noti   =5;

time_t          previous_time;
int             cntMin=0;

const long reportInterval = 2000;
const int  arcInterval  =1000;

bool   meter_task_disable  = false; 
extern  TaskHandle_t xTaskHandle_spi;
extern  char   device_id [];   
extern  QueueHandle_t txQue;


//static   float    PRE_WH_ACCU=0;
//static   float    PREPRE_WH_ACCU=0;

int   noti_period=5;

//org  bool  relay_state=  false;
//shcho
int  relay_state=  -1; // prev_relay_state = -2 
extern int  prev_relay_state ; // prev_relay_state = -2 
//org: bool   relay_latch_flag = false;   
//shcho
//  volatile int   relay_latch_flag = false;   
int   relay_latch_flag = false;   
extern int flag_relay_control ; 


volatile  bool   zero_flag=false;
extern volatile  bool  time_1min;
extern volatile  bool  time_1day;
static  int  db_size;  

extern  uint8_t mesh_ap_ssid[];

Warning  warning;
char buffer[BUFFER_SIZE];
extern int32_t dma_buffer[FFT_SIZE];
  
//extern  int32_t   voltage_buf[FFT_SIZE+1]; 


#if 0
kiss_fft_cpx in[FFT_SIZE], out[FFT_SIZE];
#else  
kiss_fft_cpx *in, *out;
#endif 

update_flags_t  flags; 
bool            event_flag;
bool            arc_flag=false; 

extern  meter write_meter;
extern  meter last_meter;
extern  volatile int   power_in; 
static  int   prev_power_in;

extern esp_err_t Write_Relay(char *str_relay);





int loop(void);
void runLength(long  seconds);
void sendEvent(const char* str);
long convert_to_seconds(char* datetime);

extern void mqtt_app_publish(char* topic, char *publish_string);

extern char  swell_status[6];
extern char  dip_status[6];  
char  over_current_status[6]= "0";
char  freq_error_status[6]=  "0";  

char  rms_voltage[10] ="0";
char  rms_current[10] ="0"; 
char  frequency[10]="0";
char  ground_fault[10]="0";  
char  power_status[10]= "off";
char  arc_status[10]=  "off"; 

extern int	flag_AV_WAVE_work ;


uint16_t   rated_freq=60;


void  check_remove_db(char  datetime[]);


void  init_power_status(void)
{
    power_in = gpio_get_level(PIN_NUM_POWER_IN);
    ESP_LOGI(TAG_GPIO, "POWER_IN : %d", power_in);
    if(power_in)
    {
		sprintf(power_status, "on"); 
    } 
    else 
    {
		sprintf(power_status, "off");
    }
    prev_power_in = power_in;
    event_flag  =true;
    flags.power_status =1;  
}






static QueueHandle_t gpio_evt_zx_queue = NULL;
extern   void check_ADE9053a(void);

static void check_ADE9053a_task(void *arg)
{
	(void)arg;
	check_ADE9053a();
}


//  void app_measure(void)
void app_measure(void *pvParameters)
{
	static int is_1st = 1 ; 

//  	ESP_LOGI("SPI", "ADE9053 Task Init");
//  	ESP_LOGI("SPI", "ADE9053 Task Init");
//  	ESP_LOGI("SPI", "ADE9053 Task Init");
//  	ESP_LOGI("SPI", "ADE9053 Task Init");
//  	ESP_LOGI("SPI", "ADE9053 Task Init");
//  	ESP_LOGI("SPI", "ADE9053 Task Init");
//  	ESP_LOGI("SPI", "ADE9053 Task Init");
   
   static  int  ret =0;

   ESP_LOGI("SPI", "ADE9053 Task Init");
 
	AIGain_Init();
	if( is_1st == 1 ) 
	{
		is_1st = 0 ; 

		init_db();
		init_power_status();



		// 24-07-03
		// every 60s(1 min) 
		previous_time = time(NULL);  // 시작 시간 저장
		char datetime[20]; 
		struct tm *local_time = localtime(&previous_time);
		strftime(datetime, 20, "%Y-%m-%d %H:%M:%S", local_time);
		check_remove_db(datetime);
	}

	// zx_gpio_... 사용을 변경하려고 했는데 
	// SW문제가 아니라 Relay가 동작하면서 방사 Noise가 ESP32을 결국을 Rebooting할 수 밖에
	// 없는 상태로 만든다. Relay를 On/Off하지 않고 계속 On이나 Off해서 Relay가 동작하지 않도록하면
	// Relay Control을 위한 SW는 정상적으로 동작함을 확인할 수 있다.
//  	gpio_evt_zx_queue = xQueueCreate(10, sizeof(uint32_t));

 
  

	while(1)
	{
//  		ESP_LOGI("shcho", "							meter_task_disable=%d/ vTaskDelay(1000msec) + delay in loop()", meter_task_disable);
//  		ESP_LOGI("shcho", "							meter_task_disable=%d/ vTaskDelay(1000msec) + delay in loop()", meter_task_disable);
//  		ESP_LOGI("shcho", "							meter_task_disable=%d/ vTaskDelay(1000msec) + delay in loop()", meter_task_disable);

		if(meter_task_disable==false) ret = loop();

		if(ret !=0)    break;
		//org: vTaskDelay(1000/portTICK_PERIOD_MS);
		vTaskDelay(pdMS_TO_TICKS(1000)); // 1000ms 대기 , loop()에서 추가로 1초정도 소모한다.
	}

	ESP_LOGW("app_measure: ", "1.	after relay control: respawn check_ADE9053a() --> app_measure()");
	ESP_LOGW("app_measure: ", "2.	after relay control: respawn check_ADE9053a() --> app_measure()");
	xTaskCreate(check_ADE9053a_task,  "check_task", 2048+1024, NULL,  0, NULL);
	vTaskDelete(NULL);
   

}


extern   int   rated_voltage;
extern   int   rated_current;
extern   uint16_t   swell_voltage;
extern   uint16_t   dip_voltage;
extern  float  over_current;
extern  float  freq_margin;



  


#if 0

void readandwrite(Warning *warning)
{
       
      char  buffer[BUFFER_SIZE];
      char  topic[200];

      update_flags_t flags = {0}; 

      static  int  prev_over_current_en;
      static  int  prev_over_voltage_en;
      static  int  prev_under_voltage_en;
      static  int  prev_freq_error_en; 

      int  over_current_en;
      int  over_voltage_en;
      int  under_voltage_en;
      int  freq_error_en;

      extern  int    power_in;
      static  int    prev_power_in=1;

     
     /* Read and Print WATT Register using ADE9153A Read Library */
     ReadPowerReg(&powerVals);     //Template to read Power registers from ADE9000 and store data in Arduino MCU           
     ReadRMSRegs(&rmsVals);
     ReadPQRegs(&pqVals);
     ReadTemperature(&tempVal);

     refMeterData.RMSCurrentVal =  rmsVals.CurrentRMSValue/1000;
     // printf("RMS Current:\t %f A\n", refMeterData.RMSCurrentVal);
 

     //flags.over_current = 1;
     //flags.current = 1; 

     if(refMeterData.RMSCurrentVal>(rated_current+over_current_margin))
     {
              over_current_en = 1;
              snprintf(warning->over_current, sizeof(char)*6, "%d",  over_current_en);
     }   
     else 
     {
              over_current_en = 0; 
              snprintf(warning->over_current, sizeof(char)*6, "%d",  over_current_en); 
     } 



     if(((over_current_en==1) && (prev_over_current_en==0)) | ((over_current_en==0) && (prev_over_current_en==1)))
     {
            flags.over_current = 1;
            if(over_current_en==1)
            {
                 snprintf(rms_current,   sizeof(rms_current),  "%f",  refMeterData.RMSCurrentVal);
            } 
           else 
            {
                 snprintf(rms_current,  sizeof(rms_current),  "0");  
            }
     } 
     
      prev_over_current_en  =  over_current_en;      


      //flags.over_voltage = 1;
      //flags.voltage =  1;

    
     refMeterData.RMSVoltageVal = rmsVals.VoltageRMSValue/1000;
     if(refMeterData.RMSVoltageVal>(rated_voltage+over_voltage_margin))
     {
          over_voltage_en  =1;        
          snprintf(warning->over_voltage,  sizeof(char)*6, "%d",  over_voltage_en);
     }
     else 
     {
          over_voltage_en  =0;
          snprintf(warning->over_voltage,  sizeof(char)*6, "%d",  over_voltage_en);
     }

     // flags.under_voltage = 1;
     // flags.voltage =  1;

     if(((over_voltage_en==1) && (prev_over_voltage_en==0))  | ((over_voltage_en==0) && (prev_over_voltage_en==1)))
     {
          flags.over_voltage = 1;

          if(over_voltage_en==1)
          {
                 snprintf(rms_voltage,  sizeof(rms_voltage),  "%f",  refMeterData.RMSVoltageVal);
          }
          else 
          {
                 snprintf(rms_voltage,  sizeof(rms_voltage),  "0"  );  
          }
     } 

     prev_over_voltage_en =  over_voltage_en;


     if(refMeterData.RMSVoltageVal<(rated_voltage-under_voltage_margin))
     {        
           under_voltage_en =1;
           snprintf(warning->under_voltage,  sizeof(char)*6, "%d",  under_voltage_en);
     }
     else 
     {
           under_voltage_en =0;
           snprintf(warning->under_voltage, sizeof(char)*6,  "%d",  under_voltage_en); 
     }


     if(((under_voltage_en==1) && (prev_under_voltage_en==0)) | ((under_voltage_en==0)  && (prev_under_voltage_en==1)))
     {
          flags.under_voltage=1;
          if(under_voltage_en==1)
          {
                snprintf(rms_voltage, sizeof(rms_voltage),  "%f",  refMeterData.RMSVoltageVal); 
          }
          else 
          {
                snprintf(rms_voltage, sizeof(rms_voltage), "0");  
          }
     }
     prev_under_voltage_en =  under_voltage_en;

   //  printf("RMS Voltage:\t %f V\n",  refMeterData.RMSVoltageVal);
     refMeterData.ActivePowerVal = powerVals.ActivePowerValue/1000;
   //  printf("Active Power:\t %f W\n", refMeterData.ActivePowerVal);

     refMeterData.ReactivePowerVal = powerVals.FundReactivePowerValue/1000;
   //  printf("Reactive Power:\t %f VAR\n", refMeterData.ReactivePowerVal);

     refMeterData.ApparentPowerVal = powerVals.ApparentPowerValue/1000;
   //  printf("Apparent Power:\t %f VA\n", refMeterData.ApparentPowerVal);
       
     refMeterData.PowerFactorVal =  pqVals.PowerFactorValue;
   //  printf("Power Factor:\t %f\n", refMeterData.PowerFactorVal);
     
     refMeterData.FrequencyVal = pqVals.FrequencyValue;
   //  printf("Frequency:\t %f Hz\n", refMeterData.FrequencyVal);

     float  rated_freq;  


    if(refMeterData.FrequencyVal>55)  rated_freq = 60.0;
    else                              rated_freq = 50.0;
 
     
    


     if((refMeterData.FrequencyVal >  (rated_freq+ freq_margin)) &&  (refMeterData.FrequencyVal >  (rated_freq- freq_margin)))
     {
                 freq_error_en =1;
                 snprintf(warning->freq_error,  sizeof(char)*6, "1");
     }  
     else 
     {
                 freq_error_en =0; 
                 snprintf(warning->freq_error,  sizeof(char)*6, "0");  
     }



     if(((prev_freq_error_en==1) && (freq_error_en==0))|((freq_error_en==1) &&  (prev_freq_error_en==1)))
     {
               flags.freq_error = 1;  

               if(freq_error_en==1)
               {
                      snprintf(frequency, sizeof(frequency),  "%f",  refMeterData.FrequencyVal); 
               }
               else 
               {
                      snprintf(frequency, sizeof(frequency),  "0");  
               }                
     }

      prev_freq_error_en =  freq_error_en; 

   //  power_in =   gpio_get_level(PIN_NUM_POWER_IN);  
   power_in =  ground_fault_status();
   
   if(((power_in==1)&&(prev_power_in==0))|(prev_power_in==1)&&(power_in==0))
   {
           flags.ground_fault  = 1;
           if(power_in==1)
           {
                 snprintf(warning->ground_fault, sizeof(char)*6,  "1");
           }
           else 
           {
                 snprintf(warning->ground_fault, sizeof(char)*6,  "0");
           }
   }
   prev_power_in  =  power_in;
     

   refMeterData.TemperatureVal = tempVal.TemperatureVal;



/*
   if((flags.over_current==1) | (flags.over_voltage==1) | (flags.under_voltage==1)  | (flags.freq_error==1) |(flags.ground_fault==1))
   {  
      snprintf(topic, sizeof(topic), "iotech/SEMS/%s/event", device_id); 
      int json_len = serialize_warning_event(flags, &buffer);
      mqtt_app_publish(topic, buffer);
   }
*/


   // printf("Temperature:\t %f degC",  refMeterData.TemperatureVal);
   // printf("\n\n");
   // serialize_warning_event()
}
#else 
bool  readandwrite(update_flags_t  *flags)
{
   
      char  topic[200];
    
      static  int  prev_over_current_en;
      static  int  prev_freq_error_en; 

      int  over_current_en;
      int  freq_error_en;

      static  int    prev_power_in;
      bool  ret =  false;

      Warning *pWarning =  & warning;
      flags->over_current =0;
    
     flags->dip =0;
     flags->swell=0;
     flags->freq_error = 0;  
     flags->ground_fault  = 0;
     
     /* Read and Print WATT Register using ADE9153A Read Library */
     ReadPowerReg(&powerVals);     //Template to read Power registers from ADE9000 and store data in Arduino MCU           
     ReadRMSRegs(&rmsVals);
     ReadPQRegs(&pqVals);
     ReadTemperature(&tempVal);

  
      
     refMeterData.RMSCurrentVal =  rmsVals.CurrentRMSValue/1000;

     if(refMeterData.RMSCurrentVal>over_current)
     {
              over_current_en = 1;
              snprintf(pWarning->over_current, sizeof(char)*6, "%d",  over_current_en);
     }   
     else 
     {
              over_current_en = 0; 
              snprintf(pWarning->over_current, sizeof(char)*6, "%d",  over_current_en); 
     } 

   //  ESP_LOGI(TAG, "1");

    
     if(over_current_en!=prev_over_current_en)
     {
            flags->over_current = 1;
            ret = true; 
            if(over_current_en==1)
            {
                 snprintf(rms_current,  sizeof(rms_current),  "%.2f",  refMeterData.RMSCurrentVal);
            } 
           else 
            {
                 snprintf(rms_current,  sizeof(rms_current),  "0");  
            }
     } 
     
      prev_over_current_en  =  over_current_en;      

 
#if 0  
    
     refMeterData.RMSVoltageVal = rmsVals.VoltageRMSValue/1000;
     if(refMeterData.RMSVoltageVal>(rated_voltage+over_voltage_margin))
     {
          over_voltage_en  =1;        
          snprintf(pWarning->over_voltage,  sizeof(char)*6, "%d",  over_voltage_en);
     }
     else 
     {
          over_voltage_en  =0;
          snprintf(pWarning->over_voltage,  sizeof(char)*6, "%d",  over_voltage_en);
     }

     if(over_voltage_en!=prev_over_voltage_en)
     {
          flags->over_voltage = 1;
          ret = true; 
          if(over_voltage_en==1)
          {
                 snprintf(rms_voltage,  sizeof(rms_voltage),  "%.2f",  refMeterData.RMSVoltageVal);
          }
          else 
          {
                 snprintf(rms_voltage,  sizeof(rms_voltage),  "0"  );  
          }
     } 
     prev_over_voltage_en =  over_voltage_en;


     if(refMeterData.RMSVoltageVal<(rated_voltage-under_voltage_margin))
     {        
           under_voltage_en =1;
           snprintf(pWarning->under_voltage,  sizeof(char)*6, "%d",  under_voltage_en);
     }
     else 
     {
           under_voltage_en =0;
           snprintf(pWarning->under_voltage, sizeof(char)*6,  "%d",  under_voltage_en); 
     }

     if(under_voltage_en!=prev_under_voltage_en)
     {
          flags->under_voltage=1;
          ret = true; 
          if(under_voltage_en==1)
          {
                snprintf(rms_voltage, sizeof(rms_voltage),  "%.2f",  refMeterData.RMSVoltageVal); 
          }
          else 
          {
                snprintf(rms_voltage, sizeof(rms_voltage), "0");  
          }
     }
     prev_under_voltage_en =  under_voltage_en;
 #else

             refMeterData.RMSVoltageVal = rmsVals.VoltageRMSValue/1000;
            if((rmsVals.CurRMSVoltage!=0) && (rmsVals.CurRMSVoltage<=dip_voltage))
            {
              snprintf(pWarning->dip,  sizeof(dip_status),    "1");
            }
            else  if((rmsVals.CurRMSVoltage!=0) && (rmsVals.CurRMSVoltage>= swell_voltage))
            {
              snprintf(pWarning->swell, sizeof(swell_status),    "1");
            }
            else 
            {
               if(strcmp(pWarning->dip, "1")==0)
               {
                    flags->dip  = 1;
                    sprintf(rms_voltage, "%.1f",  rmsVals.CurRMSVoltage);    

               }
               else  if(strcmp(pWarning->swell, "1")==0)
               {
                    flags->swell  = 1;
                    sprintf(rms_voltage, "%.1f",  rmsVals.CurRMSVoltage); 
               }
               else 
               {
                          snprintf(pWarning->dip,     sizeof(dip_status),    "0");
                          snprintf(pWarning->swell,   sizeof(swell_status),  "0");
               }
            }
 #endif  

           




    // ESP_LOGI(TAG, "3");


   

    // ESP_LOGI(TAG, "4");

    

   //  ESP_LOGI(TAG, "5");

   //  printf("RMS Voltage:\t %f V\n",  refMeterData.RMSVoltageVal);
     refMeterData.ActivePowerVal = powerVals.ActivePowerValue/1000;
   //  printf("Active Power:\t %f W\n", refMeterData.ActivePowerVal);

     refMeterData.ReactivePowerVal = powerVals.FundReactivePowerValue/1000;
   //  printf("Reactive Power:\t %f VAR\n", refMeterData.ReactivePowerVal);

     refMeterData.ApparentPowerVal = powerVals.ApparentPowerValue/1000;
   //  printf("Apparent Power:\t %f VA\n", refMeterData.ApparentPowerVal);
       
     refMeterData.PowerFactorVal =  pqVals.PowerFactorValue;
   //  printf("Power Factor:\t %f\n", refMeterData.PowerFactorVal);
     
     refMeterData.FrequencyVal = pqVals.FrequencyValue;
   //  printf("Frequenc
   


   /*
     float  rated_freq;  


    if(refMeterData.FrequencyVal>55)  rated_freq = 60.0;
    else                              rated_freq = 50.0;
 */
     
    


     if((refMeterData.FrequencyVal >  (rated_freq+ freq_margin)) &&  (refMeterData.FrequencyVal <  (rated_freq- freq_margin)))
     {
                 freq_error_en =1;
                 snprintf(pWarning->freq_error,  sizeof(warning.freq_error), "1");
     }  
     else 
     {
                 freq_error_en =0; 
                 snprintf(pWarning->freq_error,  sizeof(warning.freq_error), "0");  
     }



     if(prev_freq_error_en!=freq_error_en)
     {
               flags->freq_error = 1;  
               ret = true; 
               if(freq_error_en==1)
               {
                      snprintf(frequency, sizeof(frequency),  "%.2f",  refMeterData.FrequencyVal); 
               }
               else 
               {
                      snprintf(frequency, sizeof(frequency),  "0");  
               }                
    }
    prev_freq_error_en =  freq_error_en; 

   //  power_in =   gpio_get_level(PIN_NUM_POWER_IN);  

  // ESP_LOGI(TAG, "6");

 
   power_in =  ground_fault_status();
   
   if(power_in!=prev_power_in)
   {
           flags->ground_fault  = 1;
           ret = true; 
           if(power_in==1)
           {
                 snprintf(pWarning->ground_fault, sizeof(char)*6,  "0");
           }
           else 
           {
                 snprintf(pWarning->ground_fault, sizeof(char)*6,  "1");
           }
   }
   prev_power_in  =  power_in;

    //ESP_LOGI(TAG, "7");

   refMeterData.TemperatureVal = tempVal.TemperatureVal;
   return  ret;     
}
#endif 




void  gpio_relay_on(void)
{
     // PIN_NUM_RL_ON_1  LOW
     gpio_set_level(PIN_NUM_RL_ON_1,  LOW);
     // PIN_NUM_RL_ON_1  HIGH
  
     gpio_set_level(PIN_NUM_RL_ON_1,  HIGH); 
     vTaskDelay(pdMS_TO_TICKS(200)); // 200ms 대기

     // PIN_NUM_RL_ON_1  LOW
     gpio_set_level(PIN_NUM_RL_ON_1,  LOW);

     int  rl_off_1  =  gpio_get_level(PIN_NUM_RL_OFF_1);
     int  rl_on_1  =   gpio_get_level(PIN_NUM_RL_ON_1);
     ESP_LOGI(TAG, "rl_off_1 : %d    rl_on_1:%d : ON:L->H(200msec)->L", rl_off_1, rl_on_1);   
}





void  gpio_relay_off(void)
{
      // PIN_NUM_RL_OFF_1  LOW    
      gpio_set_level(PIN_NUM_RL_OFF_1,  LOW);

      // PIN_NUM_RL_OFF_1  HIGH
      gpio_set_level(PIN_NUM_RL_OFF_1,  HIGH);
     //  vTaskDelay(pdMS_TO_TICKS(10)); // 10ms 대기
      vTaskDelay(pdMS_TO_TICKS(200)); // 200ms 대기

      // PIN_NUM_RL_OFF_1  LOW    
      gpio_set_level(PIN_NUM_RL_OFF_1,  LOW);


      int  rl_off_1  =  gpio_get_level(PIN_NUM_RL_OFF_1);
      int  rl_on_1  =   gpio_get_level(PIN_NUM_RL_ON_1);
      ESP_LOGI(TAG, "rl_off_1 : %d    rl_on_1:%d  OFF:L->H(200msec)->L", rl_off_1, rl_on_1);   
}






void  gpio_relay_nop(void)
{
     int  rl_off_1  =  gpio_get_level(PIN_NUM_RL_OFF_1);
     int  rl_on_1  =   gpio_get_level(PIN_NUM_RL_ON_1);
     
     if(rl_off_1 != 0)
     {
         // PIN_NUM_RL_OFF_1  LOW
         gpio_set_level(PIN_NUM_RL_OFF_1,  LOW);
     }

     if(rl_on_1 !=0 ) 
     {
        gpio_set_level(PIN_NUM_RL_ON_1,   LOW);      
     }
}






void  relay_on(void)
{
     
      ESP_LOGI(TAG, "relay on(%d)", PIN_NUM_RL_ON_1);    
 #if 0     
      // PIN_NUM_RL_OFF_1  LOW
      gpio_set_level(PIN_NUM_RL_OFF_1,  LOW);
      //  PIN_NUM_RL_ON_1  HIGH
      gpio_set_level(PIN_NUM_RL_ON_1,  HIGH);
      vTaskDelay(pdMS_TO_TICKS(10)); // 10ms 대기
     int  rl_off_1  =  gpio_get_level(PIN_NUM_RL_OFF_1);
     int  rl_on_1  =   gpio_get_level(PIN_NUM_RL_ON_1);
     ESP_LOGI(TAG, "rl_off_1 : %d    rl_on_1:%d", rl_off_1, rl_on_1);   
#else 
     	ESP_LOGE(TAG, "set relay_state = true >>>>>>>>>>>>>>>>>>>>>>>>>");   
        relay_state=true;
#endif 
}


void  relay_off(void)
{
      ESP_LOGI(TAG, "relay off(%d)",  PIN_NUM_RL_OFF_1);     
#if 0
     //  PIN_NUM_RL_ON_1  LOW    
      gpio_set_level(PIN_NUM_RL_ON_1,  LOW);
      // PIN_NUM_RL_OFF_1  HIGH
      gpio_set_level(PIN_NUM_RL_OFF_1,  HIGH);
      vTaskDelay(pdMS_TO_TICKS(10)); // 10ms 대기
      int  rl_off_1  =  gpio_get_level(PIN_NUM_RL_OFF_1);
      int  rl_on_1  =   gpio_get_level(PIN_NUM_RL_ON_1);
      ESP_LOGI(TAG, "rl_off_1 : %d    rl_on_1:%d", rl_off_1, rl_on_1);   
#else 
     	ESP_LOGE(TAG, "set relay_state = false >>>>>>>>>>>>>>>>>>>>>>>>>");   
        relay_state=false;
#endif 
}








// ISR handler for ADE9153 IRQ

#if 0
//  static void IRAM_ATTR irq_isr_handler(void* arg) {
//      // Increment Zero Crossing counter
//         
//       //  static  bool  prev_relay_state;
//        extern  bool   prev_relay_state;
//       
//         zx_counter++;
//         
//         #if 1
//           if(relay_state!= prev_relay_state)
//           {
//             zero_flag = true;  
//             if(relay_state) 
//             {
//                   // PIN_NUM_RL_OFF_1  LOW
//                     gpio_set_level(PIN_NUM_RL_OFF_1, LOW);
//                   //  PIN_NUM_RL_ON_1  HIGH
//                     gpio_set_level(PIN_NUM_RL_ON_1,  HIGH);
//             }
//             else 
//             {
//                     //  PIN_NUM_RL_ON_1  LOW    
//                     gpio_set_level(PIN_NUM_RL_ON_1,  LOW);
//                     // PIN_NUM_RL_OFF_1  HIGH
//                     gpio_set_level(PIN_NUM_RL_OFF_1, HIGH); 
//             }
//              
//             prev_relay_state  =  relay_state;
//             relay_latch_flag  =  true; 
//           }  
//         #endif 
//      // Read the Status register to check the source of the interrupt
//  }
#else 
static void IRAM_ATTR zx_gpio_isr_handler(void* arg) {
	// Increment Zero Crossing counter
       
	//       //  static  bool  prev_relay_state;
	// org:
	//        extern  bool   prev_relay_state;
	//shcho
//  	//move to global
//  	 extern  int   prev_relay_state;
     
	zx_counter++;
       
	#if 0 // org
	if( relay_state != -1 ) // 초기값
	{
		if(relay_state != prev_relay_state)
		{
	//  		ESP_LOGI("shcho", "relay_state != prev_relay_state");
			zero_flag = true;  
			if(relay_state) 
			{
				// PIN_NUM_RL_OFF_1  LOW
				gpio_set_level(PIN_NUM_RL_OFF_1, LOW);
				//  PIN_NUM_RL_ON_1  HIGH
				gpio_set_level(PIN_NUM_RL_ON_1,  HIGH);
			}
			else 
			{
				//  PIN_NUM_RL_ON_1  LOW    
				gpio_set_level(PIN_NUM_RL_ON_1,  LOW);
				// PIN_NUM_RL_OFF_1  HIGH
				gpio_set_level(PIN_NUM_RL_OFF_1, HIGH); 
			}
			
//  			prev_relay_state  =  relay_state;
			relay_latch_flag  =  true; 
		}  
	}
	#else
	if( flag_relay_control == 1 )
	{
		flag_relay_control = 0 ;

		if(relay_state==true)     gpio_set_level(PIN_NUM_RL_ON_1,  HIGH);
		else                      gpio_set_level(PIN_NUM_RL_OFF_1, HIGH); 

		relay_latch_flag  =  true; 
	}
	#endif 
	// Read the Status register to check the source of the interrupt
}


static void IRAM_ATTR  int_gpio_isr_handler(void * arg) {

     uint16_t  gpio_num  = (uint16_t)arg;

     if(gpio_num==PIN_NUM_INT)  {
              check_dip_swell_event();
     }
}
#endif



int zx_gpio_irq_add()
{
	static int count = 0 ;
    esp_err_t err= gpio_isr_handler_add(PIN_NUM_ZX, zx_gpio_isr_handler, NULL);                           // GPIO_ZX_PIN
	ESP_LOGW("zx_gpio_irq_add: ", "serviced=%d(err=%s)", count++, esp_err_to_name(err) );
	return 1;
}
int zx_gpio_irq_remove()
{
	static int count = 0 ;
    esp_err_t err = gpio_isr_handler_remove(PIN_NUM_ZX);                           // GPIO_ZX_PIN
	ESP_LOGW("zx_gpio_irq_remove: ", "serviced=%d(err=%s)", count++, esp_err_to_name(err) );
	return 1;
}


void irq_init(void) {
	// Zero Crossing 핀 설정 (GPIO21, 하강 에지 인터럽트)  
	gpio_config_t io_conf = {
		.intr_type =  GPIO_INTR_NEGEDGE,   // Trigger  on falling edge
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL <<PIN_NUM_ZX),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
	};
	gpio_config(&io_conf);
    


    // DIP/SWELL 핀 설정 (GPIO13, 하강 에지 인터럽트) 
    gpio_config_t  io_conf_int =  {
       .intr_type = GPIO_INTR_NEGEDGE,  // 하강 에지에서 인터럽트 발생      
       .mode  =   GPIO_MODE_INPUT,
       .pin_bit_mask = (1ULL  <<PIN_NUM_INT),
       .pull_down_en = GPIO_PULLDOWN_DISABLE, 
       .pull_up_en  =  GPIO_PULLUP_ENABLE,  // 풀업 설정  
    };
    gpio_config(&io_conf_int); 
     

    // Install  ISR service
    //org: gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);
//      gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1); //계속 Reboot , Lowest Priority
//      gpio_install_isr_service(ESP_INTR_FLAG_SHARED); //계속 Reboot


//  //  gpio_isr_handler_add(PIN_NUM_ZX, irq_isr_handler, NULL);                           // GPIO_ZX_PIN
//shcho 필요한경우에만
//  	gpio_isr_handler_add(PIN_NUM_ZX, zx_gpio_isr_handler, NULL);                           // GPIO_ZX_PIN
    gpio_isr_handler_remove(PIN_NUM_INT); 


    gpio_isr_handler_add(PIN_NUM_INT, int_gpio_isr_handler, NULL); 

}






int powerRead()
{
 #if 0
    printf("RMS Current:\t %f A\n",  refMeterData.RMSCurrentVal);
    printf("RMS Voltage:\t %f V\n",  refMeterData.RMSVoltageVal);
    printf("Active Power:\t %f W\n", refMeterData.ActivePowerVal);
    printf("Reactive Power:\t %f VAR\n", refMeterData.ReactivePowerVal);
    printf("Apparent Power:\t %f VA\n", refMeterData.ApparentPowerVal);
    printf("Power Factor:\t %f\n", refMeterData.PowerFactorVal);
    printf("Frequency:\t %f Hz\n", refMeterData.FrequencyVal);
    printf("Temperature:\t %f degC",  refMeterData.TemperatureVal);
    printf("\n\n");
 #else 
    ESP_LOGI(TAG, "RMS Current: %f",     refMeterData.RMSCurrentVal); 
    ESP_LOGI(TAG, "RMS Voltage: %f",     refMeterData.RMSVoltageVal); 
    ESP_LOGI(TAG, "Active Power: %f",    refMeterData.ActivePowerVal);
    ESP_LOGI(TAG, "Reactive Power: %f",  refMeterData.ReactivePowerVal);
    ESP_LOGI(TAG, "Apparent Power: %f",  refMeterData.ApparentPowerVal);
    ESP_LOGI(TAG, "Power Factor: %f",    refMeterData.PowerFactorVal);
    ESP_LOGI(TAG, "Frequency: %f",       refMeterData.FrequencyVal);
    ESP_LOGI(TAG, "Temperature: %f",     refMeterData.TemperatureVal);
 #endif    
    return 0;
}


int  temperatureRead(void)
{
    ReadTemperature2(&tempVal);
    sendEvent("온도");
    printf("Temperature:\t %f degC",  tempVal.TemperatureVal);
    printf("\n\n");
    return 0; 
}






void sendEvent(const char* str)
{

     time_t time_in_seconds;
     //char *buffer=NULL;
	 // move to global
//        extern  uint8_t mesh_ap_ssid[]; // 

     snprintf(topic, sizeof(topic), "iotech/SEMS/%s/%s/event",  mesh_ap_ssid,device_id);
     time_t  current_time  = time(NULL);        
     setenv("TZ", "KST-9", 1);
     tzset();
     struct tm *local_time = localtime(&current_time);
     int  tm_year  = local_time->tm_year +1900;


     if(tm_year<2024)
     {
       printf("year: %d\n",  tm_year);
       return;
     }

     time_in_seconds =  mktime(local_time);
     char  datetime[20];
     strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S", local_time);

    if(strcmp(str, "온도")==0) {
        Event  *event  =  (Event *) meter_malloc_spiram(sizeof(Event));
        if (event  == NULL) {
           printf("Memory allocation failed\n");
           return; 
        }   
       // strcpy(event->dev_id,  device_id);
        strcpy(event->time_event, datetime);
        event->time_sec= (int64_t)time_in_seconds;
        event->direction= 0;
        strncpy(event->event_class, str,  sizeof(event->event_value)-1);
        event->event_class[sizeof(event->event_class) - 1] = '\0';
        snprintf(event->event_value, sizeof(event->event_value), "%.2f", tempVal.TemperatureVal); // Example temperature value

//          int json_len = serialize_event(event, buffer);
		// shcho add
		char *buffer_tmp;
        int json_len = serialize_event(event, &buffer_tmp);
        printf("event len=%zu\n", json_len);
        mqtt_app_publish(topic, buffer_tmp); 

//          free(buffer); //shcho  : buffer is global array
        free(buffer_tmp); //shcho  : buffer is global array

        free(event);
    }
}







void subtract_minutes(time_t *time, int minutes) {
    // time_t 포인터를 사용하여 현재 시간을 얻고 구조체로 변환
    struct tm *local_time = localtime(time);

    // 현재 시간 출력
    ESP_LOGI(TAG, "Current time: %02d:%02d:%02d", local_time->tm_hour, local_time->tm_min, local_time->tm_sec);

    // 분 단위로 뺄셈 수행
    local_time->tm_min -= minutes;

    // mktime을 사용하여 수정된 시간을 time_t 형식으로 변환 및 조정
    *time = mktime(local_time);

    // 조정된 시간 출력
    local_time = localtime(time); // 다시 변환하여 출력
    ESP_LOGI(TAG, "Adjusted time: %02d:%02d:%02d", local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
}


void subtract_30_days(const char* datetime,  char* result){
      struct tm tm;
      time_t t;

     // 문자열을 struct tm으로 파싱
     if(strptime(datetime,  "%Y-%m-%d %H:%M:%S",  &tm) == NULL  )  {
          printf("Invalid date format\n");
          return;
      } 

     // struct tm 을  time_t 로 뱐환 
     t = mktime(&tm);   
           
    //30 일(30 * 24 * 3600) 을 빼기 
     t -= 30 * 24 * 3600;

    //time_t 를 다시 stuct tm 으로 변환 
    struct tm *  new_tm = localtime(&t);
    
     // 결과를 "YYYY-MM-DD HH:MM:SS" 형식의 문자열로 변환
    strftime(result, 21, "%Y-%m-%d %H:%M:%S", new_tm);
}


void   check_power_status(void)
{
      power_in = gpio_get_level(PIN_NUM_POWER_IN);
      if(prev_power_in != power_in)
      {
          event_flag  =true;
          flags.power_status =1;  
          if(power_in)
          {
               sprintf(power_status, "on"); 
          } 
          else 
          {
               sprintf(power_status, "off");
          }
          prev_power_in  =  power_in;
      }   
}





char  datetime[20];







void  save_process(void)
{
          time_t current_time = time(NULL); // 현재 시간 가져오기
          struct tm *local_time = localtime(&current_time);
          strftime(datetime, 20, "%Y-%m-%d %H:%M:%S", local_time);
  
          ReadEnergyRegs(&energyVals);
          last_meter.wh   +=energyVals.ActiveEnergyValue/1000;
          last_meter.varh +=energyVals.FundReactiveEnergyValue/1000;
          last_meter.vah  +=energyVals.ApparentEnergyValue/1000;
          snprintf(last_meter.datetime, sizeof(last_meter.datetime), datetime);
          save_db(); 

         if (xTaskHandle_spi != NULL) {
         vTaskDelete(xTaskHandle_spi);  // 태스크 삭제
         xTaskHandle_spi = NULL;        // 핸들을 NULL로 설정하여 재사용 방지
}

}







void process_1min_tasks(time_t current_time) {
    
    
	// move to global
//      extern  uint8_t mesh_ap_ssid[]; // 

    int     tm_year=0;
    // 1분마다 실행되는 작업들
           time_1min = false;
    
       
          
          // Set timezone to Eastern Standard Time and print local time
          setenv("TZ", "KST-9", 1);
          tzset();
        

                  char  startdatetime[20]; 

                  if(cntMin==noti_period-1)
                  {
                        subtract_minutes(&current_time, noti_period);
                        struct tm *local_time = localtime(&current_time);
                        strftime(startdatetime, 20, "%Y-%m-%d %H:%M:%S", local_time);
                        tm_year =  local_time->tm_year+1900;
                        
                        if(tm_year<2024)
                        {
                            cntMin =0;     
                            return;  
                        }

                  }


                  ReadEnergyRegs(&energyVals);
            

                  refMeterData.ActiveEnergyVal= energyVals.ActiveEnergyValue/1000;
                  energyArray[cntMin].ActEnergy= refMeterData.ActiveEnergyVal;  
                  ESP_LOGI(TAG,"ActiveEnergy:\t %f Whr", refMeterData.ActiveEnergyVal);
                 

                  refMeterData.ReactiveEnergyVal= energyVals.FundReactiveEnergyValue/1000;
                  energyArray[cntMin].ReactEnergy=  refMeterData.ReactiveEnergyVal;
                  ESP_LOGI(TAG,"ReactiveEnergy:\t %f VARhr", refMeterData.ReactiveEnergyVal);

                  refMeterData.ApparentEnergyVal= energyVals.ApparentEnergyValue/1000;
                  energyArray[cntMin].ApparentEnergy=  refMeterData.ApparentEnergyVal; 
                  ESP_LOGI(TAG,"ApparentEnergy:\t %f VAhr", refMeterData.ApparentEnergyVal); 


                  ESP_LOGI(TAG,"Volt : %.1f",    refMeterData.RMSVoltageVal);
                  ESP_LOGI(TAG,"Current : %.3f", refMeterData.RMSCurrentVal);
                  ESP_LOGI(TAG,"Temp:\t %.2f",   refMeterData.TemperatureVal);
                  ESP_LOGI(TAG,"Watt:\t %f",     refMeterData.ActivePowerVal);
                  ESP_LOGI(TAG, "VAR:\t %f",     refMeterData.ReactivePowerVal);
                  ESP_LOGI(TAG, "VA:\t %f",      refMeterData.ApparentPowerVal);
              
                  ESP_LOGI(TAG,"V I angle: %ld, %f degree", pqVals.AngleReg_AV_AI , pqVals.AngleValue_AV_AI);
                  ESP_LOGI(TAG,"Frequency %f Hz", pqVals.FrequencyValue);

                  
                  for(int i=0; i<20; i++) 
                  {
                                refMeterData.DateTime[i] =  datetime[i];
                                energyArray[cntMin].DateTime[i]  =  datetime[i];
                                energyArray[0].DateTime[i] =  startdatetime[i];
                  }

                  cntMin++;
                  ESP_LOGI(TAG,"cntMin: %d", cntMin);
                    

                   if(cntMin>noti_period-1)
                   {
                        ESP_LOGI(TAG,"func energysum");
                        for(int i=0;  i<cntMin;  i++)
                        {
                            refAccumEnergy.ActEnergy   += energyArray[i].ActEnergy;
                            refAccumEnergy.ReactEnergy += energyArray[i].ReactEnergy; 
                            refAccumEnergy.ApparentEnergy +=  energyArray[i].ApparentEnergy;
                        }
                        ESP_LOGI(TAG,"Active Energy: %f ",   refAccumEnergy.ActEnergy);
                        ESP_LOGI(TAG,"Reactive Energy: %f ", refAccumEnergy.ReactEnergy);
                        ESP_LOGI(TAG,"Apparent Energy: %f ", refAccumEnergy.ApparentEnergy);
                        ESP_LOGI(TAG,"Start Time : %s ",     energyArray[0].DateTime);
                        ESP_LOGI(TAG,"End Time : %s ",       energyArray[noti_period-1].DateTime);
                        cntMin=0;
                      
                        last_meter.wh  +=  refAccumEnergy.ActEnergy;   
                        last_meter.varh += refAccumEnergy.ReactEnergy;
                        last_meter.vah += refAccumEnergy.ApparentEnergy;    
                        snprintf(last_meter.datetime, sizeof(last_meter.datetime) ,energyArray[noti_period-1].DateTime);


                       
                        save_db(); 
                         

                        db_size =  get_database_size(db_path); 
                        ESP_LOGI(TAG, "db size=%zu", db_size);
                      

                        Device *device = (Device *)meter_malloc_spiram(sizeof(Device));
                        char *device_buffer = (char *)meter_malloc_spiram(600 * sizeof(char));
                        if (device == NULL || device_buffer == NULL) {
                            ESP_LOGE(TAG, "device publish: alloc failed (device=%p buffer=%p)",
                                     (void *)device, (void *)device_buffer);
                            if (device_buffer) {
                                free(device_buffer);
                            }
                            if (device) {
                                free(device);
                            }
                        } else {
                        strcpy(device->time_start,  energyArray[0].DateTime);
                        strcpy(device->time_end,    energyArray[noti_period-1].DateTime);
                        device->time_start_sec = convert_to_seconds(energyArray[0].DateTime); 
                        device->time_end_sec = convert_to_seconds(energyArray[noti_period-1].DateTime);
                        snprintf(device->watt, sizeof(char)*20, "%.6f", refMeterData.ActivePowerVal);
                        snprintf(device->var, sizeof(char)*20, "%.6f", refMeterData.ReactivePowerVal); 
                        snprintf(device->va, sizeof(char)*20, "%.6f", refMeterData.ApparentPowerVal); 
                        snprintf(device->volt, sizeof(char)*20, "%.1f", refMeterData.RMSVoltageVal); 
                        snprintf(device->current_avg, sizeof(char)*20, "%.3f", refMeterData.RMSCurrentVal);
                        snprintf(device->wh_accu, sizeof(char)*20, "%.3f", refAccumEnergy.ActEnergy);
                        snprintf(device->varh_accu, sizeof(char)*20, "%.3f", refAccumEnergy.ReactEnergy);
                        snprintf(device->vah_accu, sizeof(char)*20, "%.3f", refAccumEnergy.ApparentEnergy);
                        snprintf(device->freq,  sizeof(char)*20, "%.1f", refMeterData.FrequencyVal); 
                        snprintf(device->pf,  sizeof(char)*20,  "%.5f",  refMeterData.PowerFactorVal);
                        snprintf(device->temp,  sizeof(char)*20, "%.2f", refMeterData.TemperatureVal);  
                        snprintf(device->thd,   sizeof(char)*20, "%.3f",  refMeterData.TotalHarmDistVal);    
                        snprintf(topic, sizeof(topic), "iotech/SEMS/%s/%s/device", mesh_ap_ssid,device_id);
                        int json_len = serialize_device(device, &device_buffer);
                        // int json_len = serialize_device(device, buffer);
                        ESP_LOGI(TAG, "device len=%zu", json_len);
                     //   mqtt_app_publish(topic, buffer);
                        if(tm_year>2023) mqtt_app_publish(topic, device_buffer);
                        free(device_buffer);
                        free(device);
                        }
                   }
 }

void process_1day_tasks(time_t current_time) {
    // 1일마다 실행되는 작업들
    time_1day = false;
    char cutoff_datetime[20];
    subtract_30_days(datetime, cutoff_datetime);
    delete_old_records(cutoff_datetime);
    db_size = get_database_size(db_path);
    ESP_LOGI(TAG, "db size=%zu", db_size);
}


void handle_arc_detection(void) {
    // 아크 감지 처리
    static bool prev_arc_flag;
    arc_flag = detect_arc_by_rms_change();
    if (arc_flag != prev_arc_flag) {
        prev_arc_flag = arc_flag;
        event_flag = true;
        flags.arc_alarm = true;
        snprintf(arc_status, sizeof(arc_status), "%s", arc_flag ? "on" : "off");
        ESP_LOGI("ARC", "%s", arc_status);
    }
}

int relay_control_all_off(void)
{
	gpio_set_level(PIN_NUM_RL_ON_1,  LOW);
	gpio_set_level(PIN_NUM_RL_OFF_1, LOW); 
	return 1;
}


int   loop(void) {
	struct timeval start_time;
	int delay = 500; // xQueueReceive()의 대기 시간을 500ms로 설정
	static bool measure_ready = false; 
	   
	static unsigned long lastReport = 0;
	static unsigned long arcReport = 0;
  
	char strtime[20];
	char topic[200];

	 // move to global
//       extern  uint8_t mesh_ap_ssid[]; 

     tx_data_t *tx_data;

	 // move to global
//  	 //org:
//  //       extern  bool  relay_latch_flag;
//  //       extern  bool  relay_state;
//  	//shcho:
//       extern  int  relay_latch_flag;
//       extern  int  relay_state;

     Warning *pWarning =  &warning; 
	 // move to global
//       extern  uint8_t mesh_ap_ssid[];
     
     static  int relay_cnt; 

     gettimeofday(&start_time, NULL);
       
    

	ESP_LOGW("loop_shcho: ", "loop enter////////////////////////////////");
    // 500ms 대기로 큐에서 메시지 수신
	if (xQueueReceive(txQue, &tx_data, pdMS_TO_TICKS(delay)) == pdPASS) {
		ESP_LOGW("loop_shcho: ", "xQueueReceive() == pdPASS  enter////////////////////////////////");
    	snprintf(topic, sizeof(topic), "iotech/SEMS/%s/%s/event", mesh_ap_ssid, device_id);
        mqtt_app_publish(topic, tx_data->data);
        ESP_LOGI(TAG, "Received(%d): %s", tx_data->length, tx_data->data);
        free(tx_data); // 수신 후 메모리 해제
	} else {
		// zx_gpio ... : xQueueSendFromISR로 처리하려고 기능을 다르게 구현함
		ESP_LOGW(TAG, "zx_counter: %ld,  zero_flag:  %d / relay_latch_flag=%d.", zx_counter, zero_flag, relay_latch_flag);  
		ESP_LOGW(TAG, "                  flag_relay_control:  %d", flag_relay_control);  
        //  irq_status();
        input_status();

        if(relay_latch_flag == true)
        {
	        relay_latch_flag=false;
	        flag_relay_control=false;


			zx_gpio_irq_remove();


			//여기서는 끄기만 한다.
//  			//shcho add
//  	        if(relay_state==true)     gpio_set_level(PIN_NUM_RL_ON_1,  HIGH);
//  	        else                      gpio_set_level(PIN_NUM_RL_OFF_1, HIGH); 

				//이미 Relay가 동작하고 있다.
//  		    vTaskDelay(pdMS_TO_TICKS(200)); // 200ms 대기
		    vTaskDelay(pdMS_TO_TICKS(200)); // 200ms 대기

	        int  rl_off_1  =  gpio_get_level(PIN_NUM_RL_OFF_1);
	        int  rl_on_1  =   gpio_get_level(PIN_NUM_RL_ON_1);
			//shcho LOGI ==> LOGE
	        ESP_LOGE(TAG_GPIO,"  rl_off_1:%d, rl_on_1:%d : add 200msec delay", rl_off_1, rl_on_1);   

			//ISR에서 여기로 이동 확실하게 여기서
			prev_relay_state  =  relay_state;

	        if(relay_state==true)
			{
				gpio_set_level(PIN_NUM_RL_ON_1,  LOW);

				Write_Relay("on");
			}
	        else
			{
				gpio_set_level(PIN_NUM_RL_OFF_1, LOW); 

				Write_Relay("off");
			}
			return 1;  

        }
     } 
     

     time_t current_time = time(NULL); // 현재 시간 가져오기
     struct tm *local_time = localtime(&current_time);
     strftime(datetime, 20, "%Y-%m-%d %H:%M:%S", local_time);
	 	


     if (time_1min == true) {
          process_1min_tasks(current_time); // 1분 주기 작업 함수 호출
          read_AV_WAVE();      
          // read_samples_with_dma();
           perform_fft_and_thd();
      }


     if(time_1day == true) {
           process_1day_tasks(current_time); // 1일 주기 작업 함수 호출
     } 




      if(event_flag== true)
      {
               snprintf(topic, sizeof(topic),   "iotech/SEMS/%s/%s/event", mesh_ap_ssid, device_id);
               int json_len = serialize_warning_event(flags);

               if(json_len!=0)
               {
                        ESP_LOGI(TAG, "warn event len=%zu", json_len);
                        mqtt_app_publish(topic, buffer);
               }
               event_flag = false;
      }
      else 
      {
                  double elapsed_time2 = difftime(current_time, previous_time); //경과 시간 계산
                  extern int   delay2;
                  if((elapsed_time2 >= delay2) &(measure_ready== true))  
                  {
                    snprintf(topic, sizeof(topic),   "iotech/SEMS/%s/%s/warning", mesh_ap_ssid,  device_id);
                     struct tm *local_time = localtime(&current_time);  
                     strftime(datetime, 20,  "%Y-%m-%d %H:%M:%S",  local_time);
                     strcpy(pWarning->time_warning,  datetime); 
                     pWarning->time_sec = convert_to_seconds(pWarning->time_warning); 
                     int json_len = serialize_warning(); 
                     ESP_LOGI(TAG, "warning= %s ", buffer);
                     mqtt_app_publish(topic,  buffer);
                     previous_time   = current_time;  
                  }
       }

       
       unsigned long currentReport = start_time.tv_sec * 1000 + start_time.tv_usec / 1000;

       if ((currentReport - lastReport) > reportInterval) {
            lastReport = currentReport;
            event_flag = readandwrite(&flags);
            measure_ready = true;
       }

       if ((currentReport - arcReport) > arcInterval) {
           // ESP_LOGI("ARC", "CHECK: %ld", currentReport - arcReport);
            arcReport = currentReport;
            handle_arc_detection(); // 아크 감지 처리 함수 호출
       }
       check_power_status(); // 전력 상태 체크


       return  0; 
}







portMUX_TYPE myMux = portMUX_INITIALIZER_UNLOCKED;


int  auto_calibration(void)
{
    meter_task_disable  = true;    
   // printf("Autocalibrating Current Channel\n");
    ESP_LOGI(TAG_AUTOCAL,"Autocalibrating Current......");
    
    
    StartAcal_AINormal();
   // printf("1:\n");
    ESP_LOGI(TAG_AUTOCAL,"Current(1)......");
    runLength(20);
    //printf("2:\n");
    ESP_LOGI(TAG_AUTOCAL,"Current(2)......");
    
    StopAcal();

//  printf("Autocalibrating Voltage Channel\n");
    ESP_LOGI(TAG_AUTOCAL,"Autocalibrating Voltage......");

    StartAcal_AV(); 
    ESP_LOGI(TAG_AUTOCAL,"Voltage(1)......");      
    runLength(40);
    ESP_LOGI(TAG_AUTOCAL,"Voltage(2)......");
    StopAcal();
    vTaskDelay(pdMS_TO_TICKS(100));
    ReadAcalRegs(&acalVals);
   


  /* 
    printf("AICC: %f\n", acalVals.AICC);
    printf("AICERT: %ld\n", acalVals.AcalAICERTReg);
    printf("AVCC:  %f\n", acalVals.AVCC);
    printf("AVCERT: %ld\n", acalVals.AcalAVCERTReg);

  */

    ESP_LOGI(TAG_AUTOCAL, "AICC: %f", acalVals.AICC); 
    ESP_LOGI(TAG_AUTOCAL, "AICERT: %ld", acalVals.AcalAICERTReg);  
    ESP_LOGI(TAG_AUTOCAL, "AVCC: %f", acalVals.AVCC);  
    ESP_LOGI(TAG_AUTOCAL, "AVCERT: %ld", acalVals.AcalAVCERTReg);


   // long Igain =   (-(acalVals.AICC / 838.190) - 1) * 134217728;
   // long Vgain = ((acalVals.AVCC / 13411.05) - 1) * 134217728;
    
    if((acalVals.AICC!=0)&&(acalVals.AVCC!=0))
    {
        ApplyAcal(-acalVals.AICC, acalVals.AVCC);
    }
   // printf("Autocalibration Complete");
    ESP_LOGI(TAG_AUTOCAL,"Autocalibration Complete......");
      
    vTaskDelay(pdMS_TO_TICKS(2000));
    meter_task_disable  = false; 

     return 0;
}





void  check_remove_db(char  datetime[])
{
        char cutoff_datetime[20];
        //  현재 날씨에서  30일  뺀 날짜 계산 
         subtract_30_days(datetime,  cutoff_datetime);
        // SQLite에서 해당 날짜 이전의 레코드 삭제
         delete_old_records(cutoff_datetime);
         db_size =  get_database_size(db_path);  
         ESP_LOGI(TAG, "db size=%zu", db_size);
}



void  runLength(long  seconds)
{
      struct  timeval  measure_time;

      //  Record the start time
      gettimeofday(&measure_time,  NULL);
    
      unsigned long  startReport  =  measure_time.tv_sec*1000  +  measure_time.tv_usec/1000;
      unsigned long  measureReport  = startReport;

      while(measureReport - startReport < (seconds*1000))  {
               // digitalWrite(LED, HIGH);
               vTaskDelay(pdMS_TO_TICKS(blinkInterval));      //Hold low for 500 milliseconds
               // digitalWrite(LED, LOW);   
               vTaskDelay(pdMS_TO_TICKS(blinkInterval));      //Hold low for 500 milliseconds 
               //printf("runlength: %d", ) 
               gettimeofday(&measure_time, NULL);
               measureReport = measure_time.tv_sec*1000  + measure_time.tv_usec/1000;

      }
}


int   eventFlag(void)
{
        event_flag  = true;
        flags.freq_error  = 1;
        return 0; 

}





















long convert_to_seconds(char* datetime) {
    struct tm t;
    time_t time_in_seconds;

    // datetime을 분석하여 tm 구조체에 할당
    sscanf(datetime, "%d-%d-%d %d:%d:%d",
           &t.tm_year, &t.tm_mon, &t.tm_mday,
           &t.tm_hour, &t.tm_min, &t.tm_sec);

    // 년도와 월을 tm 구조체의 형식에 맞게 조정
    t.tm_year -= 1900;
    t.tm_mon -= 1;
    t.tm_isdst = -1; // 일광 절약 시간 고려 안 함

    // mktime 함수 사용하여 time_t 타입으로 변환
    time_in_seconds = mktime(&t);
    return (long)time_in_seconds;
}


int  ground_fault_status(void)  {
    
   int  ret=   gpio_get_level(PIN_NUM_POWER_IN);
   return  ret;
}




//  FFT  크기를 계산하는  함수 
double calculate_magnitude(kiss_fft_cpx cpx) {
    return sqrt(cpx.r * cpx.r + cpx.i * cpx.i);
}



// THD 계산 함수
double calculate_thd(double *magnitudes, int fundamental_index, int n_harmonics) {
    double harmonic_sum = 0.0;
    for (int i = 2; i <= n_harmonics; i++) {
        int harmonic_index = fundamental_index * i;
        if (harmonic_index < FFT_SIZE / 2) {
            harmonic_sum += magnitudes[harmonic_index] * magnitudes[harmonic_index];
        }
    }
    return sqrt(harmonic_sum) / magnitudes[fundamental_index];
}



// 주파수 및 크기를 저장할 배열
double magnitudes[FFT_SIZE / 2];
// FFT 및 THD 계산 함수
void perform_fft_and_thd(void) {
      
      
      
      
      
      // PSRAM에 kiss_fft_cpx 배열 할당
      kiss_fft_cpx *in = (kiss_fft_cpx *)heap_caps_malloc(sizeof(kiss_fft_cpx) * FFT_SIZE, MALLOC_CAP_SPIRAM);
      kiss_fft_cpx *out = (kiss_fft_cpx *)heap_caps_malloc(sizeof(kiss_fft_cpx) * FFT_SIZE, MALLOC_CAP_SPIRAM);

     if (in == NULL || out == NULL) {
      printf("Failed to allocate FFT buffers in PSRAM\n");
      return;  // 메모리 할당 실패 처리
     }
      
      
      
      
      
       // KISS FFT 설정
       kiss_fft_cfg cfg = kiss_fft_alloc(FFT_SIZE, 0, NULL, NULL);

     //  ESP_LOGI(TAG, "fft 1.............");
      // FFT 입력 및 출력 배열 


       // DMA로 받은 샘플 데이터를  실수형으로  변환 
       for (int  i=0;  i< FFT_SIZE;  i++)   {
          in[i].r = (float)dma_buffer[i];   // 실수 부분에 DMA 샘플을   voltage_buf
       //     in[i].r = (float)voltage_buf[i];
            in[i].i = 0;  // 허수 부분은 0으로 설정
       }


    //  ESP_LOGI(TAG, "fft 2.............");

       // FFT 실행
       kiss_fft(cfg, in, out);

  

       
       // 주파수 및 크기 계산
       for (int i = 0; i < FFT_SIZE / 2; i++) {
        // 주파수 계산
        double frequency = i * (double)SAMPLE_RATE / FFT_SIZE;

        // 크기 계산 (실수부와 허수부의 제곱합의 제곱근)
        double magnitude = calculate_magnitude(out[i]);
        magnitudes[i] = magnitude;

        // 결과 출력 (UART로 출력)
    //    printf("Frequency: %.2f Hz, Magnitude: %.5f\n", frequency, magnitude);
      }



   //   ESP_LOGI(TAG, "fft 3.............");


      // 기본 주파수의 인덱스 계산 (60Hz에 해당하는 인덱스) 
      int fundamental_index = (FREQ_60HZ * FFT_SIZE) / SAMPLE_RATE;

      // THD 계산 (기본 주파수에 대해 2차 ~ 5차 고조파 계산)
      double thd = calculate_thd(magnitudes, fundamental_index, 5);

      refMeterData.TotalHarmDistVal   = thd*100;

     //ESP_LOGI(TAG, "fft 4.............");

   // THD 출력
     //  printf("THD: %.5f%%\n", thd * 100);

      ESP_LOGI(TAG, "THD: %.5f%%", refMeterData.TotalHarmDistVal);

      //메모리 해제 
 //  #endif 
    
     // ESP_LOGI(TAG, "fft 5.............");
  

      free(cfg); 
      heap_caps_free(in);
      heap_caps_free(out); 

}  


