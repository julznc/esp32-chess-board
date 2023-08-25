
#include <WebServer.h>
#include <Update.h>
#include <nvs_flash.h>

#include "globals.h"

#include "chess/chess.h"
#include "lichess/lichess_api.h"

#include "web_server_cfg.h"
#include "web_server.h"
#include "wifi_setup.h"

#include "index.html.h"
#include "favicon.svg.h"


namespace web
{

WebServer server(WEB_SERVER_HTTP_PORT);

static void handleRoot()
{
    server.send(200, "text/html", index_html);
}

static void handleSetToken()
{
    String token = server.arg("token");
    LOGD("token: %s", token.c_str());
    if (token.length() >= 20 /*min token length?*/)
    {
        size_t len = lichess::set_token(token.c_str());
        server.send(200, "text/plain", len >= token.length() ? "ok" : "failed");
    }
    else
    {
        server.send(400, "text/plain", "failed");
    }
}

static void handleSetGameOptions()
{
    String limit     = server.arg("clock-limit");
    String increment = server.arg("clock-increment");
    String opponent  = server.arg("opponent");
    String mode      = server.arg("mode");

    bool b_status = lichess::set_game_options(opponent, limit.toInt() * 60, increment.toInt(), mode == "rated");
    LOGD("opponent: \"%s\" (%s) time_control: %d+%d (%s)",
        opponent.c_str(), mode.c_str(), limit.toInt(), increment.toInt(), b_status ? "ok" : "failed");
    server.send(b_status ? 200 : 400, "text/plain", b_status ? "ok" : "failed");
}

static void handlePostWiFiCfg()
{
    String ssid   = server.arg("ssid");
    String passwd = server.arg("passwd");

    bool b_status = wifi::set_credentials(ssid.c_str(), passwd.c_str());
    LOGD("set wifi: %s (%s) %s", ssid.c_str(), passwd.c_str(), b_status ? "ok" : "failed");
    server.send(b_status ? 200 : 400, "text/plain", b_status ? "ok" : "failed");
}

static void handlePostReset()
{
    if (server.hasArg("restart"))
    {
        server.send(200, "text/plain", "app reset");
        delay(3000UL);
        ESP.restart();
    }
    else if (server.hasArg("restore"))
    {
        int err = 0;
        err += nvs_flash_deinit();
        //LOGD("nvs_flash_deinit() = %d", err);
        err += nvs_flash_erase();
        //LOGD("nvs_flash_erase() = %d", err);
        err += nvs_flash_init();
        //LOGD("nvs_flash_init() = %d", err);
        server.send(err ? 500 : 200, "text/plain", String("restore factory settings ") + (err ? "failed" : "ok"));
        delay(3000UL);
        ESP.restart();
    }
    else
    {
        server.send(404, "text/plain", "not found");
    }
}

static void handleNotFound()
{
    LOGW("%s %s", (server.method() == HTTP_GET) ? "get" : "post", server.uri().c_str());
    server.send(404, "text/plain", "not found");
}

void serverSetup(void)
{
    server.on("/", handleRoot);

    server.on("/favicon.ico", HTTP_GET, []() {
        server.send(200, "image/svg+xml", favicon_svg);
    });

    server.on("/version", HTTP_GET, []() {
        server.send(200, "text/plain", K_APP_VERSION);
    });

    server.on("/username", HTTP_GET, []() {
        String name;
        server.send(lichess::get_username(name) ? 200 : 404, "text/plain", name.isEmpty() ? "?" : name);
    });

    server.on("/pgn", HTTP_GET, []() {
        String pgn;
        server.send(chess::get_pgn(pgn) ? 200 : 404, "text/plain", pgn.isEmpty() ? "..." : pgn);
    });

    server.on("/lichess-game", HTTP_GET, []() {
        char        rsp[128];
        String      opponent;
        uint16_t    u16_limit;
        uint8_t     u8_increment;
        bool        b_rated;
        lichess::get_game_options(opponent, u16_limit, u8_increment, b_rated);
        snprintf(rsp, sizeof(rsp), "{\"opponent\": \"%s\", \"mode\": \"%s\", \"limit\":%u, \"increment\": %u}",
                opponent.c_str(), b_rated ? "rated" : "casual", u16_limit / 60, u8_increment);
        server.send(200, "application/json", rsp);
    });

    server.on("/wifi-cfg", HTTP_GET, []() {
        char rsp[64];
        String ssid, passwd;
        wifi::get_credentials(ssid, passwd);
        snprintf(rsp, sizeof(rsp), "{\"ssid\": \"%s\", \"passwd\":\"%s\"}", ssid.c_str(), passwd.c_str());
        server.send(200, "application/json", rsp);
    });

    server.on("/lichess-token", HTTP_POST, handleSetToken);
    server.on("/lichess-game", HTTP_POST, handleSetGameOptions);
    server.on("/wifi-cfg", HTTP_POST, handlePostWiFiCfg);
    server.on("/reset", HTTP_POST, handlePostReset);

    server.on("/update", HTTP_POST, []() {
        //LOGD("update done");
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        delay(3000UL);
        ESP.restart();
    }, []() {
        HTTPUpload& upload = server.upload();
        WDT_FEED();
        if (upload.status == UPLOAD_FILE_START) {
            LOGD("Update: %s", upload.filename.c_str());
            if (!Update.begin()) { //start with max available size
                LOGW("start fail (error=%u)", Update.getError());
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            PRINTF(".");
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                LOGW("write fail (error=%u)", Update.getError());
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) { //true to set the size to the current progress
                LOGI("Update Success: %u\r\nRebooting...\r\n", upload.totalSize);
            } else {
                LOGW("end fail (error=%u)", Update.getError());
            }
        } else {
            LOGW("Update Failed Unexpectedly (likely broken connection): status=%d", upload.status);
          #if 0
            ESP.restart();
          #else
            Update.abort();
            server.stop();
            server.begin();
          #endif
        }
    });

    server.onNotFound(handleNotFound);
}

void serverBegin(void)
{
    server.begin();
}

void serverStop(void)
{
    server.stop();
}

void serverHandle(void)
{
    server.handleClient();
}

} // namespace web
