#include <WiFi.h>
#include <Wire.h>
#include "MS5837.h"

const char* ssid = "hydroboticsDataTransmission";
const char* password = "controlfloat";

WiFiServer server(1234);
MS5803 MS(0x76);


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

void readWire() {
  Wire.requestFrom(0x76, 24);

  while (Wire.available()) {
    char c = Wire.read();
    Serial.print(c);
  }

  delay(500);
}

void loop() {
  WiFiClient client = server.available();
  MS.read();

  if (client) {
    Serial.println("true");
    client.write("hello");
  }

  Serial.println(MS.getTemperature());

  readWire();
  delay(500);
}