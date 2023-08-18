
#include <functional>   // std::bind

#include "globals.h"

#include "chess/chess.h"
#include "ui/ui.h"
#include "wifi/wifi_setup.h"

#include "lichess_api_cfg.h"
#include "lichess_api.h"


namespace lichess
{

static APIClient        main_client;    // main connection (non-stream)

static const int        CLIENT_TIMEOUT_MS = ((WDT_TIMEOUT_SEC - 1) * 1000);


APIClient::APIClient()
{
    _config.host                = LICHESS_API_HOST;
    _config.path                = "/api";
    _config.transport_type      = HTTP_TRANSPORT_OVER_SSL;
    _config.cert_pem            = LICHESS_ORG_PEM;
    _config.event_handler       = event_handler;
    _config.timeout_ms          = CLIENT_TIMEOUT_MS; // network timeout in ms
    _config.keep_alive_enable   = true;
    _config.keep_alive_idle     = 180;
    _config.keep_alive_interval = 180;
    _config.keep_alive_count    = 3;
    _config.user_data           = this;

    _client = esp_http_client_init(&_config);

    snprintf(_auth, sizeof(_auth), "Bearer %s", LICHESS_API_ACCESS_TOKEN);
}

APIClient::~APIClient()
{
    end();
    esp_http_client_cleanup(_client);
}

esp_err_t APIClient::event_handler(esp_http_client_event_t *evt)
{
    //LOGI("%s(%d)", __func__, evt->event_id);

    APIClient *cli = (APIClient *)evt->user_data;

    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        LOGW("HTTP_EVENT_ERROR");
        break;

    case HTTP_EVENT_ON_CONNECTED:
        LOGI("HTTP_EVENT_ON_CONNECTED %s", cli->_endpoint.c_str());
        cli->_connected = true;
        break;

    case HTTP_EVENT_HEADERS_SENT:
        //LOGD("HTTP_EVENT_HEADERS_SENT");
        break;

    case HTTP_EVENT_ON_HEADER:
        //LOGD("HTTP_EVENT_ON_HEADER: key=%s, value=%s", evt->header_key, evt->header_value);
        break;

    case HTTP_EVENT_ON_DATA:
        //LOGD("HTTP_EVENT_ON_DATA: len=%d", evt->data_len);
#if 0 // enable this if using "esp_http_client_perform()"
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            int remaining = sizeof(cli->_rsp_buf) - cli->_rsp_len;
            int copy_len  = evt->data_len;
            if (copy_len > remaining) {
                LOGW("need more %d/%d bytes", copy_len - remaining, remaining);
                if (remaining < 1) {
                    return ESP_FAIL; // buffer full
                }
                copy_len = remaining;
            }
            if (copy_len > 0) {
                memcpy(cli->_rsp_buf + cli->_rsp_len, evt->data, copy_len);
                cli->_rsp_len += copy_len;
                //LOGD("response (%lu)\r\n%.*s", cli->_rsp_len, cli->_rsp_len, cli->_rsp_buf);
            }
        }
#endif
        break;

    case HTTP_EVENT_ON_FINISH:
        //LOGD("HTTP_EVENT_ON_FINISH");
        break;

    case HTTP_EVENT_DISCONNECTED:
        LOGW("HTTP_EVENT_DISCONNECTED");
        cli->_connected = false;
        break;
    }

    return ESP_OK;
}

bool APIClient::begin(const char *endpoint)
{
    esp_err_t err = ESP_OK;

    _endpoint = endpoint;
    _rsp_len  = 0;
    memset(_rsp_buf, 0, sizeof(_rsp_buf));

    if (ESP_OK != (err = esp_http_client_set_url(_client, endpoint)))
    {
        LOGW("failed set_url(%s) (err=%x)", endpoint, err);
    }
    else if (ESP_OK != (err = esp_http_client_set_header(_client, "Authorization", _auth)))
    {
        LOGW("failed set_auth(%s) (err=%x)", _auth, err);
    }

    return (ESP_OK == err);
}

void APIClient::end()
{
    esp_http_client_close(_client);
}

int APIClient::GET(bool b_retry)
{
    esp_err_t   err  = ESP_OK;
    int         code = 0;

    if (ESP_OK != (err = esp_http_client_set_method(_client, HTTP_METHOD_GET)))
    {
        LOGW("failed set_method(%d) (err=%x)", HTTP_METHOD_GET, err);
        code = -err;
    }
    else if (ESP_OK != (err = perform(NULL, 0)))
    {
        LOGW("failed get() (err=%x)", err);
        if (b_retry) {
            LOGW("retry");
            return GET(false);
        }
        code = -err;
    }
    else
    {
        code = esp_http_client_get_status_code(_client);
    }

    return code;
}

