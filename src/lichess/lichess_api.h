#ifndef __LICHESS_API_H__
#define __LICHESS_API_H__

#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "lichess_api.h"
#include "lichess_board.h"
#include "lichess_challenges.h"


namespace lichess
{

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
