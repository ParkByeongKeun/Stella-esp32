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
#include  <math.h>
#include  <unistd.h> // Include this header for usleep
#include  "ade9153a_spi.h"
#include  "kiss_fft.h"
#include "freertos/portmacro.h"

// portMUX_TYPE 뮤텍스 선언 및 초기화
portMUX_TYPE my_mux = portMUX_INITIALIZER_UNLOCKED;

#define STR_MATCH	(0)





float   CAL_IRMS_CC   =   0.1394286;
float   CAL_VRMS_CC   =   17.65897;
float   CAL_ENERGY_CC =  	0.187999;
float   CAL_POWER_CC  =   330.4462;
      

char  str_cal_irms_cc[10]  ="0.1394286";
char  str_cal_vrms_cc[10]  ="17.65897";
char  str_cal_energy_cc[10]="0.187999";
char  str_cal_power_cc[10] ="330.4462"; 
// Counter for Zero Crossing events

extern char  str_rated_voltage[]; 
extern char  str_rated_current[];
extern char  str_rated_freq[];
extern char  str_swell_voltage[];
extern char  str_dip_voltage[];
extern char  str_over_current[];
extern char  str_warning_duration[];
extern char  str_relay[]; 

int     rated_voltage= 220;
int     rated_current= 20;
extern  uint16_t  rated_freq;
float   over_current= 22.0;
int     warning_duration= 30;  
int	flag_AV_WAVE_work = 0 ;




char  dip_status[6];
char  swell_status[6];


int    rl_sw_in = 0;
volatile  int    power_in=0;
volatile  int    power_out= 0;


//uint32_t  vdip_lvl_value  =  0x0C8A;            // DIP 감지 임계값(192V)
uint32_t  vdip_lvl_value  =  0x734A9B;            // DIP 감지 임계값(198V)

//uint32_t  vswell_lvl_value  =  0x0F32;          // SWELL  감지 임계값(242V)
uint32_t  vswell_lvl_value  =  0x8C8A0C;          // SWELL  감지 임계값(242V)

bool      dip_flag  = false;
bool      swell_flag  = false;




static const char *TAG_GPIO = "gpio";

static const char *TAG_METER = "A9153A";

ade9153A_spi_driver_t * spi_driver_ptr;

uint16_t  dip_voltage= 198;    // DIP 임계 전압  (198V)
uint16_t  swell_voltage= 242;  //  SWELL  임계 전압  (242V)      



#if 0
  //  uint32_t dma_buffer[FFT_SIZE];  // DMA로 읽을 샘플 버퍼
uint32_t * dma_buffer;  // DMA로 읽을 샘플 버퍼
uint32_t tx_buffer[FFT_SIZE+1];//
#else 
//uint8_t  tx_buffer[FFT_SIZE +2] = {0};
//uint8_t  dma_buffer[FFT_SIZE+2] = {0};
//uint8_t  *dma_buffer;
//uint8_t  *tx_buffer;
WORD_ALIGNED_ATTR  uint32_t tx_buffer[8];
WORD_ALIGNED_ATTR  int32_t dma_buffer[FFT_SIZE];

//int32_t   voltage_buf[FFT_SIZE]; 

#endif 



void      ade9153a_reset(void);
void      SPI_Write_16(spi_device_handle_t spi, uint16_t Address, uint16_t Data);
void      SPI_Write_32(spi_device_handle_t spi,  uint16_t Address,  uint32_t Data);
uint16_t  SPI_Read_16(spi_device_handle_t spi,  uint16_t  Address);
uint32_t  SPI_Read_32(spi_device_handle_t spi,  uint16_t  Address);
extern  esp_err_t iotech_get_nvs_str(uint8_t *key, uint8_t *value, size_t value_cap);
extern  esp_err_t iotech_set_nvs_str(uint8_t *key, uint8_t *value);

// Counter for Zero Crossing events
volatile uint32_t zx_counter = 0;
volatile uint32_t zxtimeout_counter = 0;











 esp_err_t ReadConstant(void)
{
      esp_err_t  ret=ESP_OK;
      ret=iotech_get_nvs_str((uint8_t*)"cal_irms_cc", (uint8_t*)str_cal_irms_cc, sizeof(str_cal_irms_cc));
      CAL_IRMS_CC=  strtof(str_cal_irms_cc, NULL);
      //printf("cal_irms_cc: %s,  %f\n", str_cal_irms_cc, CAL_IRMS_CC);
      ESP_LOGI(TAG_METER,"cal_irms_cc: %s,  %f", str_cal_irms_cc, CAL_IRMS_CC);  
      
      ret=iotech_get_nvs_str((uint8_t*)"cal_vrms_cc", (uint8_t*)str_cal_vrms_cc, sizeof(str_cal_vrms_cc));
      CAL_VRMS_CC=  strtof(str_cal_vrms_cc, NULL);
      //printf("cal_vrms_cc: %s, %f\n", str_cal_vrms_cc, CAL_VRMS_CC);
      ESP_LOGI(TAG_METER,"cal_vrms_cc: %s, %f", str_cal_vrms_cc, CAL_VRMS_CC);
      
      ret=iotech_get_nvs_str((uint8_t*)"cal_energy_cc", (uint8_t*)str_cal_energy_cc, sizeof(str_cal_energy_cc));
      CAL_ENERGY_CC= strtof(str_cal_energy_cc, NULL);
      //printf("cal_energy_cc: %s, %f\n", str_cal_energy_cc, CAL_ENERGY_CC);
      ESP_LOGI(TAG_METER,"cal_energy_cc: %s, %f", str_cal_energy_cc, CAL_ENERGY_CC);
     
      ret=iotech_get_nvs_str((uint8_t*)"cal_power_cc", (uint8_t*)str_cal_power_cc, sizeof(str_cal_power_cc));
      CAL_POWER_CC = strtof(str_cal_power_cc,  NULL);
      //printf("cal_power_cc: %s, %f\n", str_cal_power_cc, CAL_POWER_CC);
      ESP_LOGI(TAG_METER,"cal_power_cc: %s, %f", str_cal_power_cc, CAL_POWER_CC);
      return ret; 
}


esp_err_t Write_CAL_IRMS_CC(void)
{
    esp_err_t ret=ESP_OK;
    sprintf(str_cal_irms_cc, "%.7f", CAL_IRMS_CC); 
    ret =  iotech_set_nvs_str((uint8_t*)"cal_irms_cc",   (uint8_t*)str_cal_irms_cc);
    return ret;
}

esp_err_t Write_CAL_VRMS_CC(void)
{
    esp_err_t ret=ESP_OK;
    sprintf(str_cal_vrms_cc,"%.7f", CAL_VRMS_CC);
    ret =  iotech_set_nvs_str((uint8_t*)"cal_vrms_cc",  (uint8_t*)str_cal_vrms_cc);
    return ret;
}

esp_err_t Write_CAL_ENERGY_CC(void)
{
    esp_err_t ret=ESP_OK;
    sprintf(str_cal_energy_cc, "%.7f", CAL_ENERGY_CC);
    ret = iotech_set_nvs_str((uint8_t*)"cal_energy_cc", (uint8_t*)str_cal_energy_cc);
    return ret;
}

esp_err_t  Write_CAL_POWER_CC(void)
{
     esp_err_t ret=ESP_OK;
     sprintf(str_cal_power_cc, "%.7f",  CAL_POWER_CC); 
     ret = iotech_set_nvs_str((uint8_t*)"cal_power_cc", (uint8_t*)str_cal_power_cc);
     return ret;
}

esp_err_t  Write_Rated_Voltage(void)
{
   esp_err_t  ret=ESP_OK;
   sprintf(str_rated_voltage,  "%d",   rated_voltage);   
   ret = iotech_set_nvs_str((uint8_t*)"rated_voltage", (uint8_t*)str_rated_voltage); 
   return ret;
}


esp_err_t  Write_Rated_Current(void)
{
     esp_err_t  ret=ESP_OK; 
     sprintf(str_rated_current,  "%d",    rated_current);
     ret = iotech_set_nvs_str((uint8_t*)"rated_current", (uint8_t*)str_rated_current);
     return ret;
}



esp_err_t Write_Rated_Freq(void)
{
     esp_err_t  ret=ESP_OK;
     sprintf(str_rated_freq,  "%d",   rated_freq);
     ret = iotech_set_nvs_str((uint8_t*)"rated_freq",  (uint8_t*)str_rated_freq);
     return ret;
}



esp_err_t Write_Swell_Voltage(void)
{
     esp_err_t  ret=ESP_OK; 
     sprintf(str_swell_voltage, "%d",  swell_voltage);
     ret = iotech_set_nvs_str((uint8_t*)"swell_voltage", (uint8_t*)str_swell_voltage);
     return ret; 
}


