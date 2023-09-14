
#include "globals.h"

#include "chess/chess.h"
#include "ui/ui.h"
#include "wifi/wifi_setup.h"

#include "lichess_api_cfg.h"
#include "lichess_api.h"


namespace lichess
{

static  ApiClient   main_client;    // main connection (non-stream'ed)
static  char        response_buff[2048];
int     ApiClient::num_connect_errors = 0;


ApiClient::SecClient::SecClient() : _sock_fd(-1), _b_init_done(false), _b_connected(false)
{
    memset(&_server_addr, 0, sizeof(_server_addr));
}

ApiClient::SecClient::~SecClient()
{
    stop(true);
}

int ApiClient::SecClient::connect()
{
    _b_connected = false;

    if (0 != init_ssl())
    {
        // cert error, etc.
    }
    else if (connect_socket() <= 0)
    {
        // failed to init socket
    }
    else if (0 != connect_ssl())
    {
        stop(false);
    }
    else
    {
        _b_connected = true;
    }

    return _b_connected;
}

uint8_t ApiClient::SecClient::connected()
{
    if (_b_connected)
    {
        int err = mbedtls_ssl_read(&_ssl_ctx, NULL, 0);

        if ((err < 0) && (MBEDTLS_ERR_SSL_WANT_READ != err) && (MBEDTLS_ERR_SSL_WANT_WRITE != err))
        {
            if (MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY == err)
            {
                //LOGD("peer closed connection");
            }
            else
            {
                LOGW("ssl_read(0) = -0x%x", -err);
            }
            stop(false);
        }
    }

    return _b_connected;
}

void ApiClient::SecClient::stop(bool b_deinit)
{
    //mbedtls_ssl_close_notify(&_ssl_ctx);
    mbedtls_ssl_session_reset(&_ssl_ctx);

    if (b_deinit) {
        memset(&_server_addr, 0, sizeof(_server_addr));
        deinit_ssl();
    }

    if (_sock_fd > 0){
        lwip_shutdown(_sock_fd, 2);
        lwip_close(_sock_fd);
        _sock_fd = -1;
    }

    _b_connected = false;
}

size_t ApiClient::SecClient::write(const uint8_t *buf, size_t size)
{
    int res = 0;

    if (_b_connected)
    {
        if ((res = send_ssl(buf, size)) < 0) {
            stop(false);
            res = 0;
        }
    }

    return res;
}

int ApiClient::SecClient::available()
{
    return connected() ? mbedtls_ssl_get_bytes_avail(&_ssl_ctx) : 0;
}

int ApiClient::SecClient::read(uint8_t *buf, size_t size)
{
    return _b_connected ? mbedtls_ssl_read(&_ssl_ctx, buf, size) : 0;
}

int ApiClient::SecClient::readline(char *buf, size_t size, uint32_t timeout)
{
    uint32_t    read_timeout = millis() + (timeout ? timeout : LICHESS_API_TIMEOUT_MS);
    int         len = 0;
    uint8_t     ch  = 0;

    if (!_b_connected)
    {
        return 0;
    }

    do {
        int ret = 0;
        if ((ret = mbedtls_ssl_read(&_ssl_ctx, &ch, 1)) > 0)
        {
            if (ch >= ' ') { // readable chars only
                read_timeout = millis() + 10; // shorter timeout
                *buf++ = ch;
                len++;
            } else if (ch == '\n') {
                break;
            }
        }
        else if ((0 == ret) || (MBEDTLS_ERR_SSL_WANT_READ == ret) || (MBEDTLS_ERR_SSL_WANT_WRITE == ret))
        {
            if (millis() > read_timeout) {
                //LOGW("read timed out");
                break;
            }
        }
        else
        {
            LOGE("mbedtls_ssl_read() = -0x%x", -ret);
            //len = 0;
            break;
        }
        vTaskDelay(2);
    } while (len < size);

    return len;
}

void ApiClient::SecClient::flush()
{
    // to do
    //mbedtls_ssl_flush_output(&_ssl_ctx);
}


int ApiClient::SecClient::init_ssl()
{
    static const char *pers = "esp32-tls";
    int err = 0;

    if (_b_init_done)
    {
        return 0;
    }

    mbedtls_ssl_init(&_ssl_ctx);
    mbedtls_ssl_config_init(&_ssl_conf);
    mbedtls_x509_crt_init(&_ca_cert);
    mbedtls_ctr_drbg_init(&_ctr_drbg);
    mbedtls_entropy_init(&_entropy_ctx);

    if (0 != (err = mbedtls_ctr_drbg_seed(&_ctr_drbg, mbedtls_entropy_func,
                                        &_entropy_ctx, (const unsigned char *)pers, strlen(pers))))
    {
        LOGW("mbedtls_ctr_drbg_seed() failed (err = %d)", err);
    }
    else if (0 != (err = mbedtls_x509_crt_parse(&_ca_cert, (const unsigned char *)LICHESS_ORG_PEM, strlen(LICHESS_ORG_PEM) + 1)))
    {
        LOGW("mbedtls_x509_crt_parse() failed (err = %d)", err);
    }
    else if (0 != (err = mbedtls_ssl_set_hostname(&_ssl_ctx, LICHESS_API_HOST)))
    {
        LOGW("mbedtls_ssl_set_hostname() failed (err = %d)", err);
    }
    else if (0 != (err = mbedtls_ssl_config_defaults(&_ssl_conf,
                                                    MBEDTLS_SSL_IS_CLIENT,
                                                    MBEDTLS_SSL_TRANSPORT_STREAM,
                                                    MBEDTLS_SSL_PRESET_DEFAULT)))
    {
        LOGW("mbedtls_ssl_config_defaults() failed (err = %d)", err);
    }
    else
    {
        mbedtls_ssl_conf_authmode(&_ssl_conf, MBEDTLS_SSL_VERIFY_REQUIRED);
        mbedtls_ssl_conf_ca_chain(&_ssl_conf, &_ca_cert, NULL);
        mbedtls_ssl_conf_rng(&_ssl_conf, mbedtls_ctr_drbg_random, &_ctr_drbg);

        if (0 != (err = mbedtls_ssl_setup(&_ssl_ctx, &_ssl_conf)))
        {
            LOGW("mbedtls_ssl_setup() failed (err = %d)", err);
        }
        else
        {
            LOGI("config ssl ok");
            _b_init_done = true;
            return 0;
        }
    }

    // failed
    deinit_ssl();
    return -1;
}

void ApiClient::SecClient::deinit_ssl()
{
    mbedtls_entropy_free(&_entropy_ctx);
    mbedtls_ctr_drbg_free(&_ctr_drbg);
    mbedtls_x509_crt_free(&_ca_cert);
    mbedtls_ssl_config_free(&_ssl_conf);
    mbedtls_ssl_free(&_ssl_ctx);
    _b_init_done = false;
}

int ApiClient::SecClient::connect_socket()
{
    IPAddress   address;
    const char *host = LICHESS_API_HOST;

    if (0 == _server_addr.sin_addr.s_addr)
    {
        if (!WiFi.hostByName(host, address))
        {
            LOGW("failed to resolve '%s'", host);
            return -1;
        }
        _server_addr.sin_addr.s_addr = address;
        _server_addr.sin_port        = htons(LICHESS_API_PORT);
        _server_addr.sin_family      = AF_INET;
        LOGI("'%s' = %s", host, address.toString().c_str());
    }


    if (_sock_fd > 0)
    {
        LOGD("reuse socket %d", _sock_fd);
        return _sock_fd;
    }

    _sock_fd = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (_sock_fd <= 0) {
        LOGW("failed opening socket (%d)", _sock_fd);
        return _sock_fd;
    }

    int flags = lwip_fcntl(_sock_fd, F_GETFL, 0 );
    lwip_fcntl(_sock_fd, F_SETFL, flags | O_NONBLOCK );

    fd_set fdset;
    struct timeval tv;
    FD_ZERO(&fdset);
    FD_SET(_sock_fd, &fdset);
    tv.tv_sec = LICHESS_API_TIMEOUT_MS / 1000;
    tv.tv_usec = (LICHESS_API_TIMEOUT_MS % 1000) * 1000;

    int         enable = 1;
    int         sockerr = 0;
    socklen_t   len = (socklen_t)sizeof(int);

    int res = lwip_connect(_sock_fd, (struct sockaddr*)&_server_addr, sizeof(_server_addr));
    if (res < 0 && (errno != EINPROGRESS))
    {
        LOGW("connect on fd %d, errno: %d, \"%s\"", _sock_fd, errno, strerror(errno));
    }
    else if ((res = select(_sock_fd + 1, nullptr, &fdset, nullptr, &tv)) < 0)
    {
        LOGW("select on fd %d, errno: %d, \"%s\"", _sock_fd, errno, strerror(errno));
    }
    else if (res == 0)
    {
        LOGW("select returned due to timeout %d ms for fd %d", LICHESS_API_TIMEOUT_MS, _sock_fd);
    }
    else if (getsockopt(_sock_fd, SOL_SOCKET, SO_ERROR, &sockerr, &len) < 0)
    {
        LOGW("getsockopt on fd %d, errno: %d, \"%s\"", _sock_fd, errno, strerror(errno));
    }
    else if (0 != sockerr)
    {
        LOGW("socket error on fd %d, errno: %d, \"%s\"", _sock_fd, sockerr, strerror(sockerr));
    }
    else if (lwip_setsockopt(_sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        LOGW("config SO_RCVTIMEO failed");
    }
    else if (lwip_setsockopt(_sock_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0)
    {
        LOGW("config SO_SNDTIMEO failed");
    }
    else if (lwip_setsockopt(_sock_fd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable)) < 0)
    {
        LOGW("config TCP_NODELAY failed");
    }
    else if (lwip_setsockopt(_sock_fd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable)) < 0)
    {
        LOGW("config SO_KEEPALIVE failed");
    }
    else
    {
        //LOGD("socket fd = %d", _sock_fd);
        return _sock_fd;
    }

    lwip_shutdown(_sock_fd, 2);
    lwip_close(_sock_fd);
    _sock_fd = -1;
    return -1;
}

int ApiClient::SecClient::connect_ssl()
{
    int     err = 0;

    mbedtls_ssl_set_bio(&_ssl_ctx, &_sock_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

    //LOGD("Performing the SSL/TLS handshake...");

    uint32_t handshake_timeout = millis() + LICHESS_API_TIMEOUT_MS;
    while (0 != (err = mbedtls_ssl_handshake(&_ssl_ctx)))
    {
        if ((MBEDTLS_ERR_SSL_WANT_READ != err) && (MBEDTLS_ERR_SSL_WANT_WRITE != err))
        {
            LOGW("mbedtls_ssl_handshake() failed (err = -0x%x)", -err);
            break;
        }
        else if (millis() > handshake_timeout)
        {
            LOGW("handshake timeout (err = -0x%x)", -err);
            break;
        }

        //LOGD("mbedtls_ssl_handshake() = -0x%x", -err);
        vTaskDelay(5);
    }

    if (0 == err)
    {
        //LOGD("Verifying peer X.509 certificate...");
        if (0 != (err = mbedtls_ssl_get_verify_result(&_ssl_ctx)))
        {
            LOGW("mbedtls_ssl_get_verify_result() failed (err = -0x%x)", -err);
        }
        else
        {
            //LOGD("verified (%s)", mbedtls_ssl_get_ciphersuite(&_ssl_ctx));
        }
    }
    else
    {
        LOGW("handshake failed (err = -0x%x)", -err);
        //stop();
    }

    return err;
}

int ApiClient::SecClient::send_ssl(const uint8_t *data, size_t len)
{
    int      bytes_written = 0;
    uint32_t write_timeout = millis() + LICHESS_API_TIMEOUT_MS;

    //LOGD("send %d bytes:\r\n%.*s", len, len, data);

    do {
        int ret = mbedtls_ssl_write(&_ssl_ctx, data, len - bytes_written);
        if (ret > 0)
        {
            bytes_written   += ret;
            data            += ret;
        }
        else if (ret == 0)
        {
            if (millis() > write_timeout) {
                LOGW("write timed out");
                break;
            }
        }
        else if ((MBEDTLS_ERR_SSL_WANT_READ != ret) && (MBEDTLS_ERR_SSL_WANT_WRITE != ret))
        {
            LOGW("mbedtls_ssl_write() failed (err = -0x%x", -ret);
            return -1;
        }
        vTaskDelay(2);
    } while (bytes_written < len);

    //LOGD("written %d/%d", bytes_written, len);
    return bytes_written;
}

ApiClient::ApiClient()
{
    memset(_auth, 0, sizeof(_auth));
    memset(_uri, 0, sizeof(_uri));
}

bool ApiClient::begin(const char *endpoint)
{
    if (0 == _auth[0])
    {
        char token[64];
        if (!get_token(token, sizeof(token) - 1))
        {
            LOGW("unknown access");
            return false;
        }
        snprintf(_auth, sizeof(_auth) - 1, "Bearer %s", token);
        LOGI("auth: %s", token);
    }

    strncpy(_uri, endpoint, sizeof(_uri));
    LOGD("%s", _uri);

    return true;
}

bool ApiClient::connect(void)
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
            LOGW("failed connect to %s (%d/%d)", LICHESS_API_HOST, num_connect_errors, max_connect_errors);
            if (num_connect_errors >= max_connect_errors)
            {
                LOGW("max connect errors. reconnect wifi.");
                end(true);
                wifi::disconnect();
                num_connect_errors = 0;
            }
        }
        else
        {
            //LOGD("connected to %s", LICHESS_API_HOST);
            num_connect_errors = 0;
            b_status = _secClient.connected();
        }
    }

    LOGD("%s %lums (fd %d heap %lu)", b_status ? "ok" : "failed", millis() - ms_start, _secClient.fd(), ESP.getFreeHeap());
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
        _secClient.stop(true);
    }
    memset(_uri, 0, sizeof(_uri));
    _returnCode = 0;
    _size = -1;
}

