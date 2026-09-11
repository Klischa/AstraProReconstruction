#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include <Preferences.h>
#include <ESPmDNS.h>

#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     21
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       19
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM        5
#define Y2_GPIO_NUM        4
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

const char* ap_ssid = "ESP32-CAM";
const char* ap_pass = "12345678";

String sta_ssid = "";
String sta_pass = "";

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;
Preferences pref;
bool ap_up = false;

void ensureAP(bool on) {
  if (on && !ap_up) {
    WiFi.softAP(ap_ssid, ap_pass);
    ap_up = true;
    Serial.println("[net] AP re-enabled (fallback)");
  } else if (!on && ap_up) {
    WiFi.softAPdisconnect(true);
    ap_up = false;
    Serial.println("[net] AP disabled (STA connected)");
  }
}

void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case WIFI_EVENT_STA_DISCONNECTED: {
      int r = info.wifi_sta_disconnected.reason;
      Serial.printf("[WiFi] STA disconnect, reason=%d (201=no AP, 202=auth fail/wrong pass, 15=handshake timeout)\n", r);
      break;
    }
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("[WiFi] STA connected to router");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[WiFi] got IP: %s\n", WiFi.localIP().toString().c_str());
      break;
    default:
      break;
  }
}

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  char part_buf[64];
  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;
  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      res = ESP_FAIL;
    } else {
      size_t hlen = snprintf(part_buf, 64, _STREAM_PART, fb->len);
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
      if (res == ESP_OK)
        res = httpd_resp_send_chunk(req, part_buf, hlen);
      if (res == ESP_OK)
        res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
      esp_camera_fb_return(fb);
      if (res != ESP_OK) break;
    }
  }
  return res;
}

static esp_err_t capture_handler(httpd_req_t *req) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
  esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return res;
}

static esp_err_t set_res_handler(httpd_req_t *req) {
  char buf[32];
  int ret = httpd_req_get_url_query_str(req, buf, sizeof(buf));
  if (ret != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing query");
    return ESP_FAIL;
  }
  char val[8];
  if (httpd_query_key_value(buf, "size", val, sizeof(val)) != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing size");
    return ESP_FAIL;
  }
  int size = atoi(val);
  sensor_t *s = esp_camera_sensor_get();
  if (!s) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No sensor");
    return ESP_FAIL;
  }
  s->set_framesize(s, (framesize_t)size);
  httpd_resp_set_type(req, "text/plain");
  return httpd_resp_send(req, "OK", 2);
}

static esp_err_t set_quality_handler(httpd_req_t *req) {
  char buf[32];
  int ret = httpd_req_get_url_query_str(req, buf, sizeof(buf));
  if (ret != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing query");
    return ESP_FAIL;
  }
  char val[8];
  if (httpd_query_key_value(buf, "val", val, sizeof(val)) != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing val");
    return ESP_FAIL;
  }
  int q = atoi(val);
  sensor_t *s = esp_camera_sensor_get();
  if (!s) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No sensor");
    return ESP_FAIL;
  }
  s->set_quality(s, q);
  httpd_resp_set_type(req, "text/plain");
  return httpd_resp_send(req, "OK", 2);
}

