
#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include <Arduino.h>
#include <esp_task_wdt.h>


// app firmware info
#define K_APP_NAME                  "Electronic Chess Board"
#define K_APP_VERSION               "1.03.04"


// for esp32-s2: RX1=18, TX1=17
#define HWSERIAL                    Serial1
#define PRINTF                      HWSERIAL.printf

/*** PIN OUTS ***/

// UI
#define I2C_SDA_PIN                 (9)
#define I2C_SCL_PIN                 (10)
#define LED_STRIP_PIN               (21)

#define BTN_1_PIN                   (11)    // SW3 (upper left)
#define BTN_2_PIN                   (12)    // SW2 (lower left)
#define BTN_3_PIN                   (14)    // SW4 (lower right)

// spi bus
#define RFID_SPI_BUS                HSPI // SPI2
#define RFID_MISO_PIN               (33)
#define RFID_MOSI_PIN               (34)
#define RFID_SCK_PIN                (35)
#define RFID_SS_PIN                 (1)     // active high (inverted)
#define RFID_RST_PIN                (5)     // active low

// multiplexed via HC238
#define RFID_RST_A_PIN              (8)
#define RFID_RST_B_PIN              (7)
#define RFID_RST_C_PIN              (6)

// multiplexed via HC138
#define RFID_SS_A_PIN               (4)
#define RFID_SS_B_PIN               (3)
#define RFID_SS_C_PIN               (2)



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
