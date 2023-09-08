
#include "globals.h"

#include "chess/chess.h"
#include "ui/ui.h"
#include "wifi/wifi_setup.h"

#include "lichess_api_cfg.h"
#include "lichess_api.h"


namespace lichess
{

static  ApiClient   main_client;    // main connection (non-stream)
int     ApiClient::num_connect_errors = 0;


ApiClient::SecClient::SecClient() : WiFiClientSecure()
{
    _timeout = 10;
    sslclient->handshake_timeout = 10000;

#if 1
    _CA_cert = LICHESS_ORG_PEM; // setCACert()
#else // only ~100ms less connect() duration
    setInsecure();
#endif
}

void ApiClient::SecClient::stop()
{
    WiFiClientSecure::stop();

#if 1 // re-initialize after stop_ssl_socket() -- required?
    mbedtls_ssl_init(&sslclient->ssl_ctx);
    mbedtls_ssl_config_init(&sslclient->ssl_conf);
    mbedtls_ctr_drbg_init(&sslclient->drbg_ctx);
#endif
}

uint8_t ApiClient::SecClient::connected()
{
#if 0
    return WiFiClientSecure::connected();
#else
    int err = mbedtls_ssl_read(&sslclient->ssl_ctx, NULL, 0);
    if ((err < 0) && (MBEDTLS_ERR_SSL_WANT_READ != err) && (MBEDTLS_ERR_SSL_WANT_WRITE != err))
    {
        //LOGW("ssl_read(0) = -0x%x", -err);
        stop();
    }
    return _connected;
#endif
}


ApiClient::ApiClient() : HTTPClient()
{
    _connectTimeout = 10000; // ms
    _tcpTimeout     = 10000; // ms
}

bool ApiClient::begin(const char *endpoint)
{
    String token;

    get_token(token);

    if (token.isEmpty())
    {
        LOGW("unknown access");
        return false;
    }

    _auth       = "Bearer ";
    _auth      += token;

    _client     = &_secClient;
    _secure     = true;

    _protocol   = LICHESS_API_PROTOCOL;
    _host       = LICHESS_API_HOST;
    _port       = LICHESS_API_PORT;
    _uri        = endpoint;

    //LOGD("protocol: %s, host: %s port: %d url: %s", _protocol.c_str(), _host.c_str(), _port, _uri.c_str());
    LOGD("%s", _uri.c_str());

    return true;
}

bool ApiClient::connect(void)
{
    static const int max_connect_errors = 3;
    bool b_status;
    uint32_t ms_start = millis();
#if 0
    b_status = HTTPClient::connect();
#else
    b_status = _secClient.connected();
    if (!b_status)
    {
        //LOGD("heap before tls %lu", ESP.getFreeHeap());
        if (!_secClient.connect(_host.c_str(), _port, _connectTimeout))
        {
            num_connect_errors++;
            LOGW("failed connect to %s:%u (%d/%d)", _host.c_str(), _port, num_connect_errors, max_connect_errors);
            if (num_connect_errors >= max_connect_errors)
            {
                LOGE("max connect errors. reconnect wifi.");
                wifi::disconnect();
                num_connect_errors = 0;
            }
        }
        else
        {
            _client->setTimeout((_tcpTimeout + 500) / 1000);
            //LOGD("connected to %s:%u", _host.c_str(), _port);
            num_connect_errors = 0;
            b_status = _secClient.connected();
        }
    }
#endif
    LOGD("%s %s %lums (heap %lu)", __func__, b_status ? "ok" : "failed", millis() - ms_start, ESP.getFreeHeap());
    return b_status;
}

bool ApiClient::connected()
{
    return _secClient.connected();
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
            //LOGW("connect() failed");
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
        //LOGW("stream(%s) failed", endpoint);
        _uri = "";
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
    bool b_status = false;
    bool b_stop   = false;

    if (!main_client.begin(endpoint))
    {
        LOGW("begin(%s) failed", endpoint);
    }
    else
    {
        int httpCode = main_client.sendRequest("GET");
        if (httpCode > 0)
        {
            String payload = main_client.readLine();
            if (b_debug) {
                LOGD("payload (%u):\r\n%s", payload.length(), payload.c_str());
            }

            if ((HTTP_CODE_OK == httpCode) || (HTTP_CODE_MOVED_PERMANENTLY == httpCode))
            {
                if (payload.length() > 0)
                {
                    json.clear();
                    DeserializationError error = deserializeJson(json, payload);
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
                LOGW("GET %s failed %s", endpoint, main_client.errorToString(httpCode).c_str());
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
}

bool api_post(const char *endpoint, String payload, DynamicJsonDocument &json, bool b_debug)
{
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
                LOGD("response (%u): %s", resp.length(), resp.c_str());
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
                LOGW("POST %s failed %s", endpoint, main_client.errorToString(httpCode).c_str());
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
}

} // namespace lichess