static esp_err_t scanwifi_handler(httpd_req_t *req) {
  Serial.printf("scanwifi: mode=%d WL=%d\n", WiFi.getMode(), WiFi.status());
  httpd_resp_set_type(req, "application/json");
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    delay(200);
  }
  int n = -1;
  for (int t = 0; t < 3 && n < 0; t++) {
    if (t > 0) delay(1500);
    n = WiFi.scanNetworks();
    Serial.printf("  attempt %d -> %d\n", t + 1, n);
    if (n > 0) {
      for (int i = 0; i < n; i++) {
        Serial.printf("    [%d] '%s' rssi=%d auth=%d (3=WPA2,4=WPA/WPA2,7=WPA3)\n",
                      i, WiFi.SSID(i).c_str(), WiFi.RSSI(i), (int)WiFi.encryptionType(i));
      }
    }
  }
  if (n < 0) n = 0;
  httpd_resp_send_chunk(req, "[", 1);
  for (int i = 0; i < n; i++) {
    char line[80];
    int rssi = WiFi.RSSI(i);
    const char* open = WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "1" : "0";
    snprintf(line, sizeof(line),
             "%s{\"ssid\":\"%s\",\"rssi\":%d,\"open\":%s}",
             i ? "," : "",
             WiFi.SSID(i).c_str(), rssi, open);
    httpd_resp_send_chunk(req, line, strlen(line));
  }
  httpd_resp_send_chunk(req, "]", 1);
  WiFi.scanDelete();
  return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t connect_handler(httpd_req_t *req) {
  Serial.println("connect_handler called");
  char buf[256];
  int ret = httpd_req_get_url_query_str(req, buf, sizeof(buf));
  Serial.printf("  get_url_query_str ret=%d buf='%s'\n", ret, buf);
  if (ret != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing query");
    return ESP_FAIL;
  }
  char ssid[64], pass[64];
  if (httpd_query_key_value(buf, "ssid", ssid, sizeof(ssid)) != ESP_OK ||
      httpd_query_key_value(buf, "pass", pass, sizeof(pass)) != ESP_OK) {
    Serial.println("  parse ssid/pass FAILED");
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ssid/pass");
    return ESP_FAIL;
  }
  Serial.printf("  ssid='%s' pass='%s'\n", ssid, pass);

  pref.begin("wifi", false);
  pref.putString("ssid", String(ssid));
  pref.putString("pass", String(pass));
  pref.end();
  Serial.println("  config saved to NVS");

  httpd_resp_set_type(req, "text/plain");
  httpd_resp_send(req, "SAVED", 5);

  Serial.println("  rebooting in 500ms...");
  delay(500);
  ESP.restart();
  return ESP_OK;
}

static esp_err_t resetwifi_handler(httpd_req_t *req) {
  pref.begin("wifi", false);
  pref.remove("ssid");
  pref.remove("pass");
  pref.end();
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_send(req, "OK", 2);
  Serial.println("WiFi config cleared, rebooting...");
  delay(300);
  ESP.restart();
  return ESP_OK;
}

