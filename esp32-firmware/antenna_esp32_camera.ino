#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

const char* WIFI_SSID = "OPPO Reno10 Pro 5G";
const char* WIFI_PASSWORD = "12233344445";

IPAddress staticIP(10, 135, 98, 51);      
IPAddress subnet(255, 255, 255, 0);

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

httpd_handle_t api_httpd = NULL;
httpd_handle_t stream_httpd = NULL;
volatile bool isExtracting = false; 

// --- RESTORED DASHBOARD HTML ---
static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>Camera Alignment</title>
  <style>
    body { font-family: sans-serif; background: #121212; color: #fff; text-align: center; margin: 20px; }
    .stream-container { position: relative; display: inline-block; border: 3px solid #333; }
    img { max-width: 640px; width: 100%; display: block; }
    .overlay-box { position: absolute; top: 25%; left: 15%; width: 70%; height: 50%; border: 2px dashed #00f2fe; pointer-events: none; }
  </style>
</head>
<body>
  <h2>📡 Meter Alignment Stream</h2>
  <div class="stream-container">
    <img id="stream-img" alt="Live Stream">
    <div class="overlay-box"></div>
  </div>
  <p>Align the numbers inside the dashed box. Ensure there is NO glare.</p>
  <script>
    window.onload = function() {
      document.getElementById('stream-img').src = "http://" + window.location.hostname + ":81/stream";
    }
  </script>
</body>
</html>
)rawliteral";

void set_cors_headers(httpd_req_t *req) {
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, OPTIONS");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
}

static esp_err_t options_handler(httpd_req_t *req) {
  set_cors_headers(req);
  httpd_resp_set_status(req, "200 OK");
  return httpd_resp_send(req, "", 0);
}

static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t ping_handler(httpd_req_t *req) {
  set_cors_headers(req);
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, "{\"success\":true,\"message\":\"pong\"}", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t status_handler(httpd_req_t *req) {
  set_cors_headers(req);
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, "{\"status\":\"Ready\",\"ip\":\"10.135.98.51\"}", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t capture_handler(httpd_req_t *req) {
  set_cors_headers(req);
  isExtracting = true; 
  vTaskDelay(pdMS_TO_TICKS(150)); 
  
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    isExtracting = false;
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"error\":\"Capture failed\"}", HTTPD_RESP_USE_STRLEN);
  }

  uint8_t * _jpg_buf = NULL;
  size_t _jpg_buf_len = 0;
  bool converted = false;

  if (fb->format != PIXFORMAT_JPEG) {
    converted = fmt2jpg(fb->buf, fb->len, fb->width, fb->height, fb->format, 80, &_jpg_buf, &_jpg_buf_len);
    esp_camera_fb_return(fb);
    fb = NULL;
    if (!converted) {
      isExtracting = false;
      httpd_resp_set_type(req, "application/json");
      return httpd_resp_send(req, "{\"error\":\"JPEG conversion failed\"}", HTTPD_RESP_USE_STRLEN);
    }
  } else {
    _jpg_buf = fb->buf;
    _jpg_buf_len = fb->len;
  }

  httpd_resp_set_type(req, "image/jpeg");
  esp_err_t res = httpd_resp_send(req, (const char *)_jpg_buf, _jpg_buf_len);

  if (converted && _jpg_buf) free(_jpg_buf);
  else if (fb) esp_camera_fb_return(fb);

  isExtracting = false;
  return res;
}

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  char part_buf[64];

  res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=123456789000000000000987654321");
  if (res != ESP_OK) return res;

  while (true) {
    if (isExtracting) {
      vTaskDelay(pdMS_TO_TICKS(100)); 
      continue;
    }

    fb = esp_camera_fb_get();
    if (!fb) {
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }

    uint8_t * _jpg_buf = NULL;
    size_t _jpg_buf_len = 0;
    bool converted = false;

    if (fb->format != PIXFORMAT_JPEG) {
      converted = fmt2jpg(fb->buf, fb->len, fb->width, fb->height, fb->format, 65, &_jpg_buf, &_jpg_buf_len);
      esp_camera_fb_return(fb);
      fb = NULL;
      if (!converted) {
        vTaskDelay(pdMS_TO_TICKS(20));
        continue;
      }
    } else {
      _jpg_buf = fb->buf;
      _jpg_buf_len = fb->len;
    }

    size_t hlen = snprintf(part_buf, 64, "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", _jpg_buf_len);
    res = httpd_resp_send_chunk(req, part_buf, hlen);
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, "\r\n--123456789000000000000987654321\r\n", 37);

    if (converted && _jpg_buf) free(_jpg_buf);
    else if (fb) esp_camera_fb_return(fb);

    if (res != ESP_OK) break;
    vTaskDelay(pdMS_TO_TICKS(40)); 
  }
  return res;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_RGB565; 
  config.frame_size = FRAMESIZE_VGA;      
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_count = 2;                    
  config.fb_location = CAMERA_FB_IN_PSRAM;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("[CAMERA] Init Failed!");
    while (true) delay(1000);
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  
  IPAddress realGateway = WiFi.gatewayIP();
  IPAddress realDNS = WiFi.dnsIP();
  WiFi.config(staticIP, realGateway, subnet, realDNS, IPAddress(8,8,8,8));

  // --- RESTORED ROUTING (12 Handlers max) ---
  httpd_config_t config_api = HTTPD_DEFAULT_CONFIG();
  config_api.server_port = 80;
  config_api.ctrl_port = 32768;
  config_api.max_uri_handlers = 12; 

  httpd_uri_t uri_index   = { .uri = "/",        .method = HTTP_GET, .handler = index_handler,   .user_ctx = NULL };
  httpd_uri_t uri_ping    = { .uri = "/ping",    .method = HTTP_GET, .handler = ping_handler,    .user_ctx = NULL };
  httpd_uri_t uri_status  = { .uri = "/status",  .method = HTTP_GET, .handler = status_handler,  .user_ctx = NULL };
  httpd_uri_t uri_capture = { .uri = "/capture", .method = HTTP_GET, .handler = capture_handler, .user_ctx = NULL };
  
  httpd_uri_t opt_ping    = { .uri = "/ping",    .method = HTTP_OPTIONS, .handler = options_handler, .user_ctx = NULL };
  httpd_uri_t opt_status  = { .uri = "/status",  .method = HTTP_OPTIONS, .handler = options_handler, .user_ctx = NULL };
  httpd_uri_t opt_capture = { .uri = "/capture", .method = HTTP_OPTIONS, .handler = options_handler, .user_ctx = NULL };

  if (httpd_start(&api_httpd, &config_api) == ESP_OK) {
    httpd_register_uri_handler(api_httpd, &uri_index);
    httpd_register_uri_handler(api_httpd, &uri_ping);
    httpd_register_uri_handler(api_httpd, &opt_ping);
    httpd_register_uri_handler(api_httpd, &uri_status);
    httpd_register_uri_handler(api_httpd, &opt_status);
    httpd_register_uri_handler(api_httpd, &uri_capture);
    httpd_register_uri_handler(api_httpd, &opt_capture);
  }

  httpd_config_t config_stream = HTTPD_DEFAULT_CONFIG();
  config_stream.server_port = 81;
  config_stream.ctrl_port = 32769;

  httpd_uri_t uri_stream = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL };

  if (httpd_start(&stream_httpd, &config_stream) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &uri_stream);
  }
}

void loop() {
  delay(100);
}