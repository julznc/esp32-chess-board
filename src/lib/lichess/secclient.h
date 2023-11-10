
#pragma once

#include <lwip/netdb.h> // sockets & getaddrinfo
#include <mbedtls/platform.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/debug.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ssl.h>


namespace lichess
{

class SecClient
{
public:
    SecClient();
    ~SecClient();
    static void lib_init();
    int connect();
    uint8_t connected();
    void stop(bool b_deinit);
    size_t write(const uint8_t *buf, size_t size);
    int available();
    int read(uint8_t *buf, size_t size);
    int readline(char *buf, size_t size, uint32_t timeout);
    void flush();
    int fd() const { return _sock_fd; }

private:
    int init_ssl();
    void deinit_ssl();

    int connect_socket();
    int connect_ssl();
    int send_ssl(const uint8_t *data, size_t len);

protected:
    struct sockaddr             _server_addr;
    int                         _sock_fd;
    bool                        _b_init_done;
    bool                        _b_connected;

    mbedtls_ssl_context         _ssl_ctx;
    mbedtls_ssl_config          _ssl_conf;
    mbedtls_x509_crt            _ca_cert;
    mbedtls_ctr_drbg_context    _ctr_drbg;
    mbedtls_entropy_context     _entropy_ctx;
};

} // namespace lichess
