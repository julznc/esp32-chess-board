
#include "globals.h"
#include "lichess_client.h"


namespace lichess
{

static ApiClient        stream_client;

bool init()
{
    LOGD("%s()", __func__);

    SecClient::lib_init();

    return true;
}

void loop()
{
    delayms(10);
}

} // namespace lichess
