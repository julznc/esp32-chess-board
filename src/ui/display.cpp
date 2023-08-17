
#include "globals.h"
#include "display.h"

#include <Fonts/FreeSans9pt7b.h>
//#include <Fonts/FreeSansBold9pt7b.h>


namespace ui::display
{


Adafruit_SH1106G oled(SCREEN_WIDTH, SCREEN_HEIGHT);
const GFXfont *font2 = &FreeSans9pt7b;
//const GFXfont *font2 = &FreeSansBold9pt7b;

static SemaphoreHandle_t mtx = NULL;
static bool b_display_found  = false;


bool init(void)
{
    if (NULL == mtx)
    {
        mtx = xSemaphoreCreateMutex();
        ASSERT((NULL != mtx), "display mutex allocation failed");

        oled.begin();
    }

    if (!oled.init())
    {
        LOGW("OLED not found");
        b_display_found = false;
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
    static uint16_t  prev_raw = 0;
    uint16_t         raw      = analogRead(BATT_ADC_PIN);

    if (prev_raw != raw)
    {
        float f_batt = ((float)(raw + prev_raw) / 2) * BATT_ADC_SCALE;
        //DISPLAY_TEXT1(0, 20, "Batt %.2fV", f_batt);
        DISPLAY_TEXT1(95, 0, "%.2fV", f_batt);
        prev_raw = raw;
    }
}

} // namespace ui::display
