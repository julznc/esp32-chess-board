
#pragma once


#define LICHESS_API_HOST                "lichess.org"
#define LICHESS_API_PORT                "443"   // default https port
#define LICHESS_API_TIMEOUT_MS          (8000)  // 8-second timeout


extern const uint8_t LICHESS_ORG_PEM_START[] asm("_binary_lichess_org_pem_start");
extern const uint8_t LICHESS_ORG_PEM_END[]   asm("_binary_lichess_org_pem_end");