esp_err_t Write_Dip_Voltage(void)
{
     esp_err_t ret=ESP_OK;
     sprintf(str_dip_voltage, "%d",  dip_voltage);
     ret =  iotech_set_nvs_str((uint8_t*)"dip_voltage",  (uint8_t*)str_dip_voltage);
     return ret;
}


esp_err_t  Write_Over_Current(void)
{
     esp_err_t  ret=ESP_OK;
     sprintf(str_over_current, "%f", over_current);
     ret =  iotech_set_nvs_str((uint8_t*)"over_current",  (uint8_t*)str_over_current); 
     return  ret;
}

esp_err_t Write_Warning_Duration(void)
{
     esp_err_t ret=ESP_OK;
     sprintf(str_warning_duration, "%d",  warning_duration);
     ret = iotech_set_nvs_str((uint8_t*)"warning_duration",  (uint8_t*)str_warning_duration);
     return  ret;
}

//  esp_err_t Write_Relay(void)
esp_err_t Write_Relay(char *str_relay)
{
	volatile  int    power_out= 0;

	power_out=   gpio_get_level(PIN_NUM_POWER_OUT);

     esp_err_t  ret=ESP_OK;

	 if( ( power_out == 1  && ( strcmp(str_relay, "on" ) == STR_MATCH ) )
	  || ( power_out == 0  && ( strcmp(str_relay, "off") == STR_MATCH ) ) )
	 {
		 ESP_LOGW("Write_Relay:", " nvs_str : relay_con --> %s", str_relay);
	     ret =  iotech_set_nvs_str((uint8_t*)"relay_con",  (uint8_t*)str_relay);
	 }
	 else
	 {
	 	ESP_LOGE("Write_Relay: ", "Relay Control : doesnot match user wanted(%s):real(%d)  ", str_relay, power_out);
	 	ESP_LOGE("Write_Relay: ", "Relay Control : doesnot match user wanted(%s):real(%d)  ", str_relay, power_out);
	 	ESP_LOGE("Write_Relay: ", "Relay Control : doesnot match user wanted(%s):real(%d)  ", str_relay, power_out);
	 	ESP_LOGE("Write_Relay: ", "Relay Control : doesnot match user wanted(%s):real(%d)  ", str_relay, power_out);
		ret = -1;
	 }

     return  ret;
}


void  irq_status(void)
{
    uint32_t status;
    status= SPI_Read_32(spi_driver_ptr->handle,  REG_STATUS);

    //  Check if the intterupt was due to a zero crossing  timeout 
    if(status  & (1 << 21))  {  // Check if ZXTOAV bit is set      
        zxtimeout_counter++;
    }   
 
    // Check if the interrupt was due to a zero crossing on current channel A
 
 
    if(status  & (1 << 19))  {  //check if ZXAI bit is set    
       zx_counter++;
    }
}









void  gpio_init(void) {
//    
     gpio_config_t io_conf;
         
     #if 0
     gpio_reset_pin(PIN_NUM_RL_OFF_1);
     gpio_reset_pin(PIN_NUM_RL_ON_1);
 
   // 핀을 출력 모드로 설정
    esp_err_t err_off_1 = gpio_set_direction(PIN_NUM_RL_OFF_1, GPIO_MODE_OUTPUT);
    esp_err_t err_on_1 = gpio_set_direction(PIN_NUM_RL_ON_1, GPIO_MODE_OUTPUT);

    if (err_off_1 != ESP_OK) {
        ESP_LOGI(TAG_GPIO, "Failed to set direction for PIN_NUM_RL_OFF_1: %d", err_off_1);
    }
    if (err_on_1 != ESP_OK) {
        ESP_LOGI(TAG_GPIO, "Failed to set direction for PIN_NUM_RL_ON_1: %d", err_on_1);
    }

 #else
    gpio_reset_pin(PIN_NUM_RL_OFF_1);
  //  gpio_set_pull_mode(PIN_NUM_RL_OFF_1, GPIO_PULLDOWN_ONLY);  // PullUp, PullDown 모두 비활성화
    gpio_reset_pin(PIN_NUM_RL_ON_1);
    // PullUp 비활성화 (PullDown도 비활성화)
  //  gpio_set_pull_mode(PIN_NUM_RL_ON_1, GPIO_PULLDOWN_ONLY);   // PullUp, PullDown 모두 비활성화
    ESP_LOGI(TAG_GPIO, "before OUTPUTset");
    io_conf.intr_type  =  GPIO_INTR_DISABLE;    //인터럽트 비활성화
    io_conf.mode  =  GPIO_MODE_INPUT_OUTPUT ;          //출력 모드로 설정 
    // io_conf.mode  =  GPIO_MODE_OUTPUT ;          //출력 모드로 설정 
    io_conf.pin_bit_mask = (1ULL << PIN_NUM_RL_OFF_1)|(1ULL << PIN_NUM_RL_ON_1)|(1ULL << PIN_NUM_RUN_LED) ;  //출력 핀 선택
  

     io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;   //풀다운 비활성화  
     io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;      //풀업 비활성화  
     //ESP_ERROR_CHECK(gpio_config(&io_conf) );
      gpio_config(&io_conf);
      ESP_LOGI(TAG_GPIO, "after  OUTPUTset");

#endif 
    ESP_LOGI(TAG_GPIO, "before INPUTset");
     // GPIO 일반 입력 핀 설정   
    io_conf.mode  =  GPIO_MODE_INPUT;      // 입력 모드로 설정 
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;  // 풀업 비활성화
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;  // 풀다운 비활성화
    io_conf.pin_bit_mask  =  (1ULL << PIN_NUM_RL_SW_IN)|(1ULL << PIN_NUM_POWER_IN)|(1ULL << PIN_NUM_POWER_OUT);    //입력 모드로 설정  
  //  ESP_ERROR_CHECK(gpio_config(&io_conf)); 
    gpio_config(&io_conf);
//  gpio_set_level( PIN_NUM_RUN_LED, 1 );
//   int  run_led =  gpio_get_level(PIN_NUM_RUN_LED);
    ESP_LOGI(TAG_GPIO, "before Relay ON:");

#if 0
  esp_err_t err_off_1=   gpio_set_level( PIN_NUM_RL_ON_1, 1 );
  esp_err_t err_on_1=   gpio_set_level( PIN_NUM_RL_OFF_1, 0 );
  esp_err_t err_RUN_1=   gpio_set_level( PIN_NUM_RUN_LED, 0 );
  ESP_LOGI(TAG_GPIO, "after  Relay ON:");

  int  run_status=             gpio_get_level(PIN_NUM_RUN_LED);

  ESP_LOGI(TAG_GPIO, "RUN_LED: %d  ", run_status);
  if (err_off_1 != ESP_OK) {
       ESP_LOGI(TAG_GPIO, "Failed to set level for PIN_NUM_RL_OFF_1: %d", err_off_1);
  }
  if (err_on_1 != ESP_OK) {
        ESP_LOGI(TAG_GPIO,  "Failed to set level for PIN_NUM_RL_ON_1: %d", err_on_1);
  }
#endif 
     
 
       
}


void  input_status(void)  {
// 각 입력 핀의 값을 읽음 
     
  #if 0  
      rl_sw_in =   gpio_get_level(PIN_NUM_RL_SW_IN);
      power_in =   gpio_get_level(PIN_NUM_POWER_IN);         
      power_out=   gpio_get_level(PIN_NUM_POWER_OUT);  
      ESP_LOGI(TAG_GPIO,"RL_SW_IN:%d, POWER_IN:%d, POWER_OUT:%d ", rl_sw_in, power_in, power_out);  
 #else 
      int  rl_off_1  =  gpio_get_level(PIN_NUM_RL_OFF_1);
      int  rl_on_1  =   gpio_get_level(PIN_NUM_RL_ON_1);
      rl_sw_in =   gpio_get_level(PIN_NUM_RL_SW_IN);
      power_in =   gpio_get_level(PIN_NUM_POWER_IN);         
      power_out=   gpio_get_level(PIN_NUM_POWER_OUT);
      ESP_LOGI(TAG_GPIO,"RL_SW_IN:%d, POWER_IN:%d, POWER_OUT:%d, rl_off_1:%d, rl_on_1:%d", rl_sw_in, power_in, power_out, rl_off_1, rl_on_1);   
 #endif 

}







