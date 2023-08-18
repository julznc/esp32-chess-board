#ifndef __LICHESS_API_H__
#define __LICHESS_API_H__

#include <ArduinoJson.h>
#include <mbedtls/platform.h>
#include <esp_http_client.h>

#include "lichess_api.h"
#include "lichess_board.h"
#include "lichess_challenges.h"


namespace lichess
{

class APIClient
{
public:
    APIClient();
    ~APIClient();

    static esp_err_t event_handler(esp_http_client_event_t *evt);

    bool begin(const char *endpoint);
    void end();

    int GET(bool b_retry);
    int POST(const char *data, int len, bool b_retry);
    bool startStream(const char *endpoint);
    String readLine(void);

    bool connected() const { return _connected; }
    const String &getEndpoint(void) const { return _endpoint; }
    const char *get_content() const { return _rsp_buf; }
    size_t get_content_length() const { return _rsp_len; }

protected:
    esp_http_client_handle_t    _client;
    esp_http_client_config_t    _config;
    String                      _endpoint;
    bool                        _connected;
    char                        _auth[64];
    char                        _rsp_buf[2048];
    size_t                      _rsp_len;

    esp_err_t perform(const char *buffer, int len);
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


/*
 * Private Function Prototypes
 */
bool api_get(const char *endpoint, DynamicJsonDocument &response, bool b_debug=false);
bool api_post(const char *endpoint, String payload, DynamicJsonDocument &response, bool b_debug=false);

} // namespace lichess

#endif // __LICHESS_API_H__
