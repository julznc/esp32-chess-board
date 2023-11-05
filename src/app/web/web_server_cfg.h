
#pragma once

//#define WEB_SERVER_BASIC_AUTH

#ifdef WEB_SERVER_BASIC_AUTH
  #define WEB_SERVER_AUTH_USERNAME          "admin"
  #define WEB_SERVER_AUTH_PASSWORD          "12345678"
  #define WEB_SERVER_AUTH_REALM             "login ka muna"
#endif // WEB_SERVER_BASIC_AUTH