bool ApiClient::sendHeader(const char *type, size_t content_length)
{
    char    header_buf[256];
    size_t  header_len = 0;

    if (connected())
    {
  #define ADD_HEADER(hdr, ...)    header_len += snprintf(header_buf + header_len, sizeof(header_buf) - header_len, hdr "\r\n", ## __VA_ARGS__)

        ADD_HEADER("%s %s HTTP/1.1", type, _uri);
        ADD_HEADER("Host: %s", LICHESS_API_HOST);
        ADD_HEADER("Connection: %s", "keep-alive");
        //ADD_HEADER("User-Agent: %s", "esp32-chess-board");
        ADD_HEADER("Authorization: %s", _auth);
        if (content_length) {
            ADD_HEADER("Content-Type: %s", "application/x-www-form-urlencoded");
            ADD_HEADER("Content-Length: %ld", content_length);
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
            delay(10);
        }
    }

    return HTTPC_ERROR_CONNECTION_LOST;
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


/*
 * Private Functions
 */

bool api_get(const char *endpoint, cJSON **response, bool b_debug)
{
    bool b_status = false;

    if (!main_client.begin(endpoint))
    {
        LOGW("begin(%s) failed", endpoint);
    }
    else
    {
        int httpCode = main_client.sendRequest("GET");
        if (httpCode > 0)
        {
            memset(response_buff, 0, sizeof(response_buff));
            int response_len = main_client.readline(response_buff, sizeof(response_buff) - 1, 0);

            if (b_debug) {
                LOGD("payload (%d):\r\n%s", response_len, response_buff);
            }

            if ((HTTP_CODE_OK == httpCode) || (HTTP_CODE_BAD_REQUEST == httpCode))
            {
                if (response_len > 0)
                {
                    if ((NULL != response) && (NULL == (*response = cJSON_Parse(response_buff))))
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
            main_client.end(false);
        }
    }

    return b_status;
}

bool api_post(const char *endpoint, const uint8_t *payload, size_t payload_len, cJSON **response, bool b_debug)
{
    bool b_status = false;

    if (!main_client.begin(endpoint))
    {
        LOGW("begin(%s) failed", endpoint);
    }
    else
    {
        int httpCode = main_client.sendRequest("POST", payload, payload_len);
        if (httpCode > 0)
        {
            memset(response_buff, 0, sizeof(response_buff));
            int response_len = main_client.readline(response_buff, sizeof(response_buff) - 1, 0);

            if (b_debug) {
                LOGD("response (%d): %s", response_len, response_buff);
            }

            //if ((HTTP_CODE_OK == httpCode) || (HTTP_CODE_CREATED == httpCode) || (HTTP_CODE_MOVED_PERMANENTLY == httpCode))
            if ((HTTP_CODE_OK == httpCode) || (HTTP_CODE_BAD_REQUEST == httpCode))
            {
                if (response_len > 0)
                {
                    if ((NULL != response) && (NULL == (*response = cJSON_Parse(response_buff))))
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
                LOGW("POST %s failed (%d)", endpoint, httpCode);
            }
        }
        else
        {
            LOGW("error code %d", httpCode);
            main_client.end(false);
        }

    }

    return b_status;
}

} // namespace lichess
