
#pragma once

#include <cJSON.h>

#include "apiclient_cfg.h"
#include "secclient.h"
#include "board_api.h"
#include "challenges_api.h"



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



typedef enum {
    EVENT_UNKNOWN,
    EVENT_GAME_START,
    EVENT_GAME_FINNISH,
    EVENT_CHALLENGE_INCOMING,
    EVENT_CHALLENGE_CANCELED,
    EVENT_CHALLENGE_DECLINED,
} event_type_et;


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
    bool startStream(const char *endpoint);
    int readline(char *buf, size_t size, uint32_t timeout);
    const char *getEndpoint() const { return _uri; }

    // rest
    bool api_get(const char *endpoint, cJSON **response, bool b_debug=true);
    bool api_post(const char *endpoint, const uint8_t *payload=NULL, size_t payload_len=0, cJSON **response=NULL, bool b_debug=true);

    // board-api
    bool game_move(const char *game_id, const char *move_uci, bool draw = false);
    bool game_abort(const char *game_id);
    bool game_resign(const char *game_id);
    bool create_seek(const challenge_st *ps_challenge);
    bool handle_draw(const char *game_id, bool b_accept);
    bool handle_takeback(const char *game_id, bool b_accept);

    // challenges-api
    bool accept_challenge(const char *challenge_id);
    bool decline_challenge(const char *challenge_id, const char *reason=NULL);
    bool create_challenge(const challenge_st *ps_challenge, const char *fen);
    bool cancel_challenge(const char *challenge_id);

protected:
    bool sendHeader(const char *type, size_t content_length=0);
    int handleHeaderResponse();

private:
    SecClient   _secClient;
    char        _rsp_buf[2048]; // response | payload buffer
    char        _auth[80];
    char        _uri[128];      // endpoint buffer
    int         _returnCode = 0;
    int         _size = -1;

    static const char *pc_token;
    static int  num_connect_errors;
};

} // namespace lichess
