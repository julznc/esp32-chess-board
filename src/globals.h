
#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include <Arduino.h>
#include <esp_task_wdt.h>


// app firmware info
#define K_APP_NAME                  "Electronic Chess Board"
#define K_FW_VER_MAJOR              (0)
#define K_FW_VER_MINOR              (1)
#define K_FW_VER_TEST               (3)


// for esp32-s2: RX1=18, TX1=17
#define HWSERIAL                    Serial1
#define PRINTF                      HWSERIAL.printf

// pinouts
#define I2C_SDA_PIN                 (9)
#define I2C_SCL_PIN                 (10)
#define LED_STRIP_PIN               (21)


// set "CORE_DEBUG_LEVEL" build flag, and call "<hwserial>.setDebugOutput(true)"
#define LOGD                        log_d
#define LOGI(fmt, ...)              log_i("\x1B[32m" fmt "\x1B[0m", ## __VA_ARGS__)
#define LOGW(fmt, ...)              log_w("\x1B[33m" fmt "\x1B[0m", ## __VA_ARGS__)
#define LOGE(fmt, ...)              log_e("\x1B[31m" fmt "\x1B[0m", ## __VA_ARGS__)
#define ASSERT(cond, msg, ...)      if (!(cond)) { LOGE(msg, ## __VA_ARGS__);  while (1) { LED_ON(); delay(100); LED_OFF(); delay(100); } }


#define WDT_TIMEOUT_SEC             (10)
#define WDT_WATCH(task)             esp_task_wdt_add(task)  // "task=NULL" -> add current thread to WDT watch
#define WDT_UNWATCH(task)           esp_task_wdt_delete(task)
#define WDT_FEED()                  esp_task_wdt_reset()

#define LED_ON()                    digitalWrite(LED_BUILTIN, HIGH)
#define LED_OFF()                   digitalWrite(LED_BUILTIN, LOW)


void global_init(void);
void dump_bytes(const void *pv, size_t sz);

#endif // __GLOBALS_H__
