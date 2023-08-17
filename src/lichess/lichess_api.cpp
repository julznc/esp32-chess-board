
#include "globals.h"

#include "chess/chess.h"
#include "ui/ui.h"
#include "wifi/wifi_setup.h"

#include "lichess_api_cfg.h"
#include "lichess_api.h"


namespace lichess
{

static ApiClient        main_client;    // main connection (non-stream)


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
                LOGW("GET %s failed: %s", endpoint, main_client.errorToString(httpCode).c_str());
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
}

} // namespace lichess
