/*
    This example creates two databases on SPIFFS,
    inserts and retrieves data from them.
*/
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "sqlite3.h"
#include "spifss.h"

const char db_path[] = "/spiffs/power_data.db";

#define MAX_SQL_QUERY_LENGTH 256
#define MAX_SQL_RESULT_LENGTH 512

static const char *TAG = "sqlite3_spiffs";

const char* data = "Callback function called";
char result[MAX_SQL_RESULT_LENGTH] ={0x0,};






static int id; 

static int rdIdx;

meter  meter_result[10];
int   count_result;
sqlite3 *db;


meter write_meter;
meter last_meter; 


// Callback for SQLite execution
static int callback(void * result, int argc, char **argv, char **azColName) 
{
    char *response = (char *)result;
    int len = strlen(response);  // Start appending at the end of the current response
    for(int i = 0; i < argc; i++) {
        len += snprintf(response + len, MAX_SQL_RESULT_LENGTH - len, "%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
       // printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
     }
       // Add a separator line between rows
    len += snprintf(response + len, MAX_SQL_RESULT_LENGTH - len, "\n");
    return 0;
}

static int callback_meter(void * result,  int argc,  char ** argv,  char **azColName) 
{
              meter * meters = (meter *)result;   // Cast data to meter array
              meters[rdIdx].id   =  atoi(argv[0]);
              meters[rdIdx].wh   =  atof(argv[1]);
              meters[rdIdx].varh =  atof(argv[2]);
              meters[rdIdx].vah  =  atof(argv[3]);
           
              strncpy(meters[rdIdx].datetime,  argv[4],  sizeof(meters[rdIdx].datetime) -1 );
              meters[rdIdx].datetime[sizeof(meters[rdIdx].datetime) - 1] = '\0';  // Ensure null termination
              rdIdx++;            // Move to the next meter structure
              ESP_LOGI(TAG,  "meter_idx : %d",  rdIdx);
              return 0;
}



// Callback for SQLite execution 
static int callback_sum(void  * result,  int argc,  char ** argv,  char **azColName) 
{
    int  * response =  (int *)result; 
     
           ESP_LOGI(TAG,  "argc : %d ",   argc);
          *response =   atoi(argv[0]);
          ESP_LOGI(TAG,  "response : %d  %s",  *response, argv[0]);
     return 0;
}


int db_open(const char *filename, sqlite3 **db) {
    int rc = sqlite3_open(filename, db);
    if (rc) {
        printf("Can't open database: %s\n", sqlite3_errmsg(*db));
        return rc;
    } else {
        printf("Opened database successfully\n");
    }
    return rc;
}


char *zErrMsg = 0;
int db_exec(sqlite3 *db,  int cmd, const char *sql) {
   // printf("%s\n", sql);
    int64_t start = esp_timer_get_time();

    int rc=0;

    switch(cmd)
    { 
     case 0:
              rc = sqlite3_exec(db, sql, callback, (void*)result, &zErrMsg);
     break;
     case 1: 
              rc = sqlite3_exec(db, sql, callback_sum, (void*)&count_result, &zErrMsg);
     break;
     case 2:
              rdIdx=0;  
              rc = sqlite3_exec(db, sql, callback_meter, (void*)meter_result, &zErrMsg);  
     break; 
    }  
    
    if(rc != SQLITE_OK) {
        printf("SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    } else {
        printf("Operation done successfully\n");
    }
    printf("Time taken: %lld\n", esp_timer_get_time() - start);
    
    return rc;
}





void   init_db(void) 
{

    int rc =0;
    ESP_LOGI(TAG, "Initializing SPIFFS");
   
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

// Initialize and mount SPIFFS filesystem
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }
     
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
   
    sqlite3_initialize();

    if(db_open("/spiffs/power_data.db", &db)) {
        sqlite3_close(db);
        esp_vfs_spiffs_unregister(NULL);
        return;
    }
    const char sql_create_table[] =  "CREATE TABLE  IF NOT EXISTS PowerData (id INTEGER PRIMARY KEY, wh REAL, varh REAL, vah REAL, datetime TEXT);";

    rc  =  db_exec(db,  0,  sql_create_table);
    if(rc != SQLITE_OK) {
            sqlite3_close(db);
            esp_vfs_spiffs_unregister(NULL);
            return;
    }


    write_meter.id   =0;
    write_meter.wh   =0;
    write_meter.varh =0;
    write_meter.vah  =0;
    strcpy(write_meter.datetime,  "");  
   // snprintf(write_meter.datetime, sizeof(write_meter.datetime), null );


   if(rc == SQLITE_OK)
   {
        const char sql_count[] = "SELECT COUNT(*) FROM PowerData";
        rc =  db_exec(db, 1  , sql_count);
        if(rc  != SQLITE_OK) {
             sqlite3_close(db);
             esp_vfs_spiffs_unregister(NULL);
             return;
        }
        else 
        {
            ESP_LOGI(TAG,   "count: %d",  count_result);
            if(count_result>0)
            {
                const  char sql_select[] = "SELECT * FROM  PowerData  ORDER BY id DESC LIMIT 1;"; 
            //    rdIdx=0;
                rc =  db_exec(db,  2, sql_select);
                if(rc != SQLITE_OK) {
                      sqlite3_close(db);
                    esp_vfs_spiffs_unregister(NULL);
                    return;
                 }
                else 
                 {

                         last_meter.id    =  meter_result[0].id;
                         last_meter.wh    =  meter_result[0].wh;
                         last_meter.varh  =  meter_result[0].varh;
                         last_meter.vah   =  meter_result[0].vah;
                         strcpy(last_meter.datetime,   meter_result[0].datetime);                         


#if 0
                          write_meter.id   =  meter_result[0].id;
                          write_meter.wh   =  meter_result[0].wh;
                          write_meter.varh =  meter_result[0].varh;
                          write_meter.vah  =  meter_result[0].vah;
                          strcpy(write_meter.datetime, meter_result[0].datetime);
#endif 




                          id    = meter_result[0].id;
                          id++;
                 }
            } 
        }
    }
    
}




int   select_meter_1(void)
{          

             int rc;
             const  char sql_select[] = "SELECT * FROM  PowerData ORDER BY id DESC LIMIT 1;"; 
          //   rdIdx=0;
             rc =  db_exec(db,  2, sql_select);
             if(rc != SQLITE_OK) {
                      sqlite3_close(db);
                      esp_vfs_spiffs_unregister(NULL);
                      return -1;
              }
              else 
              {
                     //     write_meter.id   =  meter_result[0].id;
                     //     write_meter.wh   =  meter_result[0].wh;
                     //     write_meter.varh =  meter_result[0].varh;
                     //     write_meter.vah  =  meter_result[0].vah;
                     //     strcpy(write_meter.datetime, meter_result[0].datetime);
                     ESP_LOGI(TAG,  "id:%d  wh:%.1f  varh:%.1f  vah:%.1f  time:%s",   meter_result[0].id, meter_result[0].wh,  meter_result[0].varh, meter_result[0].vah,  meter_result[0].datetime);
              }
             return 0;

}


int  save_db(void)
{
      int  rc= SQLITE_OK; 
      char  sql_query[150];
      snprintf(sql_query,  sizeof(sql_query), "INSERT INTO PowerData(id, wh, varh,vah,datetime) VALUES(%d,%.1f,%.1f,%.1f,'%s');",  id,  last_meter.wh, last_meter.varh, last_meter.vah,  last_meter.datetime);    
      rc = db_exec(db,  0 , sql_query);
      if(rc != SQLITE_OK)  {
           sqlite3_close(db);
           esp_vfs_spiffs_unregister(NULL);
           return  rc;
      }
      else 
      {
           id++;
      }
      select_meter_1();
      return rc;
}

 



int delete_old_records(const char* cutoff_datetime)
{
    int  rc = SQLITE_OK;
    char  sql_query[100];
    snprintf(sql_query,  sizeof(sql_query),  "DELETE FROM PowerData WHERE datetime <'%s';", cutoff_datetime); 

    rc = db_exec(db,  0 , sql_query);
    if (rc != SQLITE_OK ) {
       sqlite3_close(db);
       esp_vfs_spiffs_unregister(NULL);
    } else {
        printf("Records older than %s deleted successfully.\n", cutoff_datetime);
    }
    return rc;
}



int  delete_records(void)
{
    int rc = SQLITE_OK; 
    char  sql_query[100];
    snprintf(sql_query,  sizeof(sql_query),  "DELETE FROM PowerData;");

    rc = db_exec(db,  0 , sql_query);
    if (rc != SQLITE_OK ) {
       sqlite3_close(db);
       esp_vfs_spiffs_unregister(NULL);
    } else {
        printf("Delete successfully.\n");
    }
    return rc;

}





size_t get_database_size(const char *db_path)
{
    struct stat st;

    if (stat(db_path, &st) == 0) {
        return st.st_size;  // 파일 크기 (바이트 단위)
    } else {
      //  printf("Unable to get the database file size\n");
        ESP_LOGE(TAG, "Unable to get the database file size");
        return 0;  // 오류 발생 시 0 반환
    }
}




int calculate_datetime_range(const char* datetime,  int duration, char *start_time, char *end_time, size_t size)
{
       struct tm tm;
       time_t time_epoch;
       

      // 입력된 datetime을 struct tm으로 변환
      if (strptime(datetime, "%Y-%m-%d %H:%M:%S", &tm) == NULL) {
                ESP_LOGI(TAG, "Unable to get the database file size");
              return 0;
      } 

       // time_t로 변환
       time_epoch = mktime(&tm);

       // 시작 시간 계산 (duration/2 분 전)
       time_t start_epoch = time_epoch - duration * 30;
       struct tm *start_tm = localtime(&start_epoch);
       strftime(start_time, size, "%Y-%m-%d %H:%M:%S", start_tm);

       // 종료 시간 계산 (duration/2 분 전)
       time_t end_epoch = time_epoch + duration * 30;
       struct tm *end_tm = localtime(&end_epoch);
       strftime(end_time, size, "%Y-%m-%d %H:%M:%S", end_tm);

    return 1; 
}


int get_nearest_data(const char *datetime, int  duration)
{
    int   rc = SQLITE_OK;
    char  sql_select[150];
    char  start_time[20],  end_time[20]; 

    // datetime 범위 계산 

    calculate_datetime_range(datetime, duration,  start_time, end_time, sizeof(start_time));
       
   
   // SQL 퀴리 생성 
    snprintf(sql_select, sizeof(sql_select),  
                  "SELECT * FROM PowerData "     
                  "WHERE datetime BETWEEN '%s' AND '%s' "
                  "LIMIT 1",  start_time, end_time);

    //ESP_LOGI(TAG, "%s", sql_select);


    rc =  db_exec(db,  2, sql_select); 
    if(rc != SQLITE_OK) {
                      sqlite3_close(db);
                      esp_vfs_spiffs_unregister(NULL);
                      return 0;
    }
    else 
    {
                     if(rdIdx!=0) 
                     { 
                              write_meter.id   =   meter_result[0].id;
                              write_meter.wh   =   meter_result[0].wh;
                              write_meter.varh =   meter_result[0].varh;
                              write_meter.vah  =   meter_result[0].vah;
                              strcpy(write_meter.datetime,  meter_result[0].datetime);
                              ESP_LOGI(TAG,  "id:%d  wh:%.1f  varh:%.1f  vah:%.1f  time:%s",   write_meter.id, write_meter.wh,  write_meter.varh, write_meter.vah,  write_meter.datetime);
                     }
                     else
                     {
                             write_meter.id   =  0;
                             write_meter.wh   =  0;
                             write_meter.varh =  0;
                             write_meter.vah  =  0; 
                             strcpy(write_meter.datetime,  "Null");
                     } 
     }
     return 1; 
}



