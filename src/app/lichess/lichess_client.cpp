
#include "globals.h"

#include "ui/ui.h"
#include "wifi/wifi_setup.h"

#include "lichess_client.h"


#define SHOW_STATUS(msg, ...)       DISPLAY_CLEAR_ROW(10, 8); \
                                    DISPLAY_TEXT1(0, 10, msg, ## __VA_ARGS__)

#define SHOW_OPPONENT(name, ...)    DISPLAY_CLEAR_ROW(32, 8); \
                                    DISPLAY_TEXT1(0, 32, name, ## __VA_ARGS__)

#define SET_BOTTOM_MSG(msg, ...)    DISPLAY_CLEAR_ROW(45, 9); \
                                    DISPLAY_TEXT1(0, 45, msg, ## __VA_ARGS__)

#define SET_BOTTOM_MENU(msg, ...)   DISPLAY_CLEAR_ROW(54, 9); \
                                    DISPLAY_TEXT1(0, 54, msg, ## __VA_ARGS__)

#define CLEAR_BOTTOM_MENU()         DISPLAY_CLEAR_ROW(45, 18);

namespace lichess
{

static ApiClient        main_client;    // main connection (non-stream'ed)
static ApiClient        stream_client;

static char             ac_username[32];
static uint8_t          u8_error_count;

static enum {
    CLIENT_STATE_INIT,
    CLIENT_STATE_GET_ACCOUNT,   // check lichess account/profile
    CLIENT_STATE_CHECK_EVENTS,  // check messages from eventd-stream
    CLIENT_STATE_CHECK_GAME,    // check messages from game-state stream
    CLIENT_STATE_CHECK_BOARD,   // get board status
    CLIENT_STATE_IDLE
} e_state;

static struct {
    nvs_handle_t    nvs;
    char            ac_token[64]; // api token
    // challenge
    char            ac_opponent[64];
    uint16_t        u16_clock_limit;
    uint8_t         u8_clock_increment;
    uint8_t         b_rated;
} s_configs;


static bool get_account();


bool init()
{
    //LOGD("%s()", __func__);

    SecClient::lib_init();

    memset(&ac_username, 0, sizeof(ac_username));
    memset(&s_configs, 0, sizeof(s_configs));

    if (ESP_OK != nvs_open("lichess", NVS_READWRITE, &s_configs.nvs))
    {
        LOGW("open configs failed");
        s_configs.nvs = 0;
    }
    else
    {
#if 0 // initial test
        set_token("my_Token");
#endif
        if (get_token(NULL)) {
            ApiClient::lib_init(s_configs.ac_token);
        }

    }

    u8_error_count = 0;
    e_state = CLIENT_STATE_INIT;

    return true;
}

void loop()
{
    switch (e_state)
    {
    case CLIENT_STATE_INIT:
        if ((u8_error_count < 10) && wifi::connected())
        {
            if (!ac_username[0]) {
                delayms(2500); // initial delay
            }
            e_state = CLIENT_STATE_GET_ACCOUNT;
        }
        else
        {
            delayms(1500UL);
        }
        break;

    case CLIENT_STATE_GET_ACCOUNT:
        if (get_account())
        {
            e_state = CLIENT_STATE_CHECK_EVENTS;
        }
        else
        {
            delayms(2500);
            u8_error_count++;
            e_state = CLIENT_STATE_INIT;
        }
        break;

    case CLIENT_STATE_CHECK_EVENTS:
        break;

    case CLIENT_STATE_CHECK_GAME:
        break;

    case CLIENT_STATE_CHECK_BOARD:
        break;

    case CLIENT_STATE_IDLE:
        break;

    default:
        e_state = CLIENT_STATE_INIT;
        break;
    }
}

bool get_token(const char **token)
{
    esp_err_t   err;
    size_t      len = sizeof(s_configs.ac_token);
    bool        b_status = false;

    if (ESP_OK != (err = nvs_get_str(s_configs.nvs, "token", s_configs.ac_token, &len)))
    {
        LOGW("nvs_get_str() = %d", err);
    }
    else
    {
        if (NULL != token)
        {
            *token = s_configs.ac_token;
        }
        //LOGD("token (%u): \"%s\"", len, s_configs.ac_token);
        b_status = (0 != s_configs.ac_token[0]);
    }

    return b_status;
}

bool set_token(const char *token)
{
    esp_err_t   err;
    bool        b_status = false;

    if (NULL == token)
    {
        //
    }
    else if (ESP_OK != (err = nvs_set_str(s_configs.nvs, "token", token)))
    {
        LOGW("nvs_set_str() = %d", err);
    }
    else if (ESP_OK != (err = nvs_commit(s_configs.nvs)))
    {
        LOGW("nvs_commit() = %d", err);
    }
    else
    {
        b_status = true;
    }

    return b_status;
}

static bool get_account()
{
    cJSON  *root     = NULL;
    cJSON  *item     = NULL;
    bool    b_status = false;

    SHOW_STATUS("lichess.org ...");

    if (ac_username[0])
    {
        SHOW_STATUS("%.*s", 20, ac_username);
        b_status = true;
    }
    else if (!main_client.api_get("/api/account", &root, false))
    {
        SHOW_STATUS("lichess api error");
    }
    else if (NULL == (item = cJSON_GetObjectItem(root, "username")))
    {
        LOGW("unknown username");
        SHOW_STATUS("unknown username");
    }
    else
    {
        const char *username = cJSON_GetStringValue(item);
        LOGI("username : %s", username);
        strncpy(ac_username, username, sizeof(ac_username) - 1);
        SHOW_STATUS("%.*s", 20, ac_username);
        b_status = true;
    }

    if (root)
    {
        cJSON_Delete(root);
    }

    return b_status;
}

} // namespace lichess