int APIClient::POST(const char *data, int len, bool b_retry)
{
    esp_err_t   err  = ESP_OK;
    int         code = 0;

    if (ESP_OK != (err = esp_http_client_set_method(_client, HTTP_METHOD_POST)))
    {
        LOGW("failed set_method(%d) (err=%x)", HTTP_METHOD_POST, err);
        code = -err;
    }
    else if ((len > 0) && (ESP_OK != (err = esp_http_client_set_header(_client, "Content-Type", "application/x-www-form-urlencoded"))))
    {
        //
    }
    else if (ESP_OK != (err = esp_http_client_set_post_field(_client, data, len)))
    {
        LOGW("failed set_post_field(%d) (err=%x)", len, err);
        code = -err;
    }
    else if (ESP_OK != (err = perform(data, len)))
    {
        LOGW("failed post() (err=%x)", err);
        if (b_retry) {
            LOGW("retry");
            return POST(data, len, b_retry);
        }
        code = -err;
    }
    else
    {
        code = esp_http_client_get_status_code(_client);
    }

    return code;
}

esp_err_t APIClient::perform(const char *buffer, int len)
{
    esp_err_t err = ESP_OK;

#if 0
    err = esp_http_client_perform(_client);
    if (ESP_ERR_HTTP_FETCH_HEADER == err)
    {
        esp_http_client_close(_client); // workaround
    }
#else
    if (ESP_OK != (err = esp_http_client_open(_client, len)))
    //if ((!_connected) && (ESP_OK != (err = esp_http_client_open(_client, len))))
    {
        LOGW("failed open(%d) (err=%x)", len, err);
    }
    else if ((NULL != buffer) && (0 != len) && (len != esp_http_client_write(_client, buffer, len)))
    {
        LOGW("failed write(%d)", len);
        err = ESP_FAIL;
    }
    else if ((len = esp_http_client_fetch_headers(_client)) < 0)
    {
        err = ESP_ERR_HTTP_FETCH_HEADER;
        LOGW("failed fetch_headers() (err=%x)", err);
      #if 1
        esp_http_client_close(_client); // workaround
      #else
        _connected = false;
      #endif
    }
    else
    {
        if (len > sizeof(_rsp_buf)) {
            LOGW("need more %d bytes", len - sizeof(_rsp_buf));
            len = sizeof(_rsp_buf);
        }
        //len = esp_http_client_read(_client, _rsp_buf, len);
        len = esp_http_client_read_response(_client, _rsp_buf, len);
        if (len > 0)
        {
            LOGD("esp_http_client_read()=%d\r\n%s", len, _rsp_buf);
            _rsp_len = len;
            err = ESP_OK;
        }
    }
#endif

    return err;
}

bool APIClient::startStream(const char *endpoint)
{
    esp_err_t   err      = ESP_OK;
    int         code     = 0;
    bool        b_status = false;

    LOGD("%s(%s)", __func__, endpoint);
    esp_http_client_set_timeout_ms(_client, CLIENT_TIMEOUT_MS);

    if (!begin(endpoint))
    {
        LOGW("begin(%s) failed", endpoint);
    }
    else if ((code = GET(true)) < 0)
    {
        LOGW("get(%s) = %d", endpoint, code);
    }
    else
    {
        esp_http_client_set_timeout_ms(_client, 10);
        b_status = code >= HttpStatus_Ok;
    }

    return b_status;
}

String APIClient::readLine(void)
{
    String ret;

    while (_connected)
    {
        char ch;
        int len = esp_http_client_read(_client, &ch, 1);
        if (len < 1) {
            //LOGW("read(1) = %d", len);
            break; // error or no data yet
        }
        if ((ch < ' ') || (ch == '\n')) {
            break; // terminator
        }
        ret += ch;
    }

    return ret;
}


/*
 * Private Functions
 */
bool api_get(const char *endpoint, DynamicJsonDocument &json, bool b_debug)
{
    int     status   = 0;
    bool    b_status = false;

    LOGD("%s(%s)", __func__, endpoint);

    if (!main_client.begin(endpoint))
    {
        LOGW("begin(%s) failed", endpoint);
    }
    else if ((status = main_client.GET(true)) >= HttpStatus_Ok)
    {
        if (b_debug) {
            LOGD("status=%d response (%lu):\r\n%s", status, main_client.get_content_length(), main_client.get_content());
        }

        json.clear();
        DeserializationError error = deserializeJson(json, main_client.get_content(), main_client.get_content_length());
        if (error)
        {
            LOGW("deserializeJson() failed: %s", error.f_str());
        }
        else
        {
            b_status = true;
        }
    }

    return b_status;
}

bool api_post(const char *endpoint, String payload, DynamicJsonDocument &json, bool b_debug)
{
    int     status   = 0;
    bool    b_status = false;

    LOGD("%s(%s)", __func__, endpoint);

    if (!main_client.begin(endpoint))
    {
        LOGW("begin(%s) failed", endpoint);
    }
    else if ((status = main_client.POST(payload.c_str(), payload.length(), true)) >= HttpStatus_Ok)
    {
        if (b_debug) {
            LOGD("status=%d response (%lu):\r\n%s", status, main_client.get_content_length(), main_client.get_content());
        }

        json.clear();
        DeserializationError error = deserializeJson(json, main_client.get_content(), main_client.get_content_length());
        if (error)
        {
            LOGW("deserializeJson() failed: %s", error.f_str());
        }
        else
        {
            b_status = true;
        }
    }

    return b_status;
}

} // namespace lichess
