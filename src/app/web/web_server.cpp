
#include <esp_http_server.h>
#include <esp_tls_crypto.h>

#include "globals.h"
#include "chess/chess.h"
#include "lichess/lichess_client.h"

#include "wifi/wifi_setup.h"
#include "web_server_cfg.h"
#include "web_server.h"


namespace web::server
{

static char recv_buf[1024];
static char send_buf[256];


#ifdef WEB_SERVER_BASIC_AUTH
static char *http_auth_basic(const char *username, const char *password)
{
    size_t out;
    char *user_info = NULL;
    char *digest = NULL;
    size_t n = 0;
    if (asprintf(&user_info, "%s:%s", username, password) < 0) {
        return NULL;
    }

    if (!user_info) {
        LOGE("No enough memory for user information");
        return NULL;
    }
    esp_crypto_base64_encode(NULL, 0, &n, (const unsigned char *)user_info, strlen(user_info));

    /* 6: The length of the "Basic " string
     * n: Number of bytes for a base64 encode format
     * 1: Number of bytes for a reserved which be used to fill zero
    */
    digest = (char *)calloc(1, 6 + n + 1);
    if (digest) {
        strcpy(digest, "Basic ");
        esp_crypto_base64_encode((unsigned char *)digest + 6, n, &out, (const unsigned char *)user_info, strlen(user_info));
    }
    free(user_info);
    return digest;
}

static bool authenticate(httpd_req_t *req)
{
    static const char *hdr = "Authorization";
    static const char *digest = NULL;
    char    val_buf[64] = {0, };
    size_t  val_len = 0;
    bool    b_status = false;

    if (NULL == digest)
    {
        digest = http_auth_basic(WEB_SERVER_AUTH_USERNAME, WEB_SERVER_AUTH_PASSWORD);
    }

    if (NULL == digest)
    {
        LOGE("unknown auth");
    }
    else if ((val_len = httpd_req_get_hdr_value_len(req, hdr)) < 1)
    {
        LOGW("not authenticated");
    }
    else if (val_len >= sizeof(val_buf))
    {
        LOGE("not enough buffer");
    }
    else if (ESP_OK != httpd_req_get_hdr_value_str(req, hdr, val_buf, val_len + 1))
    {
        LOGE("no auth value");
    }
    else if (0 != strncmp(digest, val_buf, val_len))
    {
        LOGE("wrong auth: %s", val_buf);
    }
    else
    {
        //LOGD("auth: %s", val_buf);
        b_status = true;
    }

    return b_status;
}

static esp_err_t request_auth(httpd_req_t *req)
{
    httpd_resp_set_status(req, "401 UNAUTHORIZED"); // HTTP Response 401
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"" WEB_SERVER_AUTH_REALM "\"");
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}
#endif

static void form_urldecode(char *in_out, int len)
{
    if (len > 1)
    {
        // url decode
        const char *encoded = in_out;
        char *decoded = in_out;
        char temp[8] = "0x00";
        int i = 0;
        while (i++ < len) {
            if ((*encoded == '%') && (i < len)) {
                encoded++; // '%'
                temp[2] = *encoded++;
                temp[3] = *encoded++;
                *decoded++ = strtol(temp, NULL, 16);
                i += 2;
            } else if (*encoded == '+') {
                encoded++; // '+'
                *decoded++ = ' ';
            } else {
                *decoded++ = *encoded++;
            }
        }
        *decoded++ = '\0';
        //LOGD("decoded: %s", in_out);
    }
}

/* serve index.html */
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
esp_err_t get_indexhtml_handler(httpd_req_t *req)
{
#ifdef WEB_SERVER_BASIC_AUTH
    if (!authenticate(req)) {
        return request_auth(req);
    }
#endif
    httpd_resp_send(req, (const char *) index_html_start, index_html_end - index_html_start);
    return ESP_OK;
}

/* serve favicon */
extern const uint8_t favicon_svg_start[] asm("_binary_favicon_svg_start");
extern const uint8_t favicon_svg_end[] asm("_binary_favicon_svg_end");
esp_err_t get_favicon_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "image/svg+xml");
    httpd_resp_send(req, (const char *) favicon_svg_start, favicon_svg_end - favicon_svg_start);
    return ESP_OK;
}

/* send version info */
esp_err_t get_version_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, K_APP_VERSION);
    return ESP_OK;
}

/* send lichess username */
esp_err_t get_username_handler(httpd_req_t *req)
{
    const char *username = "lichess";
    lichess::get_username(&username);
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, username);
    return ESP_OK;
}

/* send current PGN */
esp_err_t get_pgn_handler(httpd_req_t *req)
{
    const char *pgn = NULL;
    chess::get_pgn(&pgn);
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, pgn ? pgn : "...");
    return ESP_OK;
}

