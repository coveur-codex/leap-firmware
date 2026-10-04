#pragma once
#include <cstddef>
#include <cstring>
constexpr int ESP_PARTITION_TYPE_DATA=1, ESP_PARTITION_SUBTYPE_DATA_SPIFFS=2, ESP_OK=0;
struct esp_partition_t { size_t size=8*1024*1024; };
inline const esp_partition_t *esp_partition_find_first(int,int,const char*) {
 static esp_partition_t partition; return &partition;
}
inline int esp_partition_read(const esp_partition_t*,size_t,void *p,size_t n) {
 memset(p,0xff,n); return ESP_OK;
}
