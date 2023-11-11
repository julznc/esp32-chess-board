
#pragma once

#include "lichess/apiclient.h"


namespace lichess
{

bool init();
void loop();

bool get_username(const char **name);
bool get_token(const char **token);
bool set_token(const char *token);
bool get_game_options(const char **opponent, uint16_t *clock_limit, uint8_t *clock_increment, uint8_t *rated);
bool set_game_options(const char *opponent, uint16_t clock_limit, uint8_t clock_increment, uint8_t rated);

} // namespace lichess