//  void spi_init(void)
void spi_init_mesh(void)
{
   esp_err_t ret;
    spi_bus_config_t buscfg  = {
      .miso_io_num = PIN_NUM_MISO,
      .mosi_io_num = PIN_NUM_MOSI,
      .sclk_io_num = PIN_NUM_CLK,
      .quadwp_io_num =  -1,
      .quadhd_io_num =  -1,
      .max_transfer_sz = 4096,
    };  


    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 10*1000*1000, //Clock out at 1 MHz
        .mode = 3,                      //SPI mode 0
        .spics_io_num = PIN_NUM_CS,     //CS pin
        .queue_size = 7,                //We want to be able to queue 7 transactions at a time
        .command_bits = 16,
    };

     // Initialize the SPI bus
     ret = spi_bus_initialize(SPI3_HOST,  &buscfg, SPI_DMA_CH_AUTO);
     ESP_ERROR_CHECK(ret);
            
    // Allocate memory for spi_driver_ptr
       spi_driver_ptr = (ade9153A_spi_driver_t *)malloc(sizeof(ade9153A_spi_driver_t));
       if(spi_driver_ptr == NULL) {
           ESP_LOGE("SPI", "Failed to allocate memory  for SPI driver");
           return; 
       }  

       // Attach the SPI device
      ret =  spi_bus_add_device(SPI3_HOST,  &devcfg,  &(spi_driver_ptr->handle));

     // dma_buffer =  (uint8_t *) heap_caps_malloc(FFT_SIZE+2, MALLOC_CAP_SPIRAM|MALLOC_CAP_DMA); 
     // tx_buffer =  (uint8_t *) heap_caps_malloc(FFT_SIZE+2, MALLOC_CAP_SPIRAM|MALLOC_CAP_DMA); 


      ESP_ERROR_CHECK(ret);
}


void  read_AV_WAVE(void)
{
    uint32_t   ret=0, ret1=0, ret2=0;
    uint16_t   config1=0, config3=0;
    int32_t    voltage_temp;

//  	while( flag_AV_WAVE_work != 0 )
//  	{
//  		ESP_LOGW("shcho: ", " wait for flag_AV_WAVE_work == 0 ");
//  	    vTaskDelay(pdMS_TO_TICKS(1000));
//  	}
		

	flag_AV_WAVE_work = 1 ;
   

    config1=  SPI_Read_16(spi_driver_ptr->handle,  REG_CONFIG1);
    config3=  SPI_Read_16(spi_driver_ptr->handle,  REG_CONFIG3); 

    SPI_Write_16(spi_driver_ptr->handle,   REG_CONFIG3, 0x000d); // Phase A & B  voltage/current peak detection
    config3  =  SPI_Read_16(spi_driver_ptr->handle,   REG_CONFIG3); 
    ESP_LOGI(TAG_METER, "CONFIG3: %04X (after set 0x000d)", config3);

    for( int i = 0 ; i < 3 ; i ++ ) 
	  {
	     ret   =  SPI_Read_32(spi_driver_ptr->handle,   REG_AV_WAV);
	     ret1  =  SPI_Read_32(spi_driver_ptr->handle,   REG_AV_WAV_1);
         ret2  =  SPI_Read_32(spi_driver_ptr->handle,   REG_VPEAK);
	     ESP_LOGI(TAG_METER, "AV_WAV:  %08lX   AV_WAV_1:  %08lX   V_PEAK: %08lX", ret, ret1, ret2);
	  }

     //   Burst  address autoincrement됨, 그래서 계속 CMD를 보내야함
     spi_transaction_t  t;
     memset(&t,  0,  sizeof(t));

     memset(tx_buffer,  0x00, sizeof(tx_buffer)); 
     memset(dma_buffer,  0x00,  sizeof(dma_buffer));


     uint16_t command  = (REG_AV_WAV_1 << 4) | 0x08;  // AV_WAV (0x601) 레지스터에서 읽기 명령어  
     
     //gpio_intr_disable(PIN_NUM_ZX);  // PIN_NUM_ZX 핀의 인터럽트 비활성화
     // 크리티컬 섹션 시작
   // 크리티컬 섹션 시작
    portENTER_CRITICAL(&my_mux);  
  
     for( int i = 0 ; i < FFT_SIZE ; ) 
     {
              t.cmd = command;
              t.length = 32;       // TX 및 RX 길이 (32비트 명령어 + 1024 샘플)
              t.tx_buffer = (char *)&tx_buffer[0];
	            t.rx_buffer = (char *)&dma_buffer[i];    // DMA 버퍼 

              esp_err_t  ret =  spi_device_polling_transmit(spi_driver_ptr->handle, &t);  //  DMA 전송


              if(i == 0 )
              {
                    i++;                 
              }   
              else   // (i > 0)
              {
                   if(dma_buffer[i] != dma_buffer[i-1] )
                   {
                         i++;  
                   }
              }
      }
     // gpio_intr_enable(PIN_NUM_ZX);   // PIN_NUM_ZX 핀의 인터럽트 활성화
     // 크리티컬 섹션 종료
     // portEXIT_CRITICAL(); 
    // 크리티컬 섹션 종료
    portEXIT_CRITICAL(&my_mux);

	flag_AV_WAVE_work = 0 ;


       char * c=(char*)&dma_buffer;;
       int ii=0;
    

       int32_t  dma_tmp; 

       for(int i=0;   i<(FFT_SIZE*4); i+=4)
       {
   //          voltage_buf[ii++] =  c[i+0]<<24|c[i+1]<<16|c[i+2]<<8|c[i+3];  
               dma_tmp  =  c[i+0]<<24|c[i+1]<<16|c[i+2]<<8|c[i+3];
   //            printf("%ld\n", dma_tmp);
               dma_buffer[ii++] =  dma_tmp;
       }
}


#if 0


// DMA로 AD9153A에서 샘플 데이터 읽기
void read_samples_with_dma() {

        spi_device_acquire_bus(spi_driver_ptr->handle, portMAX_DELAY);  // SPI 버스 잠금

       spi_transaction_t t;
       memset(&t,  0, sizeof(t)); 
  
      uint16_t command = (REG_AV_WAV_1 << 4) | 0x08;  // AV_WAV (0x601) 레지스터에서 읽기 명령어
      //tx_buffer[FFT_SIZE+1] = {0};           // Full Duplex에서는 TX도 필요하므로 더미 데이터
      
      memset(tx_buffer, 0x00, sizeof(uint32_t)*(FFT_SIZE+1));
      //memset(dma_buffer, 0x00, sizeof(uint32_t)*(FFT_SIZE));
      tx_buffer[0] =  command;  // ADE9153A 레지스터 주소 읽기 명령어 (16비트)
      t.length = (FFT_SIZE + 1) * 32;       // TX 및 RX 길이 (32비트 명령어 + 1024 샘플)
      t.tx_buffer = tx_buffer;
      t.rxlength = FFT_SIZE * 32;  // 1024 샘플 * 32비트 (4바이트)
      t.rx_buffer = dma_buffer;    // DMA 버퍼 
  

      //esp_err_t  ret =  spi_device_polling_transmit(spi_driver_ptr->handle, &t);  //  DMA 전송
      esp_err_t  ret =  spi_device_transmit(spi_driver_ptr->handle, &t);  
      assert(ret == ESP_OK);             // 오류 처리
           // DMA 버퍼의 데이터를 처리 (FFT, THD 계산 등)
      for (int i = 1; i < FFT_SIZE; i++) {  // 첫 번째 데이터는 명령어가 들어가므로 제외
          printf("Sample[%d]: %08lX\n", i - 1, dma_buffer[i]);
      }
      spi_device_release_bus(spi_driver_ptr->handle);  // SPI 버스 해제
}


#else 
// DMA로 AD9153A에서 샘플 데이터 읽기


