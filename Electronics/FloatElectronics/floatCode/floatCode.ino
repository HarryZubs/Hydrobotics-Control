#include <WiFi.h>
#include "MS5837.h"

const char* ssid = "hydroboticsDataTransmission";
const char* password = "controlfloat";

//wifi server IP:192.168.4.1, port:1234
WiFiServer server(1234);
WiFiClient client; 
bool clientPresent = false; 

//logic variables
bool sequenceRunning = false;
bool reverse = false; 
bool pumpIn = false; 
bool pumpOut = false; 
unsigned long stepStartTime = 0;
int step = 0;
unsigned long manualStartTime = 0;
const unsigned long MANUAL_DURATION = 10000; // 10 seconds

void setup() {
  //start data transmission
  Serial.begin(115200);

  pinMode(A3, OUTPUT);
  pinMode(A2, OUTPUT);

  //start WiFi as server 
  WiFi.mode(WIFI_AP);
  server.begin();

  //start wifi with username and password above
  bool ok = WiFi.softAP(ssid, password);

  if (ok) {
    Serial.println("Network started");
    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("failed");
  }
}

void testSequence() {


  unsigned long currentTime = millis();

  switch (step) {
    case 0: // Pump in 20 sec
    //write pump to go in one direction, check this before testing in water, if opposite, swap A3 to low and A2 to high 
    //swap A3 to high and A2 to low in the second case for when the pump swaps direction
    //do similar swaps in loop function below where i have marked
      digitalWrite(A3, HIGH);
      digitalWrite(A2, LOW);

      if (currentTime - stepStartTime >= 20000) {
        step = 1;
        stepStartTime = currentTime;
      }
      break;
    case 1: // Wait 5 sec
      digitalWrite(A3, LOW);
      digitalWrite(A2, LOW);

      if (currentTime - stepStartTime >= 5000) {
        step = 2;
        stepStartTime = currentTime;
      }
      break;
    case 2: //pump in opposite direction to first test case to pump water out
      digitalWrite(A3, LOW);
      digitalWrite(A2, HIGH);

      if (currentTime - stepStartTime >= 20000) {
        stopSequence();
      }
      break;
  }
}

//sequence to stop the test midway
void stopSequence() {
  digitalWrite(A3, LOW);
  digitalWrite(A2, LOW);
  sequenceRunning = false;
  step = 0;
}

void loop() {
  static String msgBuffer = "";

  // Accept new client only if none is connected
  if (!client || !client.connected()) {
    //initiates a new client if there is no client currently connected 
    WiFiClient newClient = server.available();
    if (newClient) {
      client = newClient;
      //outputs connected to esp32 on client network
      Serial.println("Client connected!");
      client.println("Connected to ESP32");
    }
  }

  // Read incoming data from client (non-blocking)
  while (client && client.connected() && client.available()) {
    //read character from client and add to an array that holds the message
    char c = client.read();
    if (c != '\r') { // ignore carriage return
      msgBuffer += c;
    }
  }

  // Process message if any data is in buffer
  if (msgBuffer.length() > 0) {
  msgBuffer.trim();
  Serial.println("Received: " + msgBuffer);

    if (msgBuffer.equalsIgnoreCase("starttest")) {
      //starts the test sequence and takes a time variable at that time
      sequenceRunning = true;
      pumpIn = false;
      pumpOut = false;
      step = 0;
      stepStartTime = millis();
      client.println("Test started");

    } else if (msgBuffer.equalsIgnoreCase("stoptest")) {
      //stops the test sequence 
      stopSequence();
      pumpIn = false;
      pumpOut = false;
      client.println("Test stopped");

    } else if (msgBuffer.equalsIgnoreCase("pumpin")) {

      stopSequence();
      pumpIn = true;
      pumpOut = false;
      manualStartTime = millis();
      client.println("Pumping in for 10 seconds");

    } else if (msgBuffer.equalsIgnoreCase("pumpout")) {

      stopSequence();
      pumpOut = true;
      pumpIn = false;
      manualStartTime = millis();
      client.println("Pumping out for 10 seconds");
    }

    msgBuffer = "";
  }

  // Control the pump continuously
  if (sequenceRunning) {

    testSequence();

  } else if (pumpIn) {
    //swap high with low and vice versa if the pump is pumping out instead of in
    digitalWrite(A3, HIGH);
    digitalWrite(A2, LOW);

  //if the time since starting the pump sequence and current time is greater than the duration of the manual control, then stop the pumps
    if (millis() - manualStartTime >= MANUAL_DURATION) {
      pumpIn = false;
      digitalWrite(A3, LOW);
      digitalWrite(A2, LOW);
      Serial.println("Pump in complete");
    }
  } else if (pumpOut) {
    //swap high with low and vice versa if the pump is pumping in instead of out
    digitalWrite(A3, LOW);
    digitalWrite(A2, HIGH);

    //if the time since starting the pump sequence and current time is greater than the duration of the manual control, then stop the pumps
    if (millis() - manualStartTime >= MANUAL_DURATION) {
      pumpOut = false;
      digitalWrite(A3, LOW);
      digitalWrite(A2, LOW);
      Serial.println("Pump out complete");
    }

  } else {

    digitalWrite(A3, LOW);
    digitalWrite(A2, LOW);
  }
}