
#include "globals.h"
#include "wifi/wifi_setup.h"

#include "apiclient_cfg.h"
#include "secclient.h"


namespace lichess
{

static const char *API_HOST = LICHESS_API_HOST;

SecClient::SecClient() : _sock_fd(-1), _b_init_done(false), _b_connected(false)
{
    memset(&_server_addr, 0, sizeof(_server_addr));
};

SecClient::~SecClient()
{
    stop(true);
}

void SecClient::lib_init()
{
    // use default memory allocation - can also handle PSRAM up to 4MB
    mbedtls_platform_set_calloc_free(calloc, free);
}

int SecClient::connect()
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
        // handshake failed
    }
    else
    {
        _b_connected = true;
    }

    return _b_connected;
}

uint8_t SecClient::connected()
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

void SecClient::stop(bool b_deinit)
{
    _b_connected = false;

    if (b_deinit) {
        memset(&_server_addr, 0, sizeof(_server_addr));
        //LOGD("free(ssl)");
        deinit_ssl();
    } else if (_b_init_done) {
        //LOGD("session reset");
        //mbedtls_ssl_close_notify(&_ssl_ctx);
        mbedtls_ssl_session_reset(&_ssl_ctx);
    }

    if (_sock_fd > 0) {
        lwip_close(_sock_fd);
        _sock_fd = -1;
    }
}

size_t SecClient::write(const uint8_t *buf, size_t size)
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

int SecClient::available()
{
    return connected() ? mbedtls_ssl_get_bytes_avail(&_ssl_ctx) : 0;
}

int SecClient::read(uint8_t *buf, size_t size)
{
    return _b_connected ? mbedtls_ssl_read(&_ssl_ctx, buf, size) : 0;
}

int SecClient::readline(char *buf, size_t size, uint32_t timeout)
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

void SecClient::flush()
{
    // to do
    //mbedtls_ssl_flush_output(&_ssl_ctx);
}