static esp_err_t index_handler(httpd_req_t *req) {
  char ip[20];
  strcpy(ip, WiFi.localIP().toString().c_str());
  if (strcmp(ip, "0.0.0.0") == 0) strcpy(ip, "192.168.4.1");

  httpd_resp_set_type(req, "text/html");

  char b[512];
  const char* part0 =
    "<!DOCTYPE html><html><head><title>ESP32 Camera</title>"
    "<meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<style>body{font-family:sans-serif;text-align:center;background:#111;color:#eee;margin:0;padding:10px}"
    "h1{margin:10px 0;font-size:1.4em}img{max-width:100%;border:2px solid #444;border-radius:8px;margin:8px 0}"
    ".ctrl{margin:8px 0;display:flex;flex-wrap:wrap;justify-content:center;gap:6px}"
    ".ctrl button,.ctrl select{padding:8px 14px;border:none;border-radius:6px;cursor:pointer;font-size:14px;background:#333;color:#eee}"
    ".ctrl button:hover{background:#555}.ctrl select{background:#222;color:#eee}"
    "label{font-size:13px;margin-right:4px}"
    ".wifi{background:#2a2a2a;border:1px solid #444;border-radius:8px;padding:12px;margin:10px auto;max-width:420px;text-align:left}"
    ".wifi h3{margin:0 0 8px;font-size:14px;color:#0af}"
    ".wifi ul{list-style:none;margin:0;padding:0;max-height:160px;overflow-y:auto}"
    ".wifi li{cursor:pointer;padding:8px;border-bottom:1px solid #333;display:flex;justify-content:space-between}"
    ".wifi li:hover{background:#333}.wifi input{width:100%;padding:8px;margin:4px 0;box-sizing:border-box;background:#222;border:1px solid #444;color:#eee;border-radius:4px}"
    ".wifi button{width:100%;padding:10px;margin-top:6px;background:#0af;color:#000;border:none;border-radius:6px;cursor:pointer;font-weight:bold}"
    ".wifi small{color:#888}.st{color:#4f4;font-size:12px}</style></head><body>"
    "<h1>ESP32-WROVER Camera</h1>"
    "<p class=\"st\">IP: ";

  httpd_resp_send_chunk(req, part0, strlen(part0));
  snprintf(b, sizeof(b), "%s%s%s</p>", ip,
           sta_ssid.length() ? " · Wi-Fi: " : "",
           sta_ssid.length() ? sta_ssid.c_str() : "");
  httpd_resp_send_chunk(req, b, strlen(b));

  const char* part1 =
    "<div class=\"ctrl\"><label>Разрешение:</label>"
    "<select id=\"res\" onchange=\"setRes(this.value)\">"
    "<option value=\"5\">VGA 640x480</option><option value=\"7\">SVGA 800x600</option>"
    "<option value=\"8\">XGA 1024x768</option><option value=\"9\">SXGA 1280x1024</option>"
    "<option value=\"10\">UXGA 1600x1200</option><option value=\"4\">QVGA 320x240</option></select>"
    "<label>Качество:</label>"
    "<select id=\"qual\" onchange=\"setQual(this.value)\">"
    "<option value=\"4\">Высокое (4)</option><option value=\"8\">Среднее (8)</option>"
    "<option value=\"12\" selected>Норма (12)</option><option value=\"18\">Низкое (18)</option>"
    "<option value=\"28\">Минимум (28)</option></select></div>"
    "<img src=\"http://";
  httpd_resp_send_chunk(req, part1, strlen(part1));

  snprintf(b, sizeof(b), "%s:81/\">", ip);
  httpd_resp_send_chunk(req, b, strlen(b));

  const char* part2 =
    "<div class=\"wifi\"><h3>WiFi (подключение к роутеру)</h3>"
    "<ul id=\"netlist\"></ul>"
    "<button onclick=\"scanw()\" style=\"margin-bottom:6px\">Сканировать заново</button></div>"
    "<div id=\"wifiForm\" style=\"display:none\">"
    "<input id=\"net_ssid\" readonly placeholder=\"Сеть\">"
    "<input id=\"net_pass\" placeholder=\"Пароль\">"
    "<input id=\"net_isopen\" type=\"hidden\">"
    "<button onclick=\"saveWifi()\">Подключить и перезагрузить</button></div>"
    "<div id=\"wifiStatus\" style=\"margin-top:6px;color:#fa0;font-size:13px\"></div>"
    "<small>Сейчас: AP ESP32-CAM (12345678)</small></div>"
    "<script>"
    "var ip='";
  httpd_resp_send_chunk(req, part2, strlen(part2));

  snprintf(b, sizeof(b), "%s';", ip);
  httpd_resp_send_chunk(req, b, strlen(b));

  const char* part3 =
    "async function scanw(){try{var r=await fetch('/scanwifi');var n=await r.json();"
    "var s=n.map(function(w){return '<li onclick=\"sel(\\''+w.ssid.replace(/'/g,'')+'\\','+w.open+')\">"
    "<span>'+w.ssid+'</span><span>'+w.rssi+'dBm</span></li>';}).join('');"
    "document.getElementById('netlist').innerHTML=s||'<li>Сетей не найдено</li>';}catch(e){}}"
    "function sel(s,o){document.getElementById('net_ssid').value=s;"
    "document.getElementById('net_isopen').value=o;document.getElementById('wifiForm').style.display='block';"
    "document.getElementById('net_pass').disabled=o;}"
    "async function saveWifi(){var s=document.getElementById('net_ssid').value;"
    "var p=document.getElementById('net_pass').value;"
    "var o=document.getElementById('net_isopen').value==1;if(o)p='';"
    "var st=document.getElementById('wifiStatus');st.innerHTML='Отправляю...';"
    "window.location='/connect?ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p);}"
    "function setRes(v){fetch('/set_resolution?size='+v)}"
    "function setQual(v){fetch('/set_quality?val='+v)}"
    "scanw();</script></body></html>";
  httpd_resp_send_chunk(req, part3, strlen(part3));

  return httpd_resp_send_chunk(req, NULL, 0);
}

void startServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.ctrl_port = 32769;
  config.stack_size = 10000;
  httpd_start(&camera_httpd, &config);

  httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_handler };
  httpd_uri_t capture_uri = { .uri = "/capture", .method = HTTP_GET, .handler = capture_handler };
  httpd_uri_t set_res_uri = { .uri = "/set_resolution", .method = HTTP_GET, .handler = set_res_handler };
  httpd_uri_t set_qual_uri = { .uri = "/set_quality", .method = HTTP_GET, .handler = set_quality_handler };
  httpd_uri_t scan_uri = { .uri = "/scanwifi", .method = HTTP_GET, .handler = scanwifi_handler };
  httpd_uri_t conn_uri = { .uri = "/connect", .method = HTTP_GET, .handler = connect_handler };
  httpd_uri_t reset_uri = { .uri = "/resetwifi", .method = HTTP_GET, .handler = resetwifi_handler };
  httpd_register_uri_handler(camera_httpd, &index_uri);
  httpd_register_uri_handler(camera_httpd, &capture_uri);
  httpd_register_uri_handler(camera_httpd, &set_res_uri);
  httpd_register_uri_handler(camera_httpd, &set_qual_uri);
  httpd_register_uri_handler(camera_httpd, &scan_uri);
  httpd_register_uri_handler(camera_httpd, &conn_uri);
  httpd_register_uri_handler(camera_httpd, &reset_uri);

  config.server_port = 81;
  config.ctrl_port = 32770;
  config.stack_size = 10000;
  httpd_start(&stream_httpd, &config);
  httpd_uri_t stream_uri = { .uri = "/", .method = HTTP_GET, .handler = stream_handler };
  httpd_register_uri_handler(stream_httpd, &stream_uri);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== ESP32 Camera Stream ===");

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 12;
  config.fb_count = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init FAILED: 0x%x\n", err);
    while (1) { delay(1000); }
  }
  Serial.println("Camera init OK");

  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_brightness(s, 1);
    s->set_saturation(s, -1);
  }

  pref.begin("wifi", true);
  sta_ssid = pref.getString("ssid", "");
  sta_pass = pref.getString("pass", "");
  pref.end();

  WiFi.mode(WIFI_AP_STA);
  WiFi.onEvent(onWifiEvent);
  WiFi.setAutoReconnect(false);
  WiFi.setSleep(false);
  Serial.printf("ESP32 STA MAC: %s\n", WiFi.macAddress().c_str());
  ensureAP(true);

  if (sta_ssid.length() > 0) {
    Serial.printf("Connecting to Wi-Fi '%s'...\n", sta_ssid.c_str());
    WiFi.begin(sta_ssid.c_str(), sta_pass.c_str());
  } else {
    Serial.println("No saved Wi-Fi config. AP mode only.");
  }

  startServer();
  Serial.println("Servers started on port 80 (UI) and 81 (stream)");
  Serial.printf("AP:   http://192.168.4.1/\n");
  delay(3000);
  if (WiFi.status() == WL_CONNECTED) {
    ensureAP(false);
    MDNS.begin("esp32cam");
    MDNS.addService("http", "tcp", 80);
    Serial.printf("LAN:  http://%s/\n", WiFi.localIP().toString().c_str());
    Serial.printf("mDNS: http://esp32cam.local/\n");
  }
}

void loop() {
  static unsigned long lastLog = 0;
  static unsigned long lastRetry = 0;
  static unsigned long startTry = 0;
  static bool mdnsStarted = false;
  static int prevWL = -1;
  unsigned long now = millis();

  if (sta_ssid.length() > 0) {
    if (WiFi.status() != WL_CONNECTED) {
      ensureAP(true);
      if (now - startTry > 30000) {
        startTry = now;
        Serial.println("[net] retrying STA connection...");
        WiFi.disconnect();
        delay(200);
        WiFi.begin(sta_ssid.c_str(), sta_pass.c_str());
      }
    }
  }

  if (WiFi.status() == WL_CONNECTED && prevWL != WL_CONNECTED) {
    ensureAP(false);
    if (!mdnsStarted) {
      mdnsStarted = true;
      MDNS.begin("esp32cam");
      MDNS.addService("http", "tcp", 80);
    }
    Serial.printf("[net] STA connected, IP: %s\n", WiFi.localIP().toString().c_str());
  }
  if (WiFi.status() != WL_CONNECTED && prevWL == WL_CONNECTED) {
    Serial.println("[net] STA connection lost, AP fallback enabled");
    mdnsStarted = false;
  }
  prevWL = WiFi.status();

  if (now - lastLog > 10000) {
    lastLog = now;
    Serial.printf("[status] WL=%d IP=%s RSSI=%d AP(%d)\n",
                  WiFi.status(), WiFi.localIP().toString().c_str(),
                  WiFi.RSSI(), (int)ap_up);
  }
  delay(200);
}