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
        void stop();
        uint8_t connected();
    };
public:
    ApiClient();

    bool begin(const char *endpoint);
    bool connect(void);
    bool connected();
    void end(bool b_stop /*close ssl connection*/);
    int sendRequest(const char *type, uint8_t *payload=NULL, size_t size=0);
    bool startStream(const char *endpoint);
    String readLine(void);

    const String &getEndpoint(void) const { return _uri; }

private:
    String      _auth;
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
bool set_game_options(String &opponent, uint16_t u16_clock_limit, uint8_t u8_clock_increment);
bool get_game_options(String &opponent, uint16_t &u16_clock_limit, uint8_t &u8_clock_increment);


/*
 * Private Function Prototypes
 */
bool api_get(const char *endpoint, DynamicJsonDocument &response, bool b_debug=false);
bool api_post(const char *endpoint, String payload, DynamicJsonDocument &response, bool b_debug=false);

} // namespace lichess

#endif // __LICHESS_API_H__