int read_samples_with_dma() {
   //   spi_device_acquire_bus(spi_driver_ptr->handle, portMAX_DELAY);  // SPI 버스 잠금
     
    volatile uint16_t config1_value ;
   	//=======================================================
   	// set BURST_EN=1
       config1_value = SPI_Read_16(spi_driver_ptr->handle,   REG_CONFIG1);
       ESP_LOGI("shcho_spi_test", "config1(burst_en=1:before ): %04X", config1_value);
       config1_value |= 0x0800; // BURST_EN=1
       ESP_LOGI("shcho_spi_test", "config1(burst_en=1:after  ): %04X", config1_value);
	     SPI_Write_16(spi_driver_ptr->handle,  REG_CONFIG1,  config1_value);
	  //=======================================================

     
      spi_transaction_t t;
     // uint8_t  *tx_buffer;   
     // uint8_t  *dma_buffer; 
      memset(&t,  0, sizeof(t)); 

        
      size_t free_internal_memory = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
      printf("Available internal memory: %d bytes\n", free_internal_memory);

     

      memset(tx_buffer,  0x00, sizeof(tx_buffer));   // 추가로 데이터를 채우기 위한 TX 버퍼 초기화
      memset(dma_buffer, 0x00, sizeof(dma_buffer));     // DMA RX 버퍼 초기화 

      uint16_t command = (REG_AV_WAV_1 << 4) | 0x08;  // AV_WAV (0x601) 레지스터에서 읽기 명령어


      t.cmd = command;

      t.length = (FFT_SIZE) * 8;       // TX 및 RX 길이 (16비트 명령어 + 1024 샘플)
        
      t.tx_buffer = tx_buffer;
    
   //   t.rxlength = (FFT_SIZE+1) * 32;  // 1024 샘플 * 32비트 (4바이트)
      t.rx_buffer = dma_buffer;    // DMA 버퍼 
  
      ESP_LOGW("spi_dma_test", "before spi_device_polling_transmit()");
      esp_err_t  ret =  spi_device_polling_transmit(spi_driver_ptr->handle, &t);  //  DMA 전송
     // esp_err_t  ret =  spi_device_transmit(spi_driver_ptr->handle, &t);  //  DMA 전송
      ESP_LOGW("spi_dma_test", "after  spi_device_polling_transmit()");
         
      assert(ret == ESP_OK);             // 오류 처리

     // DMA 버퍼의 데이터를 처리 (FFT, THD 계산 등)

     //=======================================================
     config1_value  &= ~(0x0800);           // BURST_EN =0;
     ESP_LOGI("spi_test",  "config1(burst_en=0:rewrite): %04X", config1_value);



     // for (int i = 2; i < FFT_SIZE+2; i++) {  // 첫 번째 데이터는 명령어가 들어가므로 제외
     /* 
      for (int i = 2; i < FFT_SIZE+2; i++) {  // 첫 번째 데이터는 명령어가 들어가므로 제외
           printf("Sample[%d]: %02X\n", i - 2, dma_buffer[i]);
      }
     */

      char *c=(char*)dma_buffer;

      for (int i = 0; i < (20*4); i +=4 ) 
    	{  // 첫 번째 데이터는 명령어가 들어가므로 제외
    	    printf("Sample[%4d]: %02x %02x %02x %02x\n", i , c[i], c[i+1], c[i+2], c[i+3]);
      }
      //free(tx_buffer);  // 메모리 해제
      return 0;
 //    spi_device_release_bus(spi_driver_ptr->handle);  // SPI 버스 해제 
}


/*
void read_samples_with_dma() {
    spi_device_acquire_bus(spi_driver_ptr->handle, portMAX_DELAY);  // SPI 버스 잠금

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    uint16_t command = (REG_AV_WAV_1 << 4) | 0x08;  // AV_WAV (0x601) 레지스터에서 읽기 명령어
    tx_buffer[0] = (command >> 8) & 0xFF;
    tx_buffer[1] = command & 0xFF;

    memset(tx_buffer + 2, 0x00, sizeof(uint8_t) * FFT_SIZE);  // 명령어 뒤에 더미 데이터를 채워넣음
    memset(dma_buffer, 0x00, sizeof(uint8_t) * FFT_SIZE);     // DMA 버퍼 초기화

    t.length = (FFT_SIZE + 2) * sizeof(uint8_t);  // TX 및 RX 길이 (16비트 명령어 + 1024 샘플)
    t.tx_buffer = tx_buffer;
    
    // 첫 4바이트는 RXDATA에 저장하고, 나머지는 DMA 버퍼에 저장
    t.flags = SPI_TRANS_USE_RXDATA;  // 첫 번째 32비트는 rx_data에 저장
    t.rxlength = FFT_SIZE * sizeof(uint8_t);  // 1024 샘플 * 8비트 (1바이트)
    t.rx_buffer = dma_buffer;  // DMA 버퍼에 저장할 위치

    esp_err_t ret = spi_device_polling_transmit(spi_driver_ptr->handle, &t);  // DMA 전송
    assert(ret == ESP_OK);  // 오류 처리

    // rx_data에 저장된 첫 번째 32비트 데이터 출력
    printf("First 32-bit data (rx_data): %02X%02X%02X%02X\n", t.rx_data[0], t.rx_data[1], t.rx_data[2], t.rx_data[3]);

    // DMA 버퍼의 나머지 데이터를 처리 (FFT, THD 계산 등)
    for (int i = 2; i < 202; i++) {
        printf("Sample[%d]: %02X\n", i - 2, dma_buffer[i]);
    }

    spi_device_release_bus(spi_driver_ptr->handle);  // SPI 버스 해제
}
*/

#endif 




bool check_spi(void)
{
    bool commscheck  = false;  
    
    SPI_Write_16(spi_driver_ptr->handle, REG_RUN, ADE9153A_RUN_ON);
    SPI_Read_16(spi_driver_ptr->handle, REG_RUN);

    vTaskDelay(pdMS_TO_TICKS(100)); 

    if(SPI_Read_32(spi_driver_ptr->handle, REG_VERSION_PRODUCT)!= 0x0009153A) return false;
    

    return true;
}





int  ade9153a_spi_init(void)
//  int  ade9153a_spi_init(int argc, char **argv)
{
	// Reset ADE9153A
	//uint32_t  VersionProduct=0;
	extern AcalRegs  acalVals;



	ESP_LOGW("ade9153a_spi_init: ", "reboot원인이 ADE9153A가 아닐수도 있어서 temperature가 범위를 벗어날 때만 HW Reset하도록 함."); 
//--------------------------------------------------
//  	ade9153a_reset();
//  	ESP_LOGW("ade9153a_spi_init: ", "reset후에 4초를 기다려봄 :temp 를 Init하는데 시간이 많이 걸리나??"); 
//  	vTaskDelay(pdMS_TO_TICKS(4000)); 
//--------------------------------------------------
	


	/*SPI initialization and test*/
	bool  commscheck  =  check_spi();  
	
	if(!commscheck) {
		vTaskDelay(pdMS_TO_TICKS(1000));
		ESP_LOGE("ade9153a_spi_init: ", "Can't  Reset"); 
	
	}
	//  SPI_Write_16(spi_driver_ptr->handle,  REG_RUN,  ADE9153A_RUN_ON);
	//  SPI_Read_16(spi_driver_ptr->handle,  REG_RUN);
	ADE9153A_Init();
	
	
	if((acalVals.AICC!=0)&&(acalVals.AVCC!=0))
	{
		ApplyAcal(-acalVals.AICC, acalVals.AVCC);
	} 

	uint32_t VersionProduct = SPI_Read_32(spi_driver_ptr->handle,  REG_VERSION_PRODUCT);  

	ESP_LOGI("ade9153a_spi_init: ", "Version: %lx", VersionProduct);
	if(VersionProduct != 0x0009153A) {
		ESP_LOGE("ade9153a_spi_init: ", "Version check failed");
		spi_driver_ptr->isUsed  = false;  
		return -1;
	}
	
	spi_driver_ptr->isUsed  = true;
	ESP_LOGI("ade9153a_spi_init: ",  "SPI device initialized successfully") ;
	return  ESP_OK;

}





void ade9153a_reset(void)
{
	// Configure the reset pin as output
	gpio_config_t io_conf = {
		.pin_bit_mask = (1ULL << PIN_NUM_RST),
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_ENABLE,
		.intr_type = GPIO_INTR_DISABLE
	};
	esp_err_t ret = gpio_config(&io_conf);
	
	// Pull the reset pin low to reset the ADE9153A
	ret = gpio_set_level(PIN_NUM_RST,  0);
	if (ret != ESP_OK) {
		ESP_LOGE("GPIO", "Failed to set reset pin low: %s", esp_err_to_name(ret));
		return;
	}
	ESP_LOGE("ade9153a_reset: ", "ade9153a_reset() : set to 0/ then delay 200msec");
	vTaskDelay(pdMS_TO_TICKS(200));      //Hold low for 100 milliseconds
	     
	// Release the reset pin
	ret = gpio_set_level(PIN_NUM_RST, 1);
	if (ret != ESP_OK) {
		ESP_LOGE("GPIO", "Failed to set reset pin high: %s", esp_err_to_name(ret));
		return;
	}
	ESP_LOGE("ade9153a_reset: ", "ade9153a_reset() : set to 1/ then delay 1sec");
	vTaskDelay(pdMS_TO_TICKS(1000)); // Wait for 100 milliseconds to allow the device to initialize         
   
}