/* send lichess game settings */
esp_err_t get_gamecfg_handler(httpd_req_t *req)
{
    const char *opponent = "..";
    uint16_t    u16_limit = 0;
    uint8_t     u8_increment = 0;
    uint8_t     b_rated = false;

    lichess::get_game_options(&opponent, &u16_limit, &u8_increment, &b_rated);

    snprintf(send_buf, sizeof(send_buf) - 1,
            "{\"opponent\": \"%s\", \"mode\": \"%s\", \"limit\":%u, \"increment\": %u}",
            opponent, b_rated ? "rated" : "casual", u16_limit / 60, u8_increment);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, send_buf);
    return ESP_OK;
}

esp_err_t post_gamecfg_handler(httpd_req_t *req)
{
    size_t recv_len = req->content_len;
    if (recv_len >= sizeof(recv_buf)) {
        recv_len = sizeof(recv_buf) - 1;
    }

    memset(recv_buf, 0, sizeof(recv_buf));
    int len = httpd_req_recv(req, recv_buf, recv_len);
    //LOGD("(%d/%u) %.*s", len, recv_len, len, recv_buf);

    form_urldecode(recv_buf, len);

    //clock-limit=15&clock-increment=10&opponent=&mode=casual
    char *limit     = strstr(recv_buf, "clock-limit=");
    char *increment = strstr(recv_buf, "clock-increment=");
    char *opponent  = strstr(recv_buf, "opponent=");
    char *mode      = strstr(recv_buf, "mode=");

    httpd_resp_set_type(req, "text/plain");

    if ((NULL == limit) || (NULL == increment) || (NULL == opponent) || (NULL == mode))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "incomplete info");
    }
    else
    {
        const char *name = strtok(strchr(opponent, '=') + 1, "&");
        if (name == mode) { // if empty name
            name = "";
        }
        limit     = strtok(strchr(limit, '=') + 1, "&");
        increment = strtok(strchr(increment, '=') + 1, "&");
        mode      = strtok(strchr(mode, '=') + 1, "&");

        bool b_status = lichess::set_game_options(name,
                                atoi(limit) * 60, atoi(increment),
                                0 == strcmp(mode, "rated"));
        httpd_resp_sendstr(req, b_status ? "success" : "failed");
    }

    return ESP_OK;
}

esp_err_t post_apitoken_handler(httpd_req_t *req)
{
    size_t recv_len = req->content_len;
    if (recv_len >= sizeof(recv_buf)) {
        recv_len = sizeof(recv_buf) - 1;
    }

    memset(recv_buf, 0, sizeof(recv_buf));
    int len = httpd_req_recv(req, recv_buf, recv_len);
    //LOGD("(%d/%u) %.*s", len, recv_len, len, recv_buf);

    form_urldecode(recv_buf, len);
    httpd_resp_set_type(req, "text/plain");

    char *token = strstr(recv_buf, "token=");

    if (NULL == token)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty");
    }
    else
    {
        token = strtok(strchr(token, '=') + 1, "&");
        bool b_status = lichess::set_token(token);
        httpd_resp_sendstr(req, b_status ? "success" : "failed");
    }

    return ESP_OK;
}

/* send wifi credentials */
esp_err_t get_wificfg_handler(httpd_req_t *req)
{
    const char *ssid = "";
    const char *passwd = "";

    (void)wifi::get_credentials(&ssid, &passwd);
    snprintf(send_buf, sizeof(send_buf) - 1, "{\"ssid\": \"%s\", \"passwd\":\"%s\"}", ssid, passwd);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, send_buf);
    return ESP_OK;
}

esp_err_t post_wificfg_handler(httpd_req_t *req)
{
    size_t recv_len = req->content_len;
    if (recv_len >= sizeof(recv_buf)) {
        recv_len = sizeof(recv_buf) - 1;
    }

    memset(recv_buf, 0, sizeof(recv_buf));
    int len = httpd_req_recv(req, recv_buf, recv_len);
    //LOGD("(%d/%u) %.*s", len, recv_len, len, recv_buf);

    form_urldecode(recv_buf, len);

    char *ssid   = strstr(recv_buf, "ssid=");
    char *passwd = strstr(recv_buf, "passwd=");

    httpd_resp_set_type(req, "text/plain");

    if ((NULL == ssid) || (NULL == passwd))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "incomplete info");
    }
    else
    {
        ssid   = strtok(strchr(ssid, '=') + 1, "&");
        passwd = strtok(strchr(passwd, '=') + 1, "&");

        bool b_status = wifi::set_credentials(ssid, passwd);
        httpd_resp_sendstr(req, b_status ? "success" : "failed");
    }

    return ESP_OK;
}

