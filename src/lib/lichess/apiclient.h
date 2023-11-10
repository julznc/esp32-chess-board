
#pragma once

#include <cJSON.h>

#include "apiclient_cfg.h"
#include "secclient.h"


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
public:
    ApiClient();
    static void lib_init(const char *token);

    bool begin(const char *endpoint);
    bool connect();
    bool connected() { return _secClient.connected(); };
    void end(bool b_stop /*close ssl connection*/);
    int sendRequest(const char *type, const uint8_t *payload=NULL, size_t size=0);
    int readline(char *buf, size_t size, uint32_t timeout);

    bool api_get(const char *endpoint, cJSON **response, bool b_debug=true);

protected:
    bool sendHeader(const char *type, size_t content_length=0);
    int handleHeaderResponse();

private:
    SecClient   _secClient;
    char        _rsp_buf[2048];
    char        _auth[80];
    char        _uri[128];
    int         _returnCode = 0;
    int         _size = -1;

    static const char *pc_token;
    static int  num_connect_errors;
};

} // namespace lichess
