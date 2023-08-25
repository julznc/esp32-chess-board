#include <atomic>

#include "globals.h"
#include "display.h"

#include <Fonts/FreeSans9pt7b.h>
//#include <Fonts/FreeSansBold9pt7b.h>


namespace ui::display
{


Adafruit_SH1106G oled(SCREEN_WIDTH, SCREEN_HEIGHT);
const GFXfont *font2 = &FreeSans9pt7b;
//const GFXfont *font2 = &FreeSansBold9pt7b;

static SemaphoreHandle_t mtx             = NULL;
static std::atomic<bool> b_display_found;
static std::atomic<bool> b_batt_ok;
static uint8_t           u8_error_count  = 0;


bool init(void)
{
    if (NULL == mtx)
    {
        mtx = xSemaphoreCreateMutex();
        ASSERT((NULL != mtx), "display mutex allocation failed");

        oled.begin();
    }

    b_display_found = false;
    b_batt_ok       = false;

    if (!oled.init())
    {
        LOGW("OLED not found");
        b_display_found = false;
      #if 1 // skip checking of battery level if testing with mcu-board only
        if (++u8_error_count >= 3) {
            b_batt_ok       = true;
            u8_error_count  = 0;
        }
      #endif
        delay(5000UL);
    }
    else
    {
        oled.setTextColor(SH110X_WHITE, SH110X_BLACK);
        oled.cp437(true);

        oled.splash();
        oled.display();
        LOGD("OLED ok");
        b_display_found = true;
        u8_error_count  = 0;
    }

    return b_display_found;
}

bool lock()
{
    return b_display_found && (pdTRUE == xSemaphoreTake(ui::display::mtx, portMAX_DELAY));
}

void unlock()
{
    xSemaphoreGive(ui::display::mtx);
}

void showBattLevel(void)
{
    static bool     b_warned = false;
    static uint16_t prev_raw = BATT_LEVEL_MIN / 2;
    uint16_t        raw      = analogRead(BATT_ADC_PIN);

    if (prev_raw != raw)
    {
        float f_batt = ((float)(raw + prev_raw) / 2) * BATT_ADC_SCALE;
        //DISPLAY_TEXT1(0, 20, "Batt %.2fV", f_batt);
        DISPLAY_TEXT1(95, 0, "%.2fV", f_batt);
        b_batt_ok   = (raw >= BATT_LEVEL_MIN);
        prev_raw    = raw;

        if (!b_batt_ok && !b_warned)
        {
            LOGW("low battery level");
            DISPLAY_TEXT1(10, 10, "LOW BATTERY LEVEL");
            b_warned = true;
        }
    }
}

bool battLevelOk(void)
{
    return b_batt_ok;
}

} // namespace ui::display
