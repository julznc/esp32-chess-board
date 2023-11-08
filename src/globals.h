
#pragma once

#include <string.h>
#include <time.h>

#include <esp_log.h>
#include <esp_mac.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <nvs_flash.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>

#include "hal_gpio.h"
#include "hal_i2c.h"


#define K_APP_NAME          "Electronic Chess Board"
#define K_APP_VERSION       "00.03.04"    // <major>.<minor>.<test>

// at 1ms tick
#define delayms(ms)         vTaskDelay(ms)
#define millis()            xTaskGetTickCount()

#ifndef __FILENAME__
# define __FILENAME__       (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define PRINTF              printf

#if 1

#define __FILELINE__        (__builtin_strchr(__FILENAME__, '.') - __FILENAME__), __FILENAME__, __LINE__

#define LOGD(fmt, ...)      PRINTF("D (%lu) %.*s[%03d] " fmt "\r\n", esp_log_timestamp(), __FILELINE__, ## __VA_ARGS__)
#define LOGI(fmt, ...)      PRINTF("I (%lu) %.*s[%03d] \x1B[32m" fmt "\x1B[0m\r\n", esp_log_timestamp(), __FILELINE__, ## __VA_ARGS__)
#define LOGW(fmt, ...)      PRINTF("W (%lu) %.*s[%03d] \x1B[33m" fmt "\x1B[0m\r\n", esp_log_timestamp(), __FILELINE__, ## __VA_ARGS__)
#define LOGE(fmt, ...)      PRINTF("E (%lu) %.*s[%03d] \x1B[31m" fmt "\x1B[0m\r\n", esp_log_timestamp(), __FILELINE__, ## __VA_ARGS__)

#else

#define LOGD(...)           ESP_LOGD("app", ## __VA_ARGS__)
#define LOGI(...)           ESP_LOGI("app", ## __VA_ARGS__)
#define LOGW(...)           ESP_LOGW("app", ## __VA_ARGS__)
#define LOGE(...)           ESP_LOGE("app", ## __VA_ARGS__)

#endif


void global_init(void);