/* 
Description: Writes 16bit data to a 16 bit register. 
Input: Register address, data
Output:-
*/
void SPI_Write_16(spi_device_handle_t spi, uint16_t Address, uint16_t Data)
{
   esp_err_t ret;
   spi_transaction_t  t;
   uint16_t temp_address;
   uint8_t tx_buf[4];

   temp_address = ((Address <<4) & 0xFFF0);  // Shift address to align with cmd packet
   memset(&t, 0, sizeof(t));                 // Zero out the transaction 

   tx_buf[0]  = temp_address >> 8;
   tx_buf[1]  = (uint8_t)temp_address;
   tx_buf[2]  = Data>> 8; 
   tx_buf[3]  = (uint8_t)Data;

#if 0
       t.length = 8 * sizeof(tx_buf);    // Length in bits
       t.tx_buffer  = tx_buf;            // Data
#else 
      tx_buf[1]  =  temp_address  >> 8;
      tx_buf[0]  =  (uint8_t)temp_address;
	    memcpy(&t.cmd, &tx_buf[0], 2);
	    t.length  =  8 *  2;    // Length in bits  
	    t.tx_buffer = &tx_buf[2];                // Data 
#endif 
 
   ret = spi_device_polling_transmit(spi, &t);  // Transmit!

  if(ret != ESP_OK)  {
          ESP_LOGE("SPI",  "SPI transmission failed");
  } else {
 //         ESP_LOGI("SPI",  "Send 16-bit data: Addr=0x%04X, Data=0x%04X",temp_address, Data);
  }
}

/* 
Description: Writes 32bit data to a 32 bit register. 
Input: Register address, data
Output:-
*/
void SPI_Write_32(spi_device_handle_t spi,  uint16_t Address,  uint32_t Data)
{
       uint16_t  temp_address;
       uint8_t   tx_buf[6];
       spi_transaction_t t;
       temp_address  = ((Address <<4) & 0xFFF0);  //shift address
       memset(&t,  0,  sizeof(t));          //zero out the transaction 

       tx_buf[0]  =  temp_address  >> 8;
       tx_buf[1]  =  (uint8_t)temp_address;
       tx_buf[2]  =  Data>>24;
       tx_buf[3]  =  (uint8_t)(Data >> 16);
       tx_buf[4]  =  (uint8_t)(Data >> 8);
       tx_buf[5]  =  (uint8_t)Data;


#if 0
       t.length = 8 * sizeof(tx_buf);    // Length in bits
       t.tx_buffer  = tx_buf;            // Data
#else 
      tx_buf[1]  =  temp_address  >> 8;
      tx_buf[0]  =  (uint8_t)temp_address;
	    memcpy(&t.cmd, &tx_buf[0], 2);
	    t.length  =  8 *  4;    // Length in bits  
	    t.tx_buffer = &tx_buf[2];                // Data 
#endif 

       esp_err_t ret  = spi_device_transmit(spi, &t);    //transmit! 

       if(ret  != ESP_OK) {
           ESP_LOGE("SPI", "SPI transmission failed");        
       }       
}

uint16_t  SPI_Read_16(spi_device_handle_t spi,  uint16_t  Address)
{
     uint16_t  temp_address;
     uint16_t  returnData;
     uint8_t   tx_buf[4]= {0,};
     uint8_t   rx_buf[4]={0,};
     spi_transaction_t  t;


     temp_address  = (((Address << 4) & 0xFFF0) + 8);  
     memset(&t,  0,  sizeof(t));   // Zero out the transaction 

     tx_buf[0]  =  temp_address >> 8;
     tx_buf[1]  =  (uint8_t)temp_address;

    // First, send  the address
    // t.length =  8 * sizeof(tx_buf);    // Length in bits
    #if 0
     t.length =  8 * 4;
     t.tx_buffer  = tx_buf;            // Data
   
     t.rxlength =   8  * 4;  // Length  in bits
     t.rx_buffer  = rx_buf;
    #else 
            tx_buf[1]  =  temp_address  >> 8;
            tx_buf[0]  =  (uint8_t)temp_address;
            memcpy(&t.cmd, &tx_buf[0], 2);
		        t.length  =  8 *  2;    // Length in bits  
		        t.tx_buffer = &tx_buf[2];                // Data 
	 	  
            t.rxlength  =  8 * 2;       //   Length  in bits
            t.rx_buffer =  &rx_buf[2];
    #endif 
    
    // t.flags = SPI_TRANS_USE_RXDATA; // Receive data directly to rx_data

     esp_err_t ret = spi_device_polling_transmit(spi,  &t);           // Transmit!
     if(ret != ESP_OK) {
         ESP_LOGE("SPI",  "SPI transmission failed");
         return 0;
     }

       returnData  =  (uint16_t)rx_buf[2]<<8 |  rx_buf[3];
       //ESP_LOGI("SPI",  "Read 16-bit data: Addr=0x%04X, Data=0x%04X",  temp_address, returnData);
     //  ESP_LOGI("SPI", "Read 16-bit data: Addr=0x%04X, Data=0x%04X", temp_address, returnData);
    return  returnData;
}

uint32_t read_ade9153a_wav(void)
{
	return SPI_Read_32(spi_driver_ptr->handle,   REG_AV_WAV);
}

uint32_t  SPI_Read_32(spi_device_handle_t spi,  uint16_t  Address)
{
      uint16_t  temp_address;
      uint32_t  returnData;
      
      uint8_t   tx_buf[6]={0x0,};
      uint8_t   rx_buf[6]={0x0,};
      spi_transaction_t  t; 
      
      temp_address  = (((Address << 4) & 0xFFF0) + 8);
      memset(&t, 0,  sizeof(t));       // Zero out the transaction 
     
      tx_buf[0]  = temp_address >> 8;
      tx_buf[1] =  (uint8_t)temp_address;
 

 #if 0        // org
       // First, send  the address
       t.length = 8 * 6;    // Length in bits
       t.tx_buffer  =  tx_buf;      // Data
       t.rxlength = 8*6; 
       t.rx_buffer  =  rx_buf;          // Expecting 32 bits of data
       //t.flags = SPI_TRANS_USE_RXDATA; // Receive data directly to rx_data
#else 
        tx_buf[1]  =  temp_address  >> 8;
        tx_buf[0]  =  (uint8_t)temp_address;

        memcpy(&t.cmd, &tx_buf[0], 2);
	     	t.length  =  8 *  4;    // Length in bits  
	    	t.tx_buffer = &tx_buf[2];                // Data 
	 	
	      // t.rxlength =   8  * 4;  // Length  in bits
	      t.rx_buffer  = &rx_buf[2]; 
#endif 

 
       esp_err_t  ret  = spi_device_polling_transmit(spi,  &t);   // Transmit!
       if(ret != ESP_OK)  {
            ESP_LOGE("SPI", "SPI transmission failed" ); 
            return 0;
       } 
       returnData  = (uint32_t)rx_buf[2]<<24|(uint32_t)rx_buf[3]<<16|(uint32_t)rx_buf[4]<<8|(uint32_t)rx_buf[5];
     //  ESP_LOGI("SPI", "Read 32-bit data: Addr=0x%04X, Data=0x%08lX", temp_address, returnData);  // Note: %08lX for uint32_t
       return  returnData;
}


void  setup_dip_swell_interrupt_mode() {
   uint16_t  config1_value  = SPI_Read_16(spi_driver_ptr->handle,  REG_CONFIG1);       //  CONFIG1 레지스터 읽기
 
   // DIP/SWELL IRQ 모드 설정 (한 번의 인터럽트만 받도록 설정)
   config1_value |= (1 << 14);  // Bit 14 설정하여 한 번의 인터럽트만 발생하도록 함

    // CONFIG1 레지스터에  값 쓰기
   SPI_Write_16(spi_driver_ptr->handle,  REG_CONFIG1,  config1_value);

}


//  Voltage  Level  값을 계산하는 함수 
uint32_t  calculate_voltage_value(uint16_t  voltage) {
   //  임계 전압을 Full scale  Voltage 로 나눈 후 24 비트  정수 값으로 전환 
   float  ratio =  voltage  / FULL_SCALE_VOLTAGE;
   uint32_t  voltage_value  = (uint32_t)(ratio  * BIT_23_SCALE);    
   return  voltage_value;
}





