
#pragma once

#include "secclient.h"

namespace lichess
{

class ApiClient
{
public:
    ApiClient();

private:
    SecClient   _secClient;
};

} // namespace lichess
