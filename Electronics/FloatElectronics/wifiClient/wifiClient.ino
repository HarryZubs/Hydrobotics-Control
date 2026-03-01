#include <WiFi.h>
#include <Wire.h>
#include "MS5837.h"

const char* ssid = "hydroboticsDataTransmission";
const char* password = "controlfloat";

WiFiServer server(1234);
WiFiClient client; 
bool clientPresent = false; 


void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_AP);
  server.begin();
  Wire.begin();


  bool ok = WiFi.softAP(ssid, password);

  if (ok) {
    Serial.println("Network started");
    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("failed");
  }
}

void loop() {
  if (!client || !client.connected()) {
    client = server.available();
    if (client) {
      Serial.println("Client connected!");
      client.println("Connected to ESP32");
    }
  }

  // Handle client messages if connected
  if (client && client.connected() && client.available()) {
    String msg = client.readStringUntil('\n'); // Non-blocking if data available
    Serial.println("Received: " + msg);
    client.println("ESP32 Received: " + msg);
  }
}