//  DIP 및 SWELL 이벤트 설정 함수 
void setup_dip_swell() {

    // DIP  및  SWELL  임계값  
     vdip_lvl_value  =  calculate_voltage_value(dip_voltage);

     //DIP 임계값  설정 (REG_VDIP_LVL, 0x411)
     SPI_Write_32( spi_driver_ptr->handle,  REG_DIP_LVL, vdip_lvl_value);

     //DIP  감지 주기 설정 (REG_DIP_CYC,  0x48B)
     SPI_Write_16( spi_driver_ptr->handle,  REG_DIP_CYC,  rated_freq);

     vswell_lvl_value  =  calculate_voltage_value(swell_voltage);
    
     //SWELL 임계값  설정 (REG_SWELL_LVL, 0x414)
     SPI_Write_32( spi_driver_ptr->handle,   REG_SWELL_LVL,  vswell_lvl_value);
      

     //SWELL  감지 주기 설정(REG_SWELL_CYC,  0x48C)  
     SPI_Write_16( spi_driver_ptr->handle,  REG_SWELL_CYC,  rated_freq);

     setup_dip_swell_interrupt_mode(); 
}









void ADE9153A_Init(void)
{

      SPI_Write_16( spi_driver_ptr->handle, REG_AI_PGAGAIN,   ADE9153A_AI_PGAGAIN);
     // SPI_Write_16( spi_driver_ptr->handle, REG_BI_PGAGAIN,   ADE9153A_BI_PGAGAIN);
    // ApplyVolt(vgain);
      SPI_Write_32( spi_driver_ptr->handle, REG_CONFIG0,      ADE9153A_CONFIG0); 
      SPI_Write_16( spi_driver_ptr->handle, REG_CONFIG1,      ADE9153A_CONFIG1);
      SPI_Write_16( spi_driver_ptr->handle, REG_CONFIG2,      ADE9153A_CONFIG2);
      SPI_Write_16( spi_driver_ptr->handle, REG_CONFIG3,      ADE9153A_CONFIG3);
      SPI_Write_16( spi_driver_ptr->handle, REG_ACCMODE,      ADE9153A_ACCMODE);
      SPI_Write_32( spi_driver_ptr->handle, REG_VLEVEL,       ADE9153A_VLEVEL);
      SPI_Write_16( spi_driver_ptr->handle, REG_ZX_CFG,       ADE9153A_ZX_CFG);
      SPI_Write_32( spi_driver_ptr->handle, REG_MASK,         ADE9153A_MASK);
      SPI_Write_32( spi_driver_ptr->handle, REG_ACT_NL_LVL,   ADE9153A_ACT_NL_LVL);
      SPI_Write_32( spi_driver_ptr->handle, REG_REACT_NL_LVL, ADE9153A_REACT_NL_LVL);
      SPI_Write_32( spi_driver_ptr->handle, REG_APP_NL_LVL,   ADE9153A_APP_NL_LVL);
      SPI_Write_16( spi_driver_ptr->handle, REG_COMPMODE,     ADE9153A_COMPMODE);
      SPI_Write_32( spi_driver_ptr->handle, REG_VDIV_RSMALL, ADE9153A_VDIV_RSMALL);
      SPI_Write_16( spi_driver_ptr->handle, REG_EP_CFG,      ADE9153A_EP_CFG);
      SPI_Write_16( spi_driver_ptr->handle, REG_EGY_TIME,    ADE9153A_EGY_TIME);		//Energy accumulation ON
      SPI_Write_16( spi_driver_ptr->handle, REG_TEMP_CFG,    ADE9153A_TEMP_CFG);
      setup_dip_swell(); 
}



bool  RdyEnergy(void)
{
       bool  ret = false;


      ret  =     (bool)((SPI_Read_32(spi_driver_ptr->handle, REG_STATUS)&0x100)>>9);


      return ret; 
}





void  ReadEnergyRegs(EnergyRegs *Data)
{
      int32_t  tempReg;
      int32_t  tempReg_low;
      float    tempValue;
     
       //uint16_t  reg = SPI_Read_16(spi_driver_ptr->handle, REG_EP_CFG);
       //ESP_LOGI("SPI", "REG_EP_CFG: %04x", reg); 

       //reg = SPI_Read_16(spi_driver_ptr->handle, REG_EGY_TIME);
       //ESP_LOGI("SPI", "REG_EGY_TIME: %04x", reg); 

       //#define CAL_ENERGY_CC	0.858307	// (uWhr/xTHR_HI code)Applicable for Active, reactive and apparent energy
       tempReg = (int32_t) SPI_Read_32(spi_driver_ptr->handle,  REG_AWATTHR_HI);
       tempReg_low =  SPI_Read_32(spi_driver_ptr->handle,  REG_AWATTHR_LO) & 0xFFF;
       
       Data->ActiveEnergyReg = tempReg;
     //  ESP_LOGI("SPI","ActiveEnergy(reg): %ld  %ld", Data->ActiveEnergyReg, tempReg_low);   

       //tempValue = (float)tempReg * CAL_ENERGY_CC  / 1000;
       tempValue = (float)tempReg * CAL_ENERGY_CC/1000;
       Data->ActiveEnergyValue = tempValue;				//Energy in mWhr

       tempReg = (int32_t) SPI_Read_32(spi_driver_ptr->handle, REG_AFVARHR_HI);
       Data->FundReactiveEnergyReg = tempReg;
       tempReg_low =  SPI_Read_32(spi_driver_ptr->handle,  REG_AFVARHR_LO) & 0xFFF;
   //    ESP_LOGI("SPI","ReactiveEnergy(reg): %ld  %ld", Data->FundReactiveEnergyReg, tempReg_low); 
       //tempValue = (float)tempReg * CAL_ENERGY_CC / 1000;
       tempValue = (float)tempReg * CAL_ENERGY_CC/1000;
       Data->FundReactiveEnergyValue = tempValue;			//Energy in mVARhr
  
       tempReg = (int32_t) SPI_Read_32(spi_driver_ptr->handle, REG_AVAHR_HI);
       Data->ApparentEnergyReg = tempReg;
       tempReg_low =  SPI_Read_32(spi_driver_ptr->handle,  REG_AVAHR_LO) & 0xFFF;
     //  ESP_LOGI("SPI","ApparentEnergy(reg): %ld  %ld", Data->ApparentEnergyReg, tempReg_low); 
       //tempValue = (float)tempReg * CAL_ENERGY_CC / 1000;
       tempValue = (float)tempReg * CAL_ENERGY_CC/1000;
       Data->ApparentEnergyValue = tempValue;				//Energy in mVAhr
}




void ReadPowerReg(PowerRegs *Data)
{
      int32_t  tempReg;
      float    tempValue;

     //#define CAL_POWER_CC 	1508.743	// (uW/code) Applicable for Active, reactive and apparent power
      tempReg  =  (int32_t) SPI_Read_32(spi_driver_ptr->handle, REG_AWATT);
	    Data->ActivePowerReg = tempReg;
      //tempValue = (float)tempReg * CAL_POWER_CC / 1000;
      tempValue = (float)tempReg * CAL_POWER_CC/ 1000;
      Data->ActivePowerValue = tempValue;					//Power in mW

      tempReg =  (int32_t) SPI_Read_32(spi_driver_ptr->handle, REG_AFVAR);
    	Data->FundReactivePowerReg = tempReg;
    	//tempValue = (float)tempReg * CAL_POWER_CC / 1000;
      tempValue = (float)tempReg * CAL_POWER_CC / 1000;
	    Data->FundReactivePowerValue = tempValue;			//Power in mVAR

      tempReg = (int32_t) SPI_Read_32(spi_driver_ptr->handle, REG_AVA); 
      Data->ApparentPowerReg = tempReg;
	    //tempValue = (float)tempReg * CAL_POWER_CC / 1000;
      tempValue = (float)tempReg * CAL_POWER_CC / 1000;
	    Data->ApparentPowerValue = tempValue;				//Power in mVA
}





