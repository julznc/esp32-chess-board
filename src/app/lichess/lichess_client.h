
#pragma once

#include "lichess/apiclient.h"


namespace lichess
{

bool init();
void loop();

bool get_token(const char **token);
bool set_token(const char *token);

} // namespace lichess