/* handle OTA file upload */
esp_err_t post_update_handler(httpd_req_t *req)
{
    esp_ota_handle_t    ota_handle = 0;
    int                 remaining = req->content_len;
    LOGD("update file size %d bytes", remaining);

    const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
    ESP_ERROR_CHECK(esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle));

    while (remaining > 0)
    {
        size_t to_read = remaining;
        if (to_read > sizeof(recv_buf)) {
            to_read = sizeof(recv_buf);
        }
        int recv_len = httpd_req_recv(req, recv_buf, to_read);

        // Timeout Error: Just retry
        if (recv_len == HTTPD_SOCK_ERR_TIMEOUT){
            continue;
        }

        // Serious Error: Abort OTA
        else if (recv_len <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error");
            return ESP_FAIL;
        }

        // Successful Upload: Flash firmware chunk
        if ((ESP_OK != esp_ota_write(ota_handle, (const void *)recv_buf, recv_len)))
        {
            LOGE("ota flash error");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash Error");
            return ESP_FAIL;
        }

        remaining -= recv_len;

        PRINTF("\rrx %7d\r", req->content_len - remaining);
    }

    PRINTF("\r\n");

    // Validate and switch to new OTA image and reboot
    if ((ESP_OK != esp_ota_end(ota_handle)) ||
        (ESP_OK != esp_ota_set_boot_partition(ota_partition)))
    {
        LOGE("ota activation error");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Validation / Activation Error");
        return ESP_FAIL;
    }

    LOGI("ota update complete");
    httpd_resp_sendstr(req, "Firmware update complete, rebooting now!");

    delayms(1000);
    esp_restart();

    return ESP_OK;
}

// reboot or clear configs
esp_err_t post_reset_handler(httpd_req_t *req)
{
    size_t recv_len = req->content_len;
    if (recv_len >= sizeof(recv_buf)) {
        recv_len = sizeof(recv_buf) - 1;
    }

    memset(recv_buf, 0, sizeof(recv_buf));
    /*int len = */httpd_req_recv(req, recv_buf, recv_len);
    //LOGD("(%d/%u) %.*s", len, recv_len, len, recv_buf);

    httpd_resp_set_type(req, "text/plain");

    if (NULL != strstr(recv_buf, "restart"))
    {
        httpd_resp_sendstr(req, "app reset");
        delayms(3000UL);
        esp_restart();
    }
    else if (NULL != strstr(recv_buf, "restore"))
    {
        int err = 0;
        err += nvs_flash_deinit();
        //LOGD("nvs_flash_deinit() = %d", err);
        err += nvs_flash_erase();
        //LOGD("nvs_flash_erase() = %d", err);
        err += nvs_flash_init();
        //LOGD("nvs_flash_init() = %d", err);
        if (0 != err) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "restore factory settings failed");
        } else {
            httpd_resp_sendstr(req, "restore factory settings success");
        }
        delayms(3000UL);
        esp_restart();
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "unknown reset");
    }

    return ESP_OK;
}

bool start()
{
    static bool b_started = false;

    if (b_started) {
        return b_started;
    }

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;

    if (ESP_OK == httpd_start(&server, &config))
    {

  #define REGISTER_HANDLER(_handler, _uri, _method, func)   \
                httpd_uri_t _handler = {                    \
                    .uri = _uri, .method = _method,         \
                    .handler = func, .user_ctx = NULL       \
                };                                          \
                httpd_register_uri_handler(server, &_handler)

  #define REGISTER_POST_HANDLER(uri, func)  REGISTER_HANDLER(post_ ## func, uri, HTTP_POST, post_ ## func ## _handler)
  #define REGISTER_GET_HANDLER(uri, func)   REGISTER_HANDLER(get_ ## func, uri, HTTP_GET, get_ ## func ## _handler)

        REGISTER_GET_HANDLER("/", indexhtml);
        REGISTER_GET_HANDLER("/favicon.ico", favicon);
        REGISTER_GET_HANDLER("/version", version);
        REGISTER_GET_HANDLER("/username", username);
        REGISTER_GET_HANDLER("/pgn", pgn);

        REGISTER_GET_HANDLER("/lichess-game", gamecfg);
        REGISTER_POST_HANDLER("/lichess-game", gamecfg);
        REGISTER_POST_HANDLER("/lichess-token", apitoken); // lip_l4491VHNfYPMT9V7bQPQ

        REGISTER_GET_HANDLER("/wifi-cfg", wificfg);
        REGISTER_POST_HANDLER("/wifi-cfg", wificfg);

        REGISTER_POST_HANDLER("/update", update);
        REGISTER_POST_HANDLER("/reset", reset);

        b_started = true;
    }

    LOGD("httpd_start() %s", b_started ? "ok" : "failed");
    return b_started;
}

} // namespace web::server
