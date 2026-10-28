#pragma once
#include <cstdint>
constexpr uint8_t ESP_IMAGE_HEADER_MAGIC=0xe9;
constexpr uint16_t ESP_CHIP_ID_ESP32S3=9;
struct __attribute__((packed)) esp_image_header_t {
  uint8_t magic,segments,mode,speed;
  uint32_t entry;
  uint8_t wp,drive[3];
  uint16_t chip_id;
  uint8_t reserved[10];
};
static_assert(sizeof(esp_image_header_t)==24);
