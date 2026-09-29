#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// ═════════════════════════════════════════════════════════════════════
// ⚙️ WIFI & STATIC IP CONFIGURATION
// ═════════════════════════════════════════════════════════════════════
const char* WIFI_SSID = "OPPO Reno10 Pro 5G";              
const char* WIFI_PASSWORD = "12233344445";      

IPAddress staticIP(10, 135, 98, 50);       
IPAddress gateway(10, 135, 98, 1);         
IPAddress subnet(255, 255, 255, 0);        
IPAddress primaryDNS(8, 8, 8, 8);      
IPAddress secondaryDNS(1, 1, 1, 1);        

// ═════════════════════════════════════════════════════════════════════
// 🔌 MOTOR PINS & PARAMETERS (FROM YOUR PROVEN CODE)
// ═════════════════════════════════════════════════════════════════════
#define STEP_PIN 19        
#define DIR_PIN 18         
#define ENABLE_PIN 21      

const float TOTAL_STEPS_PER_REV = 200.0;
const int STEP_DELAY_US = 1000;  // 1000us delay from your original code

WebServer server(80);

volatile int currentAngle = 0;
volatile int currentStepPos = 0; // Tracks absolute physical steps (0-199)
volatile bool motorEnabled = true;
String systemStatus = "Ready";

// ═════════════════════════════════════════════════════════════════════
// 🌐 CORS & SERVER SETUP
// ═════════════════════════════════════════════════════════════════════
void addCORSHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void setupWebServer() {
  server.on("/status", HTTP_GET, []() {
    addCORSHeaders();
    DynamicJsonDocument doc(256);
    doc["status"] = systemStatus;
    doc["angle"] = currentAngle;
    doc["motor_enabled"] = motorEnabled;
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });
  
  server.on("/ping", HTTP_GET, []() {
    addCORSHeaders();
    server.send(200, "application/json", "{\"success\":true,\"message\":\"pong\"}");
  });
  
  server.on("/rotate", HTTP_GET, []() {
    addCORSHeaders();
    if (!server.hasArg("angle")) {
      server.send(400, "application/json", "{\"error\":\"Missing angle\"}");
      return;
    }
    
    // Normalize requested angle to 0-359
    int targetAngle = server.arg("angle").toInt() % 360;
    if (targetAngle < 0) targetAngle += 360;
    
    // --- DRIFT-FREE ABSOLUTE STEP CALCULATION ---
    // Map the 360-degree angle perfectly to a 200-step absolute position
    int targetAbsoluteStep = round((targetAngle / 360.0) * TOTAL_STEPS_PER_REV);
    if (targetAbsoluteStep == 200) targetAbsoluteStep = 0; // Wrap 360 back to 0
    
    // Calculate the shortest path in actual steps
    int stepDiff = targetAbsoluteStep - currentStepPos;
    if (stepDiff > (TOTAL_STEPS_PER_REV / 2)) {
      stepDiff -= TOTAL_STEPS_PER_REV;
    } else if (stepDiff < -(TOTAL_STEPS_PER_REV / 2)) {
      stepDiff += TOTAL_STEPS_PER_REV;
    }
    
    int stepsNeeded = abs(stepDiff);
    bool clockwise = (stepDiff > 0);
    
    if (stepsNeeded > 0) {
      digitalWrite(DIR_PIN, clockwise ? HIGH : LOW);
      digitalWrite(ENABLE_PIN, LOW); // Ensure motor is engaged
      
      for (int i = 0; i < stepsNeeded; i++) {
        digitalWrite(STEP_PIN, HIGH);
        delayMicroseconds(STEP_DELAY_US);
        digitalWrite(STEP_PIN, LOW);
        delayMicroseconds(STEP_DELAY_US);
      }
      
      // Update global trackers
      currentStepPos = targetAbsoluteStep;
      currentAngle = targetAngle;
    }
    
    DynamicJsonDocument doc(128);
    doc["success"] = true;
    doc["angle"] = currentAngle;
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });

  server.on("/enable-motor", HTTP_GET, []() {
    addCORSHeaders();
    digitalWrite(ENABLE_PIN, LOW);
    motorEnabled = true;
    server.send(200, "application/json", "{\"success\":true}");
  });

  server.on("/disable-motor", HTTP_GET, []() {
    addCORSHeaders();
    digitalWrite(ENABLE_PIN, HIGH);
    motorEnabled = false;
    server.send(200, "application/json", "{\"success\":true}");
  });

  server.onNotFound([]() {
    if (server.method() == HTTP_OPTIONS) {
      addCORSHeaders();
      server.send(200);
    } else {
      server.send(404, "text/plain", "Not found");
    }
  });
}

// ═════════════════════════════════════════════════════════════════════
// 🚀 SETUP & LOOP
// ═════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  
  // Start with motor enabled (LOW) based on your original code structure
  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIR_PIN, LOW);
  digitalWrite(ENABLE_PIN, LOW); 

  Serial.println("[WIFI] Connecting...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  // Fetch real hotspot routing dynamically
  IPAddress realGateway = WiFi.gatewayIP();
  IPAddress realDNS = WiFi.dnsIP();
  WiFi.config(staticIP, realGateway, subnet, realDNS, primaryDNS);
  
  setupWebServer();
  server.begin();
  
  Serial.println("\n[MOTOR] Controller Ready!");
  Serial.println("[MOTOR] Static IP: " + WiFi.localIP().toString());
}

void loop() {
  server.handleClient();
  delay(10);
}