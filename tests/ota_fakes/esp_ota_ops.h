#pragma once
#include <cstdint>
#include <vector>
constexpr int ESP_OK=0;
using esp_ota_handle_t=int;
enum esp_ota_img_states_t {ESP_OTA_IMG_VALID,ESP_OTA_IMG_PENDING_VERIFY};
struct esp_partition_t {size_t size; const char *label;};
inline esp_partition_t otaRunning{4*1024*1024,"ota_0"},otaTarget{4*1024*1024,"ota_1"};
inline esp_ota_img_states_t otaState=ESP_OTA_IMG_VALID;
inline int otaAborted=0,otaEnded=0,otaSelected=0,otaConfirmed=0,otaReverted=0;
inline std::vector<uint8_t> otaWritten;
inline const esp_partition_t *esp_ota_get_running_partition(){return &otaRunning;}
inline const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t*){return &otaTarget;}
inline int esp_ota_get_state_partition(const esp_partition_t*,esp_ota_img_states_t *out){*out=otaState;return ESP_OK;}
inline int esp_ota_mark_app_valid_cancel_rollback(){++otaConfirmed;return ESP_OK;}
inline int esp_ota_mark_app_invalid_rollback_and_reboot(){++otaReverted;return ESP_OK;}
inline int esp_ota_begin(const esp_partition_t*,size_t,esp_ota_handle_t *out){*out=1;otaWritten.clear();return ESP_OK;}
inline int esp_ota_write(esp_ota_handle_t,const uint8_t *p,size_t n){otaWritten.insert(otaWritten.end(),p,p+n);return ESP_OK;}
inline int esp_ota_abort(esp_ota_handle_t){++otaAborted;return ESP_OK;}
inline int esp_ota_end(esp_ota_handle_t){++otaEnded;return ESP_OK;}
inline int esp_ota_set_boot_partition(const esp_partition_t*){++otaSelected;return ESP_OK;}
