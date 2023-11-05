
#include <esp_http_server.h>
#include <esp_tls_crypto.h>

#include "globals.h"

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

/* serve index.html */
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
esp_err_t get_index_handler(httpd_req_t *req)
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
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "to do");
    return ESP_OK;
}

/* send current PGN */
esp_err_t get_pgn_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "to do");
    return ESP_OK;
}

/* send lichess game settings */
esp_err_t get_gamecfg_handler(httpd_req_t *req)
{
    char        opponent[64] = "to do";
    uint16_t    u16_limit = 15 * 60;
    uint8_t     u8_increment = 10;
    bool        b_rated = false;

    snprintf(send_buf, sizeof(send_buf) - 1,
            "{\"opponent\": \"%s\", \"mode\": \"%s\", \"limit\":%u, \"increment\": %u}",
            opponent, b_rated ? "rated" : "casual", u16_limit / 60, u8_increment);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, send_buf);
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

bool start()
{
    static bool b_started = false;

    if (b_started) {
        return b_started;
    }

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_uri_t get_index = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = get_index_handler,
        .user_ctx = NULL
    };

    httpd_uri_t get_favicon = {
        .uri      = "/favicon.ico",
        .method   = HTTP_GET,
        .handler  = get_favicon_handler,
        .user_ctx = NULL
    };

    httpd_uri_t get_version = {
        .uri      = "/version",
        .method   = HTTP_GET,
        .handler  = get_version_handler,
        .user_ctx = NULL
    };

    httpd_uri_t get_username = {
        .uri      = "/username",
        .method   = HTTP_GET,
        .handler  = get_username_handler,
        .user_ctx = NULL
    };

    httpd_uri_t get_pgn = {
        .uri      = "/pgn",
        .method   = HTTP_GET,
        .handler  = get_pgn_handler,
        .user_ctx = NULL
    };

    httpd_uri_t get_gamecfg = {
        .uri      = "/lichess-game",
        .method   = HTTP_GET,
        .handler  = get_gamecfg_handler,
        .user_ctx = NULL
    };

    httpd_uri_t get_wificfg = {
        .uri      = "/wifi-cfg",
        .method   = HTTP_GET,
        .handler  = get_wificfg_handler,
        .user_ctx = NULL
    };

    httpd_uri_t post_update = {
        .uri      = "/update",
        .method   = HTTP_POST,
        .handler  = post_update_handler,
        .user_ctx = NULL
    };

    if (ESP_OK == httpd_start(&server, &config))
    {
        httpd_register_uri_handler(server, &get_index);
        httpd_register_uri_handler(server, &get_favicon);
        httpd_register_uri_handler(server, &get_version);
        httpd_register_uri_handler(server, &get_username);
        httpd_register_uri_handler(server, &get_pgn);
        httpd_register_uri_handler(server, &get_gamecfg);
        httpd_register_uri_handler(server, &get_wificfg);
        httpd_register_uri_handler(server, &post_update);
        b_started = true;
    }

    LOGD("httpd_start() %s", b_started ? "ok" : "failed");
    return b_started;
}

} // namespace web::server
