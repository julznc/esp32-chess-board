
#include <functional>   // std::bind

#include "globals.h"

#include "chess/chess.h"
#include "ui/ui.h"
#include "wifi/wifi_setup.h"

#include "lichess_api_cfg.h"
#include "lichess_api.h"


namespace lichess
{

#if 0
static ApiClient        main_client;    // main connection (non-stream)
#else
static APIClient        main_client;    // main connection (non-stream)
#endif


APIClient::APIClient()
{
    _config.host                = LICHESS_API_HOST;
    _config.path                = "/api";
    _config.transport_type      = HTTP_TRANSPORT_OVER_SSL;
    _config.cert_pem            = LICHESS_ORG_PEM;
    _config.event_handler       = event_handler;
    _config.timeout_ms          = 10000; // network timeout in ms
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
        LOGI("HTTP_EVENT_ON_CONNECTED");
        cli->_connected = true;
        break;

    case HTTP_EVENT_HEADERS_SENT:
        LOGD("HTTP_EVENT_HEADERS_SENT");
        break;

    case HTTP_EVENT_ON_HEADER:
        LOGD("HTTP_EVENT_ON_HEADER: key=%s, value=%s", evt->header_key, evt->header_value);
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
        LOGD("HTTP_EVENT_ON_FINISH");
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
        esp_http_client_close(_client); // workaround
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
        b_status = code >= HttpStatus_Ok;
    }

    return b_status;
}

String APIClient::readLine(void)
{
    String ret;

    while (1)
    {
        char ch;
        int len = esp_http_client_read(_client, &ch, 1);
        if (len < 1) {
            LOGW("read(1) = %d", len);
            break; // error
        }
        if (len < 0) {
            break; // no data yet
        }
        if ((ch < ' ') || (ch == '\n')) {
            break; // terminator
        }
        ret += ch;
    }

    return ret;
}

ApiClient::SecClient::SecClient() : WiFiClientSecure()
{
    // to do: load from preferences
    _CA_cert = LICHESS_ORG_PEM; // setCACert()
}

int ApiClient::SecClient::connect(const char *host, uint16_t port, int32_t timeout)
{
    uint32_t ms_start = millis();
    LOGD("call WiFiClientSecure::connect(%s:%u)", host, port);
    int result = WiFiClientSecure::connect(host, port, timeout);
    LOGI("%s() = %d (%lums)", __func__, result, millis() - ms_start);
    return result;
}

uint8_t ApiClient::SecClient::connected()
{
#if 0 // slow?
    uint32_t ms_start = millis();
    LOGD("call WiFiClientSecure::connected()");
    uint8_t result = WiFiClientSecure::connected();
    LOGI("%s() = %d (%lums)", __func__, result, millis() - ms_start);
    return result;
#elif 0
    return WiFiClientSecure::connected();
#else
  #if 0
    uint8_t dummy = 0;
    read(&dummy, 0);
  #else
    mbedtls_ssl_read(&sslclient->ssl_ctx, NULL, 0);
  #endif

    return _connected;
#endif
}

int ApiClient::SecClient::available()
{
#if 0
    return WiFiClientSecure::available();
#else
    int peeked = (_peek >= 0);
    if (!_connected) {
        return peeked;
    }
    int res = data_to_read(sslclient);
    if (res < 0) {
        LOGW("data_to_read() = %d", res);
      #if 1
        stop();
        return peeked ? peeked : res;
      #else
        if (MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY != res) {
            stop();
        }
        return peeked ? peeked : res;
      #endif
    }
    return res + peeked;
#endif
}

int ApiClient::SecClient::read(uint8_t *buf, size_t size)
{
#if 0
    return WiFiClientSecure::read(buf, size);
#else
    int peeked = 0;
    int avail = available();
    if ((!buf && size) || avail <= 0) {
        return -1;
    }
    if(!size){
        return 0;
    }
    if(_peek >= 0){
        buf[0] = _peek;
        _peek = -1;
        size--;
        avail--;
        if(!size || !avail){
            return 1;
        }
        buf++;
        peeked = 1;
    }

    int res = get_ssl_receive(sslclient, buf, size);
    if (res < 0) {
        LOGW("get_ssl_receive() = %d", res);
        stop();
        return peeked?peeked:res;
    }
    return res + peeked;
#endif
}

ApiClient::ApiClient() : HTTPClient(), _auth("Bearer ")
{
    // to do: load from preferences
    _auth += LICHESS_API_ACCESS_TOKEN;
}

bool ApiClient::begin(const char *endpoint)
{
    _client     = &_secClient;
    _secure     = true;

    _protocol   = LICHESS_API_PROTOCOL;
    _host       = LICHESS_API_HOST;
    _port       = LICHESS_API_PORT;
    _uri        = endpoint;

    LOGD("protocol: %s, host: %s port: %d url: %s", _protocol.c_str(), _host.c_str(), _port, _uri.c_str());

    return true;
}