//   실제 전압 계산 함수 
float  calculate_rms_voltage(uint32_t  value,  uint32_t  vlevel,    float full_scale_voltage)
{
         float  ratio = (float)value/(float)vlevel;
         float  rms_voltage  =  full_scale_voltage  * sqrt(ratio);
         return  rms_voltage;
}




 void ReadRMSRegs(RMSRegs *Data)
 {
        uint32_t  tempReg;
        float     tempValue;
        //#define CAL_IRMS_CC		      0.2282163	// (uA/code)
        tempReg = (int32_t) SPI_Read_32(spi_driver_ptr->handle, REG_AIRMS);
      //  tempReg = (int32_t) SPI_Read_32(spi_driver_ptr->handle, REG_BIRMS);
        Data->CurrentRMSReg = tempReg;
       // tempValue = (float)tempReg * CAL_IRMS_CC / 1000;    //RMS in mA
        tempValue = (float)tempReg *  CAL_IRMS_CC/ 1000;    //RMS in mA

        Data->CurrentRMSValue = tempValue;
       
        
        tempReg = (int32_t) SPI_Read_32(spi_driver_ptr->handle, REG_AVRMS);
        Data->VoltageRMSReg = tempReg;
       // tempValue = (float)tempReg * CAL_VRMS_CC / 1000;    //RMS in mV
        tempValue = (float)tempReg * CAL_VRMS_CC/1000;    //RMS in mV
        Data->VoltageRMSValue = tempValue;
        tempReg = SPI_Read_32(spi_driver_ptr->handle,  REG_DIPA); 
     
        if(tempReg !=0)
        {
             tempValue   =  calculate_rms_voltage(tempReg,  ADE9153A_VLEVEL,  FULL_SCALE_VOLTAGE); 
             Data->CurRMSVoltage  = tempValue; 
             ESP_LOGI("SPI", "DIPA: %lu,  %f V", tempReg,  Data->CurRMSVoltage);
        }  

        tempReg = SPI_Read_32(spi_driver_ptr->handle,  REG_SWELLA);  
        if(tempReg !=0)
        {
            tempValue  =  calculate_rms_voltage(tempReg,   ADE9153A_VLEVEL,  FULL_SCALE_VOLTAGE);
            Data->CurRMSVoltage = tempValue;
            ESP_LOGI("SPI", "SWELLA: %lu, %f V",  tempReg, Data->CurRMSVoltage);
        }
 }




void ReadHalfRMSRegs(HalfRMSRegs *Data)
{
   uint32_t tempReg;
   float tempValue;

 //#define CAL_IRMS_CC		0.838190	// (uA/code)     
   tempReg = (int32_t)SPI_Read_32(spi_driver_ptr->handle, REG_AIRMS_OC);
 //  tempReg = (int32_t)SPI_Read_32(spi_driver_ptr->handle, REG_BIRMS_OC);
   Data->HalfCurrentRMSReg = tempReg;
   tempValue = (float)tempReg * CAL_IRMS_CC / 1000;	//Half-RMS in mA
   Data->HalfCurrentRMSValue = tempValue;

   tempReg = (int32_t) SPI_Read_32(spi_driver_ptr->handle, REG_AVRMS_OC);
   Data->HalfVoltageRMSReg = tempReg;
   tempValue = (float)tempReg * CAL_VRMS_CC / 1000;	//Half-RMS in mV
   Data->HalfVoltageRMSValue = tempValue; 
}




void ReadPQRegs(PQRegs *Data)
{
      int32_t tempReg;
	   uint16_t temp;
    	float mulConstant;
	   float tempValue;
     
     tempReg = (int32_t)SPI_Read_32(spi_driver_ptr->handle,REG_APF); //Read PF register
     Data->PowerFactorReg = tempReg;
	   tempValue = (float)tempReg / (float)134217728; //Calculate PF
     Data->PowerFactorValue = tempValue;

     tempReg = (int32_t) SPI_Read_32(spi_driver_ptr->handle,REG_APERIOD); //Read PERIOD register
	   Data->PeriodReg = tempReg;
	   tempValue = (float)(4000 * 65536) / (float)(tempReg + 1); //Calculate Frequency
	   Data->FrequencyValue = tempValue;

     temp = SPI_Read_16(spi_driver_ptr->handle, REG_ACCMODE); //Read frequency setting register
	   if((temp & 0x0010) > 0){
           		mulConstant = 0.02109375;  //multiplier constant for 60Hz system
     }else{
	         	mulConstant = 0.017578125; //multiplier constant for 50Hz system		
	   }

     //tempReg = (int16_t) SPI_Read_16(spi_driver_ptr->handle, REG_ANGL_AV_AI); //Read ANGLE register
	   tempReg = (int16_t) SPI_Read_16(spi_driver_ptr->handle, REG_ANGL_AV_AI); //Read ANGLE register
     Data->AngleReg_AV_AI   = tempReg;
	   tempValue  =  tempReg * mulConstant;    //Calculate Angle in degrees
	   Data->AngleValue_AV_AI = tempValue;  

      //ESP_LOGI("SPI","V I angle: %ld, %f degree", Data->AngleReg_AV_AI , Data->AngleValue_AV_AI);

}



void ReadAcalRegs(AcalRegs *Data)
{
     uint32_t  tempReg;
     float  tempValue;

     tempReg = (int32_t)SPI_Read_32(spi_driver_ptr->handle, REG_MS_ACAL_AICC); //Read AICC register
     //tempReg = (int32_t)SPI_Read_32(spi_driver_ptr->handle, REG_MS_ACAL_BICC); //Read BICC register
     Data->AcalAICCReg = tempReg;
     tempValue = (float)tempReg / (float)2048; //Calculate Conversion Constant (CC)
     Data->AICC = tempValue;
     tempReg = (int32_t)SPI_Read_32(spi_driver_ptr->handle, REG_MS_ACAL_AICERT); //Read AICERT register
     //tempReg = (int32_t)SPI_Read_32(spi_driver_ptr->handle, REG_MS_ACAL_BICERT); //Read AICERT register
     Data->AcalAICERTReg = tempReg;

     tempReg = (int32_t)SPI_Read_32(spi_driver_ptr->handle, REG_MS_ACAL_AVCC); //Read AVCC register
     Data->AcalAVCCReg = tempReg;
     tempValue = (float)tempReg/(float)2048; //Calculate Conversion Constant (CC)
     Data->AVCC = tempValue;
     tempReg = (int32_t)SPI_Read_32(spi_driver_ptr->handle, REG_MS_ACAL_AVCERT); //Read AICERT register
     Data->AcalAVCERTReg = tempReg;
}

/* 
Description: Start autocalibration on the respective channel
Input: -
Output: Did it start correctly?
*/
bool  StartAcal_AINormal(void)
{
   uint32_t ready = 0;
   int waitTime = 0;
   //org location:  ready = SPI_Read_32(REG_MS_STATUS_CURRENT);				//Read system ready bit
   while((ready  & 0x00000001) == 0)
   {
       ready  =  SPI_Read_32(spi_driver_ptr->handle, REG_MS_STATUS_CURRENT);	//shcho add
       if(waitTime >11)
       {
              ESP_LOGE("SPI", "Exit: StartAcal_AINormal");

             
             return false; 
       }       
    
       vTaskDelay(pdMS_TO_TICKS(100));      //Hold low for 100 milliseconds
       //vTaskDelay(pdMS_TO_TICKS(1000));      //Hold low for 1000 milliseconds 
       ESP_LOGI("SPI",  "Acal_AI: %d", waitTime) ;
       
       waitTime++;
   }
   SPI_Write_32(spi_driver_ptr->handle, REG_MS_ACAL_CFG, 0x00000013);
   return true;
}


bool  StartAcal_AITurbo(void)
{   
    uint32_t  ready  = 0;
    int waitTime =0;
    while((ready & 0x00000001) == 0)
    {
          ready  = SPI_Read_32(spi_driver_ptr->handle, REG_MS_STATUS_CURRENT);    //Read system ready
     
           if(waitTime >15)
	        {
               return false;
           }		   
           //msleep(100);
           //usleep(100*1000);

           vTaskDelay(pdMS_TO_TICKS(100));      //Hold low for 100 milliseconds 
	        waitTime++;
    }
    SPI_Write_32(spi_driver_ptr->handle, REG_MS_ACAL_CFG, 0x00000017);
    return true;
}


bool  StartAcal_AV(void)
{
       uint32_t  ready=0;
       int       waitTime=0;

       while((ready  & 0x00000001)  == 0)
       {

            ready  =  SPI_Read_32(spi_driver_ptr->handle, REG_MS_STATUS_CURRENT);     //Read system ready bit
            if( waitTime > 15)
            {
                return false;
            }
            //msleep(100);
            vTaskDelay(pdMS_TO_TICKS(100));      //Hold low for 100 milliseconds 
             waitTime++; 
       }
       return true;
}



bool  detect_arc_by_rms_change()  {
          static uint32_t prev_airms_1;
          bool  arc_detect = false; 
            //SPI_Read_32(spi_driver_ptr->handle, REG_AIRMS_1)
         uint32_t curr_airms_1 = SPI_Read_32(spi_driver_ptr->handle, REG_AIRMS_1);  // AIRMS_1
         ESP_LOGI("ARC", "prev_current:%ld, current:%ld",  prev_airms_1, curr_airms_1);
         if (prev_airms_1 != 0) {
              int32_t difference = fabs(curr_airms_1 - prev_airms_1);
              float percent_change = (float)(difference)/(float)(prev_airms_1) * 100;
              if (percent_change > 20.0) {
              ESP_LOGI( "ARC", "Arc detected based on RMS current change! difference:%ld Change: %.2f%%", difference, percent_change);
              arc_detect = true;
           }
         }
      prev_airms_1  = curr_airms_1;
      return   arc_detect;
}









