
#include <esp_wifi.h>
#include "globals.h"
#include "apiclient.h"


namespace lichess
{

static const char *API_HOST = LICHESS_API_HOST;
const char *ApiClient::pc_token = NULL;
int ApiClient::num_connect_errors = 0;


ApiClient::ApiClient()
{
    memset(_auth, 0, sizeof(_auth));
    memset(_uri, 0, sizeof(_uri));
}

void ApiClient::lib_init(const char *token)
{
    pc_token = token;
}

bool ApiClient::begin(const char *endpoint)
{
    if (0 == _auth[0])
    {
        if ((NULL == pc_token) || (*pc_token <= ' '))
        {
            LOGW("unknown access");
            return false;
        }
        snprintf(_auth, sizeof(_auth) - 1, "Bearer %s", pc_token);
        LOGI("auth: %s", pc_token);
    }

    strncpy(_uri, endpoint, sizeof(_uri) - 1);
    LOGD("%s", _uri);

    return true;
}

bool ApiClient::connect()
{
    static const int max_connect_errors = 3;
    uint32_t        ms_start = millis();
    bool            b_status = _secClient.connected();

    if (!b_status)
    {
        //LOGD("heap before tls %lu", ESP.getFreeHeap());
        if (!_secClient.connect())
        {
            num_connect_errors++;
            LOGW("failed connect to %s (%d/%d)", API_HOST, num_connect_errors, max_connect_errors);
            if (num_connect_errors >= max_connect_errors)
            {
                LOGW("max connect errors. reconnect wifi.");
                end(true);
                (void)esp_wifi_disconnect();
                num_connect_errors = 0;
            }
        }
        else
        {
            //LOGD("connected to %s", API_HOST);
            num_connect_errors = 0;
            b_status = _secClient.connected();
        }
    }

    LOGD("%s %lums (fd %d heap %u)", b_status ? "ok" : "failed", millis() - ms_start, _secClient.fd(), heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    return b_status;
}

void ApiClient::end(bool b_stop)
{
    if (_secClient.available()) {
        _secClient.flush();
    }
    if (b_stop) {
        _secClient.stop(true);
    }
    memset(_uri, 0, sizeof(_uri));
    _returnCode = 0;
    _size = -1;
}

int ApiClient::sendRequest(const char *type, const uint8_t *payload, size_t size)
{
    int code = 0;

    if (!connect())
    {
        code = HTTPC_ERROR_CONNECTION_REFUSED;
        //LOGW("connect() failed");
    }
    else
    {
        // send Header
        if (!sendHeader(type, (payload && size > 0) ? size : 0))
        {
            LOGW("sendHeader(%s) failed", type);
            code = HTTPC_ERROR_SEND_HEADER_FAILED;
        }
        // send payload if needed
        else if (payload && (size > 0) && (_secClient.write(payload, size) != size))
        {
            LOGW("send payload failed");
            code = HTTPC_ERROR_SEND_PAYLOAD_FAILED;
        }
        else
        {
            code = handleHeaderResponse();
        }
    }

    return code;
}

bool ApiClient::startStream(const char *endpoint)
{
    int     code        = 0;
    bool    b_status    = false;

    // to do: handle redirects, if any

    if (begin(endpoint))
    {
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
        end(true);
    }

    return b_status;
}

int ApiClient::readline(char *buf, size_t size, uint32_t timeout)
{
    if (_secClient.available())
    {
        return _secClient.readline(buf, size, timeout);
    }
    return 0;
}

bool ApiClient::sendHeader(const char *type, size_t content_length)
{
    char    header_buf[256];
    size_t  header_len = 0;

    if (connected())
    {
  #define ADD_HEADER(hdr, ...)    header_len += snprintf(header_buf + header_len, sizeof(header_buf) - header_len, hdr "\r\n", ## __VA_ARGS__)

        ADD_HEADER("%s %s HTTP/1.1", type, _uri);
        ADD_HEADER("Host: %s", API_HOST);
        ADD_HEADER("Connection: %s", "keep-alive");
        //ADD_HEADER("User-Agent: %s", "esp32-chess-board");
        ADD_HEADER("Authorization: %s", _auth);
        if (content_length) {
            ADD_HEADER("Content-Type: %s", "application/x-www-form-urlencoded");
            ADD_HEADER("Content-Length: %d", content_length);
        }

        ADD_HEADER(""); // end \r\n

        return (header_len == _secClient.write((const uint8_t *)header_buf, header_len));
    }
    return false;
}

int ApiClient::handleHeaderResponse()
{
    char line_buff[512];
    char line_len = 0;

    _returnCode = 0;
    _size = -1;

    unsigned long lastDataTime = millis();

    while (connected())
    {
        if (_secClient.available() > 0)
        {
            memset(line_buff, 0, sizeof(line_buff));
            line_len = _secClient.readline(line_buff, sizeof(line_buff) - 1, 0);

            lastDataTime = millis();

            //LOGD("[rx] %s", line_buff);
            char *sep = strchr(line_buff, ':');

            if (0 == strncmp(line_buff, "HTTP", 4)) // first line
            {
                //LOGD("%s", line_buff);
                if (NULL != (sep = strchr(line_buff, ' '))) {
                    _returnCode = atoi(sep + 1);
                }
            }
            else if (sep)
            {

                *sep++ = 0; // ':'
                *sep++ = 0; // ' '
                if (0 == strcasecmp(line_buff, "Content-Length"))
                {
                    _size = atoi(sep);
                }
                else
                {
                    //LOGD("name=%s value=%s", line_buff, sep);
                }
            }
            else if (0 == line_len)
            {
                if (_size > 0) {
                    //LOGD("size: %d", _size);
                }

                if (_returnCode) {
                    return _returnCode;
                } else {
                    LOGW("Remote host is not an HTTP Server!");
                    return HTTPC_ERROR_NO_HTTP_SERVER;
                }
            }

        } else {
            if ((millis() - lastDataTime) > LICHESS_API_TIMEOUT_MS) {
                return HTTPC_ERROR_READ_TIMEOUT;
            }
            delayms(10);
        }
    }

    return HTTPC_ERROR_CONNECTION_LOST;
}


bool ApiClient::api_get(const char *endpoint, cJSON **response, bool b_debug)
{
    bool b_status = false;

    if (!begin(endpoint))
    {
        LOGW("begin(%s) failed", endpoint);
    }
    else
    {
        int httpCode = sendRequest("GET");
        if (httpCode > 0)
        {
            memset(_rsp_buf, 0, sizeof(_rsp_buf));
            int response_len = readline(_rsp_buf, sizeof(_rsp_buf) - 1, 0);

            if (b_debug) {
                LOGD("payload (%d):\r\n%s", response_len, _rsp_buf);
            }

            if ((HTTP_CODE_OK == httpCode) || (HTTP_CODE_BAD_REQUEST == httpCode))
            {
                if (response_len > 0)
                {
                    if ((NULL != response) && (NULL == (*response = cJSON_Parse(_rsp_buf))))
                    {
                        LOGW("parse json failed (%s)", cJSON_GetErrorPtr());
                    }
                    else
                    {
                        b_status = true;
                    }
                }
            }
            else
            {
                LOGW("GET %s failed (%d)", endpoint, httpCode);
            }
        }
        else
        {
            LOGW("error code %d", httpCode);
            end(false);
        }
    }

    return b_status;
}

} // namespace lichess
