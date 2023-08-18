#ifndef __LICHESS_API_H__
#define __LICHESS_API_H__

#include <ArduinoJson.h>
#include <HTTPClient.h>

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
    int GET();
    int POST(const char *data, int len);

    const char *get_content() const { return _rsp_buf; }
    size_t get_content_length() const { return _rsp_len; }

protected:
    esp_http_client_handle_t    _client;
    esp_http_client_config_t    _config;
    char                        _auth[64];
    char                        _rsp_buf[2048];
    size_t                      _rsp_len;
};

// fix me:
// https://stackoverflow.com/questions/72242061/esp-32-http-get-is-so-slow
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_client.html#persistent-connections
class ApiClient : public HTTPClient
{
    class SecClient : public WiFiClientSecure
    {
    public:
        SecClient();
        int connect(const char *host, uint16_t port, int32_t timeout);
        uint8_t connected();
        int available();
        int read(uint8_t *buf, size_t size);
    };
public:
    ApiClient();

    bool begin(const char *endpoint);
    bool connect(void);
    void end(bool b_stop /*close ssl connection*/);
    int sendRequest(const char *type, uint8_t *payload=NULL, size_t size=0);
    int handleHeaderResponse();
    bool startStream(const char *endpoint);
    String readLine(void);

    const String &getEndpoint(void) const { return _uri; }

private:
    String      _auth;
    SecClient   _secClient;
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
