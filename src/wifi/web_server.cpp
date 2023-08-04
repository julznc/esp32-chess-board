
#include <WebServer.h>
#include <Update.h>

#include "globals.h"

#include "web_server_cfg.h"
#include "web_server.h"

#if 0
#include "index.html.h"
#include "favicon.svg.h"
#endif


namespace web
{

WebServer server(WEB_SERVER_HTTP_PORT);

static void handleRoot()
{
    //server.send(200, "text/html", index_html);
    server.send(200, "text/html", "<html>esp32-chess</html>");
}

static void handleNotFound()
{
    LOGW("%s %s", (server.method() == HTTP_GET) ? "get" : "post", server.uri().c_str());
    server.send(404, "text/plain", "not found");
}

void serverSetup(void)
{
    server.on("/", handleRoot);

#if 0 // to do
    server.on("/favicon.ico", HTTP_GET, []() {
        server.send(200, "image/svg+xml", favicon_svg);
    });
#endif

    server.on("/version", HTTP_GET, []() {
        server.send(200, "text/plain", K_APP_VERSION);
    });

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
