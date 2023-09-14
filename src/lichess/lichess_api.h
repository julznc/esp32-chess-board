#ifndef __LICHESS_API_H__
#define __LICHESS_API_H__

#include <lwip/sockets.h>
#include <mbedtls/platform.h>
#include <mbedtls/net.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ssl.h>

#include <ArduinoJson.h>
#include <WiFi.h>

#include "lichess_api.h"
#include "lichess_board.h"
#include "lichess_challenges.h"


namespace lichess
{

// copied from framework-arduinoespressif32/libraries/HTTPClient/src/HTTPClient.h
typedef enum {
    HTTP_CODE_OK = 200,
    HTTP_CODE_CREATED = 201,
    HTTP_CODE_ACCEPTED = 202,
    HTTP_CODE_MOVED_PERMANENTLY = 301,
    HTTP_CODE_BAD_REQUEST = 400,
    HTTP_CODE_NOT_FOUND = 404,
    HTTP_CODE_TOO_MANY_REQUESTS = 429,
    HTTP_CODE_INTERNAL_SERVER_ERROR = 500,
} http_codes_et;

typedef enum {
    HTTPC_ERROR_CONNECTION_REFUSED  = (-1),
    HTTPC_ERROR_SEND_HEADER_FAILED  = (-2),
    HTTPC_ERROR_SEND_PAYLOAD_FAILED = (-3),
    HTTPC_ERROR_NOT_CONNECTED       = (-4),
    HTTPC_ERROR_CONNECTION_LOST     = (-5),
    HTTPC_ERROR_NO_STREAM           = (-6),
    HTTPC_ERROR_NO_HTTP_SERVER      = (-7),
    HTTPC_ERROR_TOO_LESS_RAM        = (-8),
    HTTPC_ERROR_ENCODING            = (-9),
    HTTPC_ERROR_STREAM_WRITE        = (-10),
    HTTPC_ERROR_READ_TIMEOUT        = (-11)
} http_client_error_et;

class ApiClient
{
    class SecClient
    {
    public:
        SecClient();
        ~SecClient();
        int connect();
        uint8_t connected();
        void stop();
        size_t write(const uint8_t *buf, size_t size);
        int available();
        int read(uint8_t *buf, size_t size);
        int readline(char *buf, size_t size);
        void flush();
        int fd() const { return _sock_fd; }

    private:
        int init_ssl();
        void deinit_ssl();

        int connect_socket();
        int connect_ssl();
        int send_ssl(const uint8_t *data, size_t len);

    protected:
        struct sockaddr_in          _server_addr;
        int                         _sock_fd;
        bool                        _b_init_done;
        bool                        _b_connected;

        mbedtls_ssl_context         _ssl_ctx;
        mbedtls_ssl_config          _ssl_conf;
        mbedtls_x509_crt            _ca_cert;
        mbedtls_ctr_drbg_context    _ctr_drbg;
        mbedtls_entropy_context     _entropy_ctx;
    };
public:
    ApiClient();

    bool begin(const char *endpoint);
    bool connect(void);
    bool connected();
    void end(bool b_stop /*close ssl connection*/);
    int sendRequest(const char *type, uint8_t *payload=NULL, size_t size=0);
    bool startStream(const char *endpoint);
    int readline(char *buf, size_t size);

    const String &getEndpoint(void) const { return _uri; }

protected:
    bool sendHeader(const char *type, size_t content_length=0);
    int handleHeaderResponse();

private:
    String      _auth;
    String      _uri;
    int         _returnCode = 0;
    int         _size = -1;
    SecClient   _secClient;
    static int  num_connect_errors;
};

typedef enum
{
    EVENT_UNKNOWN,
    EVENT_GAME_START,
    EVENT_GAME_FINNISH,
    EVENT_CHALLENGE_INCOMING,
    EVENT_CHALLENGE_CANCELED,
    EVENT_CHALLENGE_DECLINED,
} event_type_et;


/*
 * Public Function Prototypes
 */
void init(void);
bool get_username(String &name);
size_t set_token(const char *token);
bool get_token(String &token);
bool set_game_options(String &opponent, uint16_t u16_clock_limit, uint8_t u8_clock_increment, bool b_rated);
bool get_game_options(String &opponent, uint16_t &u16_clock_limit, uint8_t &u8_clock_increment, bool &b_rated);


/*
 * Private Function Prototypes
 */
bool api_get(const char *endpoint, DynamicJsonDocument &response, bool b_debug=false);
bool api_post(const char *endpoint, String payload, DynamicJsonDocument &response, bool b_debug=false);

} // namespace lichess

#endif // __LICHESS_API_H__
