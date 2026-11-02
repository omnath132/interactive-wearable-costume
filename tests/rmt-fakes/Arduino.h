#pragma once
#include <stdint.h>
#include <stddef.h>
using esp_err_t=int;
using gpio_num_t=int;
constexpr int ESP_OK=0,ESP_ERR_TIMEOUT=1,ESP_ERR_NO_MEM=2;
#define CONFIG_IDF_TARGET_ESP32 1
#define ARDUINO_RUNNING_CORE 1
