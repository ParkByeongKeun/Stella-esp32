#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "json_struct.h"



typedef struct STRUCT_METERING_DATA {
 float  RMSCurrentVal;
 float  RMSVoltageVal;
 float  ActivePowerVal;
 float  ReactivePowerVal;
 float  ApparentPowerVal;
 float  PowerFactorVal;
 float  ActiveEnergyVal;
 float  ReactiveEnergyVal;
 float  ApparentEnergyVal;
 float  FrequencyVal;
 float  TemperatureVal;
 float  TotalHarmDistVal; 
 char   DateTime[20];
}strMeterData;


typedef struct STRUCT_ACCUM_ENERGY{
 char   TimeStart[20];
 char   TimeEnd[20];
 double ActEnergy;
 double ReactEnergy;
 double ApparentEnergy;
}strAccumEnergy;



typedef struct STRUCT_ENERGY{
   char    DateTime[20];
   double  ActEnergy;
   double  ReactEnergy;
   double  ApparentEnergy;
}strEnergy; 

void app_measure(void *pvParameters);
int  powerRead();
int  temperatureRead(void);
int  auto_calibration(void);
long convert_to_seconds(char* datetime); 
void relay_on(void);
void relay_off(void);
void irq_init(void);
int eventFlag(void);

int  ground_fault_status(void);  
void check_remove_db(char  datetime[]);


//void readandwrite(Warning *warning);
//bool  readandwrite(Warning *warning, update_flags_t *flags);
bool  readandwrite(update_flags_t *flags);

//  esp_err_t Write_Relay(void);
esp_err_t Write_Relay(char *str);

void  gpio_relay_on(void);
void  gpio_relay_off(void);
void  gpio_relay_nop(void);
void  perform_fft_and_thd(void);
void  save_process(void);



//org:  extern   bool  relay_state;
//shcho 
extern   int  relay_state;

#ifdef __cplusplus
}
#endif