bool ApiClient::connect(void)
{
    bool b_status;
    uint32_t ms_start = millis();
#if 1
    b_status = HTTPClient::connect();
#else
    b_status = _secClient.connected();
    if (!b_status)
    {
        if (!_secClient.connect(_host.c_str(), _port, _connectTimeout))
        {
            LOGW("failed connect to %s:%u", _host.c_str(), _port);
            delay(5000UL);
        }
        else
        {
            _client->setTimeout((_tcpTimeout + 500) / 1000);
            LOGD("connected to %s:%u", _host.c_str(), _port);
            b_status = true; //_secClient.connected();
        }
    }
#endif
    LOGI("%s() = %d (%lums)", __func__, b_status, millis() - ms_start);
    return b_status;
}

void ApiClient::end(bool b_stop)
{
    if (_secClient.available()) {
        _secClient.flush();
    }
    if (b_stop) {
        _secClient.stop();
    }
    clear();
}

int ApiClient::sendRequest(const char *type, uint8_t *payload, size_t size)
{
#if 0
    HTTPClient::addHeader("Authorization", _auth);
    return HTTPClient::sendRequest(type, payload, size);

#else // to do: handle redirects, if any

    int code = 0;

    addHeader("Authorization", _auth);

    // wipe out any existing headers from previous request
    for (size_t i = 0; i < _headerKeysCount; i++) {
        if (_currentHeaders[i].value.length() > 0) {
            _currentHeaders[i].value.clear();
        }
    }

    if (!connect())
    {
        code = HTTPC_ERROR_CONNECTION_REFUSED;
        LOGW("connect() failed");
    }
    else
    {
        if (payload && size > 0) {
            addHeader(F("Content-Length"), String(size));
        }

        // add cookies to header, if present
        String cookie_string;
        if (generateCookieString(&cookie_string)) {
            addHeader("Cookie", cookie_string);
        }

        // send Header
        if (!sendHeader(type))
        {
            LOGW("sendHeader(%s) failed", type);
            _secClient.stop();
            code = HTTPC_ERROR_SEND_HEADER_FAILED;
        }
        // send payload if needed
        else if (payload && (size > 0) && (_secClient.write(&payload[0], size) != size))
        {
            LOGW("send payload failed");
            _secClient.stop();
            code = HTTPC_ERROR_SEND_PAYLOAD_FAILED;
        }
        else
        {
            code = handleHeaderResponse();
        }
    }

    return code;
#endif
}

int ApiClient::handleHeaderResponse()
{
    int code;
    uint32_t ms_start = millis();
    code = HTTPClient::handleHeaderResponse();
    LOGI("handleHeaderResponse() = %d (%lums)", code, millis() - ms_start);
    return code;
}

bool ApiClient::startStream(const char *endpoint)
{
    int     code        = 0;
    bool    b_status    = false;

    // to do: handle redirects, if any

    if (begin(endpoint))
    {
        addHeader("Authorization", _auth);

        // wipe out any existing headers from previous request
        for (size_t i = 0; i < _headerKeysCount; i++) {
            if (_currentHeaders[i].value.length() > 0) {
                _currentHeaders[i].value.clear();
            }
        }

        if (!connect())
        {
            LOGW("connect() failed");
        }
        else if (!sendHeader("GET"))
        {
            LOGW("sendHeader() failed");
        }
        else if ((code = handleHeaderResponse()) > 0)
        {
            b_status = (HTTP_CODE_OK == code);
        }
    }

    if (!b_status) {
        end(true);
    }

    return b_status;
}

String ApiClient::readLine(void)
{
    if (_secClient.available())
    {
        String line = _secClient.readStringUntil('\n');
        line.trim(); // remove \r
        return line;
    }
    return "";
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
#if 1
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
#else
    bool b_status = false;
    bool b_stop   = false;

    if (!main_client.begin(endpoint))
    {
        LOGW("begin(%s) failed", endpoint);
    }
    else
    {
        if (!payload.isEmpty())
        {
            main_client.addHeader("Content-Type", "application/x-www-form-urlencoded");
        }

        int httpCode = main_client.sendRequest("POST", (uint8_t *)payload.c_str(), payload.length());
        if (httpCode > 0)
        {
            String resp = main_client.readLine();
            if (b_debug) {
                LOGD("response (%u):\r\n%s", resp.length(), resp.c_str());
            }

            if ((HTTP_CODE_OK == httpCode) || (HTTP_CODE_CREATED == httpCode) || (HTTP_CODE_MOVED_PERMANENTLY == httpCode))
            {
                if (resp.length() > 0)
                {
                    json.clear();
                    DeserializationError error = deserializeJson(json, resp);
                    if (error)
                    {
                        LOGW("deserializeJson() failed: %s", error.f_str());
                    }
                    else
                    {
                        b_status = true;
                    }
                }
            }
            else
            {
                LOGW("POST %s failed: %s", endpoint, main_client.errorToString(httpCode).c_str());
            }
        }
        else
        {
            LOGW("error code %d", httpCode);
            b_stop = true;
            main_client.end(true);
        }

    }

    //main_client.end(b_stop);

    return b_status;
#endif
}

} // namespace lichess