void StopAcal(void)
{
      
           SPI_Write_32(spi_driver_ptr->handle, REG_MS_ACAL_CFG, 0x00000000);
}



void  ApplyAcal(float AICC, float AVCC)
{
      int32_t AIGAIN;
    	int32_t AVGAIN;
	
    	AIGAIN = (AICC / (CAL_IRMS_CC * 1000) - 1) * 134217728;
   	  AVGAIN = (AVCC / (CAL_VRMS_CC * 1000) - 1) * 134217728;
	
	    SPI_Write_32(spi_driver_ptr->handle, REG_AIGAIN, AIGAIN);
      //SPI_Write_32(spi_driver_ptr->handle, REG_BIGAIN, AIGAIN);
   	  SPI_Write_32(spi_driver_ptr->handle, REG_AVGAIN, AVGAIN);
}




void ApplyVolt(float AVCC)
{
     int32_t AVGAIN;
     AVGAIN = (AVCC / (CAL_VRMS_CC * 1000) - 1) * 134217728;
             // 134217728
    // AVGAIN  =   42000000; 
     printf("AVGAIN: %ld\n", AVGAIN);
     SPI_Write_32(spi_driver_ptr->handle, REG_AVGAIN, AVGAIN);
}









/* 
Description: Starts a new acquisition cycle. Waits for constant time and returns register value and temperature in Degree Celsius
Input:	Structure name
Output: Register reading and temperature value in Degree Celsius
*/
void ReadTemperature(Temperature * Data)
{
    uint32_t trim;
	  uint16_t gain;
    uint16_t offset;
   	uint16_t tempReg; 
   	float tempValue;

     SPI_Write_16(spi_driver_ptr->handle, REG_TEMP_CFG, ADE9153A_TEMP_CFG);//Start temperature acquisition cycle
    //msleep(10);   //delay of 2ms. Increase delay if TEMP_TIME is changed
     vTaskDelay(pdMS_TO_TICKS(10));      //Hold low for 10 milliseconds
    
     trim = SPI_Read_32(spi_driver_ptr->handle,  REG_TEMP_TRIM);
	   gain = (trim & 0xFFFF);  //Extract 16 LSB
	   offset = ((trim >> 16) & 0xFFFF); //Extract 16 MSB
	   tempReg = SPI_Read_16(spi_driver_ptr->handle, REG_TEMP_RSLT);	//Read Temperature result register
	   tempValue = ((float)offset/32.00) - ((float)tempReg * (float)gain/(float)131072); 

     Data->TemperatureReg = tempReg;
	   Data->TemperatureVal = tempValue;
}



void ReadTemperature2(Temperature * Data)
{
	uint32_t trim;
	uint16_t gain;
	uint16_t offset;
	uint16_t tempReg; 
	float tempValue;
	
	SPI_Write_16(spi_driver_ptr->handle, REG_TEMP_CFG, ADE9153A_TEMP_CFG);//Start temperature acquisition cycle
	//msleep(10);   //delay of 2ms. Increase delay if TEMP_TIME is changed
	//vTaskDelay(pdMS_TO_TICKS(10));      //Hold low for 10 milliseconds
	ESP_LOGW("check TEMP_CFG", "REG_TEMP_CFG=%04x(w:%04X)", SPI_Read_16(spi_driver_ptr->handle, REG_TEMP_CFG), ADE9153A_TEMP_CFG);
  

	vTaskDelay(pdMS_TO_TICKS(1000));           //Hold low for 1000 milliseconds   // 시간이 중요하지 않아서  

	trim = SPI_Read_32(spi_driver_ptr->handle,  REG_TEMP_TRIM);
	gain = (trim & 0xFFFF);  //Extract 16 LSB
	offset = ((trim >> 16) & 0xFFFF); //Extract 16 MSB

	// printf("trim:%08lx gain:%04x offset:%04x ",trim, gain, offset); 
	ESP_LOGW("Temp2","trim:%08lx gain:%04x offset:%04x\n ",trim, gain, offset); 

	// shcho : wait for TEMP_RDY ( status.12[0x402 ) 
	uint32_t status = 0;
	uint32_t count = 0;
	int flag_break = 0;
     
	while(1)
	{
		status =  SPI_Read_32(spi_driver_ptr->handle,  REG_STATUS);
		ESP_LOGW("Temp2", "status:%08lx(count=%ld)", status, count); 
		if((status  &(1<<12))!=0)
		{
			break; 
		}

		if( count == 10 )
		{
			// trigraph error ??? Why ?
//  			/ijoon_s1/Project/ESP32/example/Mesh_example_from_idf_v5.2.1/mesh_border_router_w_ethernet_bridge/joajoa/ip_internal_network_w_https_ota_v3_with_console_add__adc_json_for_s3_2024_1016/main/ade9153a_spi.c:1495:75: error: trigraph ??) ignored, use -trigraphs to enable [-Werror=trigraphs]

//  			ESP_LOGE("Temp2","ADE9153A malfunction(Relay Spark??) : wait status register set (temp_done(12) ) set to : -128.0");
//  			ESP_LOGW("Temp2","ade9153a read status reg : malfunction(Relay Spark??) : TempValue set to : -128.0");
			flag_break = 1 ;
			break;
		}
		count++; 
		vTaskDelay(pdMS_TO_TICKS(1000));            // Hold low for 10  milliseconds
	}
     
	if( flag_break == 1 ) 
	{
		ESP_LOGE("Temp2", " Temperature done status : check timeout : set to -1298.0도");
		Data->TemperatureVal = -128.0;
	}
	else
	{
		tempReg = SPI_Read_16(spi_driver_ptr->handle, REG_TEMP_RSLT);	//Read Temperature result register
		//printf("tempReg:%04x \n", tempReg);
		tempValue = ((float)offset/32.00) - ((float)tempReg * (float)gain/(float)131072); 
		ESP_LOGW("Temp2",  "tempReg:%04x/ %4.2f", tempReg,  tempValue); 
	    
		Data->TemperatureReg = tempReg;
		Data->TemperatureVal = tempValue;
	}
}



uint32_t  ProductVersion(void)
{
     return SPI_Read_32(spi_driver_ptr->handle,  REG_VERSION_PRODUCT);
}


void AIGain_Init(void)
{
   // SPI_Write_32(spi_driver_ptr->handle, REG_AIGAIN,  -268435456);
    SPI_Write_32(spi_driver_ptr->handle, REG_BIGAIN,  -268435456); 
    vTaskDelay(pdMS_TO_TICKS(500));      //Hold low for 500 milliseconds
}





//DIP/SWELL 이벤트 확인 함수 구현

void  check_dip_swell_event()  {
      


    extern  RMSRegs  rmsVals;

    //   EVENT_STATUS 레지스터에서 DIP/SWELL 상태 읽기 (REG_EVENT_STATUS, 0x4C1)
    uint16_t event_status = SPI_Read_16(spi_driver_ptr->handle,  REG_EVENT_STATUS);


     if(event_status & (1<<DIP_ENABLE_BIT))  {

          ESP_LOGI("DIP Event",  "DIP event detected!");
          // DIPA 레지스터에서 최소 RMS(REG_DIPA)값 읽기  (DIP 시 최소 전압))
          uint16_t dip_rms_value = SPI_Read_16(spi_driver_ptr->handle,  REG_DIPA);
       // ESP_LOGI("DIP Event", "Minimum RMS during DIP: %d",  dip_rms_value);
      //    rmsVals.CurDipVoltage  
          rmsVals.CurRMSVoltage= calculate_rms_voltage(dip_rms_value, ADE9153A_VLEVEL,  FULL_SCALE_VOLTAGE);
          ESP_LOGI("DIP Event", "Minimum RMS during DIP: %f(V)", rmsVals.CurRMSVoltage);
          sprintf(dip_status, "1");
          dip_flag  =  true;
     }


    if(event_status & (1<<SWELLA_BIT))  {
         ESP_LOGI("SWELL Event",  "SWELL event detected!");
          // SWELLA 레지스터에서 최대 RMS 값 읽기 (SWELL 시 최대 전압)
         uint16_t swell_rms_value = SPI_Read_16(spi_driver_ptr->handle,  REG_SWELLA);
         rmsVals.CurRMSVoltage= calculate_rms_voltage(swell_rms_value, ADE9153A_VLEVEL,  FULL_SCALE_VOLTAGE);
         ESP_LOGI("SWELL Event", "Maximum RMS during SWELL: %f", rmsVals.CurRMSVoltage);
         sprintf(swell_status, "1"); 
         swell_flag  = true;
     }  
}







































