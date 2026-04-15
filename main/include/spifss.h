/*   2022 Espressif Systems (Shanghai) CO LTD
 
*/
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int id;
    float wh;
    float varh;
    float vah;
    char datetime[20];  // Adjust the size as needed for the datetime string
} meter;

void   init_db(void);
int    save_db(void);
int    delete_old_records(const char* cutoff_datetime);
int    select_meter_1(void);
size_t get_database_size(const char *db_path);
int    get_nearest_data(const char *datetime, int  duration);
int    delete_records(void);

/** SPIFFS SQLite DB 경로 — meter_app / spifss 공통 */
extern const char db_path[];

#ifdef __cplusplus
}
#endif
