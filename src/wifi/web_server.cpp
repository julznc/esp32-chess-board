
#include <WebServer.h>
#include <Update.h>

#include "globals.h"

#include "web_server_cfg.h"
#include "web_server.h"

#include "index.html.h"
#include "favicon.svg.h"


namespace web
{

WebServer server(WEB_SERVER_HTTP_PORT);

static void handleRoot()
{
    server.send(200, "text/html", index_html);
}

static void handleGetUsername()
{
    server.send(200, "text/plain", "to do");
}

static void handleGetPgn()
{
    server.send(200, "text/plain", "to do");
}

static void handlePostToken()
{
    //server.send(200, "text/plain", "to do");
    server.send(302, "text/html", "<head><meta http-equiv=\"refresh\" content=\"0;url=/\"></head>");
}

static void handlePostChallenge()
{
    server.send(200, "text/plain", "to do");
}

static void handlePostWiFiCfg()
{
    server.send(200, "text/plain", "to do");
}

static void handlePostReset()
{
    server.send(200, "text/plain", "to do");
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

    server.on("/username", HTTP_GET, handleGetUsername);
    server.on("/pgn", HTTP_GET, handleGetPgn);
    server.on("/lichess-token", HTTP_POST, handlePostToken);
    server.on("/lichess-challenge", HTTP_POST, handlePostChallenge);
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
