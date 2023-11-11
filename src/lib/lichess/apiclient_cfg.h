
#pragma once


#define LICHESS_API_HOST                "lichess.org"
#define LICHESS_API_PORT                "443"   // default https port
#define LICHESS_API_TIMEOUT_MS          (8000)  // 8-second timeout

#define CHALLENGE_DEFAULT_OPPONENT          PLAYER_CUSTOM
#define CHALLENGE_DEFAULT_OPPONENT_NAME     "maia5"
#define CHALLENGE_DEFAULT_LIMIT             (15U * 60)
#define CHALLENGE_DEFAULT_INCREMENT         (10)

extern const uint8_t LICHESS_ORG_PEM[]  asm("_binary_lichess_org_pem_start");
