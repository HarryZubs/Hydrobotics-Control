#include <WiFi.h>
#include <Wire.h>
#include "MS5837.h"

const char* ssid = "hydroboticsDataTransmission";
const char* password = "controlfloat";

WiFiServer server(1234);
WiFiClient client; 
bool clientPresent = false; 

//logic variables
bool motorRunning = false;

void setup() {
  Serial.begin(115200);

  pinMode(D3, OUTPUT);
  pinMode(D4, OUTPUT);

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
  static String msgBuffer = "";

  // Accept new client only if none is connected
  if (!client || !client.connected()) {
    WiFiClient newClient = server.available();
    if (newClient) {
      client = newClient;
      Serial.println("Client connected!");
      client.println("Connected to ESP32");
    }
  }

  // Read incoming data from client (non-blocking)
  while (client && client.connected() && client.available()) {
    char c = client.read();
    if (c != '\r') { // ignore carriage return
      msgBuffer += c;
    }
  }

  // Process message if any data is in buffer
  if (msgBuffer.length() > 0) {
    msgBuffer.trim(); // remove whitespace
    Serial.println("Received: " + msgBuffer);

    // Commands
    if (msgBuffer.equalsIgnoreCase("start")) {
      motorRunning = true;
      client.println("Pump started");
    } else if (msgBuffer.equalsIgnoreCase("stop")) {
      motorRunning = false;
      client.println("Pump stopped");
    } else {
      client.println("Unknown command: " + msgBuffer);
    }

    msgBuffer = ""; // clear buffer for next command
  }

  // Control the pump continuously
  if (motorRunning) {
    digitalWrite(D4, HIGH);
    digitalWrite(D3, LOW);
  } else {
    digitalWrite(D3, LOW);
    digitalWrite(D4, LOW);
  }
}