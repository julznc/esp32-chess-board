
#include "globals.h"
#include "wifi_setup.h"


namespace wifi
{

static struct {
    nvs_handle_t    nvs;
    char            ac_ssid[64];
    char            ac_passwd[64];
} s_configs;


bool init()
{
    //LOGD("%s()", __func__);

    memset(&s_configs, 0, sizeof(s_configs));

    ESP_ERROR_CHECK( nvs_open("wifi-cfg", NVS_READWRITE, &s_configs.nvs) );

#if 0 // set initial wifi credentials
    set_credentials("myssid", "mypasswd");
    delayms(1000);
#endif
    (void)get_credentials(NULL, NULL);

    return true;
}

void loop()
{
    delayms(10);
}

bool set_credentials(const char *ssid, const char *passwd)
{
    esp_err_t   err;
    bool        b_status = false;

    if ((NULL == ssid) && (NULL == passwd))
    {
        //
    }
    else if (ESP_OK != (err = nvs_set_str(s_configs.nvs, "ssid", ssid)))
    {
        LOGW("nvs_set_str() = %d", err);
    }
    else if (ESP_OK != (err = nvs_set_str(s_configs.nvs, "passwd", passwd)))
    {
        LOGW("nvs_set_str() = %d", err);
    }
    else if (ESP_OK != (err = nvs_commit(s_configs.nvs)))
    {
        LOGW("nvs_set_str() = %d", err);
    }
    else
    {
        b_status = true;
    }

    return b_status;
}

bool get_credentials(const char **ssid, const char **passwd)
{
    esp_err_t   err;
    size_t      ssid_len = sizeof(s_configs.ac_ssid);
    size_t      passwd_len = sizeof(s_configs.ac_ssid);
    bool        b_status = false;

    if (ESP_OK != (err = nvs_get_str(s_configs.nvs, "ssid", s_configs.ac_ssid, &ssid_len)))
    {
        LOGW("nvs_get_str() = %d", err);
    }
    else if (ESP_OK != (err = nvs_get_str(s_configs.nvs, "passwd", s_configs.ac_passwd, &passwd_len)))
    {
        LOGW("nvs_get_str() = %d", err);
    }
    else
    {
        LOGD("cfg: \"%s\" - \"%s\"", s_configs.ac_ssid, s_configs.ac_passwd);
        if ((NULL != ssid) && (NULL != passwd))
        {
            *ssid = s_configs.ac_ssid;
            *passwd = s_configs.ac_passwd;
            b_status = s_configs.ac_ssid[0] && s_configs.ac_passwd[0];
        }
    }

    return b_status;
}

} // namespace wifi