int SecClient::init_ssl()
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
        LOGW("mbedtls_ctr_drbg_seed() failed (err = -0x%x)", -err);
    }
    else if (0 != (err = mbedtls_x509_crt_parse(&_ca_cert, LICHESS_ORG_PEM, strlen((const char *)LICHESS_ORG_PEM) + 1)))
    {
        //LOGD("pem %s", (const char *)LICHESS_ORG_PEM);
        LOGW("mbedtls_x509_crt_parse() failed (err = -0x%x)", -err);
    }
    else if (0 != (err = mbedtls_ssl_set_hostname(&_ssl_ctx, API_HOST)))
    {
        LOGW("mbedtls_ssl_set_hostname() failed (err = -0x%x)", -err);
    }
    else if (0 != (err = mbedtls_ssl_config_defaults(&_ssl_conf,
                                                    MBEDTLS_SSL_IS_CLIENT,
                                                    MBEDTLS_SSL_TRANSPORT_STREAM,
                                                    MBEDTLS_SSL_PRESET_DEFAULT)))
    {
        LOGW("mbedtls_ssl_config_defaults() failed (err = -0x%x)", -err);
    }
    else
    {

      #if 1
        mbedtls_ssl_conf_authmode(&_ssl_conf, MBEDTLS_SSL_VERIFY_REQUIRED);
      #else
        mbedtls_ssl_conf_authmode(&_ssl_conf, MBEDTLS_SSL_VERIFY_NONE);
      #endif
        mbedtls_ssl_conf_ca_chain(&_ssl_conf, &_ca_cert, NULL);
        mbedtls_ssl_conf_rng(&_ssl_conf, mbedtls_ctr_drbg_random, &_ctr_drbg);

        if (0 != (err = mbedtls_ssl_setup(&_ssl_ctx, &_ssl_conf)))
        {
            LOGW("mbedtls_ssl_setup() failed (err = -0x%x)", -err);
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

void SecClient::deinit_ssl()
{
    if (_b_init_done)
    {
        mbedtls_entropy_free(&_entropy_ctx);
        mbedtls_ctr_drbg_free(&_ctr_drbg);
        mbedtls_x509_crt_free(&_ca_cert);
        mbedtls_ssl_config_free(&_ssl_conf);
        mbedtls_ssl_free(&_ssl_ctx);

        _b_init_done = false;
    }
}

int SecClient::connect_socket()
{
    int res = 0;

    if (_sock_fd > 0)
    {
        LOGD("reuse socket %d", _sock_fd);
        return _sock_fd;
    }

    if (0 == _server_addr.sa_len)
    {
        struct  addrinfo hints;
        struct  addrinfo *addr_list;

        uint32_t wifi_timeout = millis() + LICHESS_API_TIMEOUT_MS;
        while (!wifi::connected()) {
            if (millis() > wifi_timeout) {
                LOGD("wifi not connected");
                return MBEDTLS_ERR_NET_CONNECT_FAILED;
            }
            delayms(500);
        }

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC; // ipv4 or ipv6
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        if (0 != lwip_getaddrinfo(API_HOST, LICHESS_API_PORT, &hints, &addr_list))
        {
            LOGW("getaddrinfo() failed");
            return MBEDTLS_ERR_NET_UNKNOWN_HOST;
        }

        for (struct addrinfo *cur = addr_list; cur != NULL; cur = cur->ai_next)
        {
            _sock_fd = lwip_socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
            if (_sock_fd <= 0) {
                LOGW("failed opening socket (%d)", _sock_fd);
                res = MBEDTLS_ERR_NET_SOCKET_FAILED;
                continue;
            }

            if (0 != (res = lwip_connect(_sock_fd, cur->ai_addr, cur->ai_addrlen)))
            {
                LOGW("connect on fd %d, errno: %d, \"%s\"", _sock_fd, errno, strerror(errno));
                lwip_close(_sock_fd);
                _sock_fd = -1;
                res = MBEDTLS_ERR_NET_CONNECT_FAILED;
            }
            else // success
            {
                memcpy(&_server_addr, cur->ai_addr, sizeof(_server_addr));
                _server_addr.sa_len     = cur->ai_addrlen;
                _server_addr.sa_family  = cur->ai_family;

                char    str_addr[INET6_ADDRSTRLEN];
                struct  sockaddr_in *in_addr = (struct sockaddr_in *)&_server_addr;
                lwip_inet_ntop(_server_addr.sa_family, &in_addr->sin_addr, str_addr, sizeof(str_addr));
                LOGI("\"%s\" = %s", API_HOST, str_addr);
                break;
            }
        }

        freeaddrinfo(addr_list);
    }
    else if ((_sock_fd = lwip_socket(_server_addr.sa_family, SOCK_STREAM, IPPROTO_TCP)) <= 0)
    {
        LOGW("failed opening socket (%d)", _sock_fd);
    }
    else if (0 != (res = lwip_connect(_sock_fd, &_server_addr, _server_addr.sa_len)))
    {
        LOGW("connect on fd %d, errno: %d, \"%s\"", _sock_fd, errno, strerror(errno));
        lwip_close(_sock_fd);
        _sock_fd = -1;
    }

    if (_sock_fd < 0)
    {
        LOGW("failed");
        return -1;
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

    if ((res = select(_sock_fd + 1, nullptr, &fdset, nullptr, &tv)) < 0)
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

    lwip_close(_sock_fd);
    _sock_fd = -1;
    return _sock_fd;
}

int SecClient::connect_ssl()
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
        if ((MBEDTLS_ERR_SSL_WANT_READ != err) && (MBEDTLS_ERR_SSL_WANT_WRITE != err) && (MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS != err))
        {
            stop(true);
        }
        else
        {
            stop(false);
        }
    }

    return err;
}

int SecClient::send_ssl(const uint8_t *data, size_t len)
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

} // namespace lichess
