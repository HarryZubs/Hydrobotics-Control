//wifi library for data transmission and remote input commands
#include <WiFi.h>
//ms5837 depth sensor chip library
#include <MS5837.h>
//i2c library
#include <Wire.h>

//set wifi network name to 'hydroboticsDataTransmission' and password to 'controlfloat' 
const char* ssid = "hydroboticsDataTransmission";
const char* password = "controlfloat";

//wifi server IP:192.168.4.1, port:1234
WiFiServer server(1234);
WiFiClient client; 
bool clientPresent = false; 

//initialise MS5837 sensor object
MS5837 sensor;

//logic variables for pump
bool sequenceRunning = false;
bool reverse = false; 
bool pumpIn = false; 
bool pumpOut = false; 
bool pumpAir = false; 
String ClientInput = "";


void setup() {
  //start data transmission
  Serial.begin(115200);

  //start WiFi as server 
  WiFi.mode(WIFI_AP);
  server.begin();

  //start i2c
  Wire.begin();

  //start wifi with network name and password above
  bool ok = WiFi.softAP(ssid, password);

  //if wifi network initiated, display ip address, otherwise return failed to console
  if (ok) {
    Serial.println("Network started");
    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("failed");
  }

  //if sensor not connected print init failed
  while (!sensor.init()) {
    Serial.println("Init failed!");
    delay(5000);
  }
  
  //set model type, 2BA or 3BA
  sensor.setModel(MS5837::MS5837_30BA);
  sensor.setFluidDensity(997);
}

void testSequence() {
  //pump in water and let pumps run for 25 seconds
  digitalWrite(A3, HIGH);
  digitalWrite(A2, LOW);
  delay(25000);

  //stop pumps for 5 seconds
  digitalWrite(A3, LOW);
  digitalWrite(A2, LOW);
  delay(5000);

  //pump water out for 25 seconds then stop pumps 
  digitalWrite(A3, LOW);
  digitalWrite(A2, HIGH);
  delay(25000);

  stopSequence();
}

//sequence to stop the test midway
void stopSequence() {
  //reset pump pin voltages
  digitalWrite(A3, LOW);
  digitalWrite(A2, LOW);
  sequenceRunning = false;
}

void loop() {

  // Accept new client only if none is connected
  if (!client || !client.connected()) {
    //initiates a new client if there is no client currently connected 
    WiFiClient newClient = server.available();

    //print client connected to computer console and Connected to ESP32 to client console
    Serial.println("Client connected!");
    client.println("Connected to ESP32");
  }

  //read input from client if there is one
  if (client.readString() != ""){
    ClientInput = client.readString();
  }
  
  if (ClientInput == "starttest") {
    //starts the test sequence and takes a time variable at that time
    sequenceRunning = true;
    pumpIn = false;
    pumpOut = false;
    pumpAir = false;
    client.println("Test started");

  } else if (ClientInput == "stoptest") {
    //stops the test sequence 
    stopSequence();
    pumpIn = false;
    pumpOut = false;
    pumpAir = false;
    client.println("Test stopped");

  } else if (ClientInput == "pumpin") {
    //starts sequence to pump water into the tank for 10 seconds
    stopSequence();
    pumpIn = true;
    pumpOut = false;
    pumpAir = false;
    client.println("Pumping in for 10 seconds");

  } else if (ClientInput == "pumpout") {
    //starts sequence to pump water out of the tank for 10 seconds
    stopSequence();
    pumpOut = true;
    pumpIn = false;
    pumpAir = false;
    client.println("Pumping out for 10 seconds");
  } else if (ClientInput == "pumpair") {
    //pumps air out of the water tank to maximise water capacity
    stopSequence();
    pumpOut = false;
    pumpIn = false;
    pumpAir = true; 
    client.println("Pumping air");
  }

  // Control the pump continuously
  if (sequenceRunning) {

    testSequence();

  } else if (pumpIn) {
    //swap high with low and vice versa if the pump is pumping out instead of in
    //pumps in for 35 seconds
    digitalWrite(A3, HIGH);
    digitalWrite(A2, LOW);
    delay(35000);
    //stop pump and reset variable
    pumpIn = false;
    digitalWrite(A3, LOW);
    digitalWrite(A2, LOW);
    Serial.println("Pump in complete");

  } else if (pumpOut) {
    //swap high with low and vice versa if the pump is pumping in instead of out
    //pumps out for 35 seconds
    digitalWrite(A3, LOW);
    digitalWrite(A2, HIGH);
    delay(35000);
    pumpOut = false;
    digitalWrite(A3, LOW);
    digitalWrite(A2, LOW);
    Serial.println("Pump out complete");

  //if pump air variable is equal to true, pump out for 5 seconds then stop pumps and reset pumpAir variable
  } else if(pumpAir == true){
    digitalWrite(A3, LOW);
    digitalWrite(A2, HIGH);
    delay(5000);
    digitalWrite(A3, LOW);
    digitalWrite(A2, LOW);
    pumpAir = false;
  }
  
  //if no variables true then reset pumps
  else {
    digitalWrite(A3, LOW);
    digitalWrite(A2, LOW);
  }

  //code for transmitting data to client
  //snippet used is from bluerobotics ms5837 library
  sensor.read();

  //print temperature data to client console each clock cycle
  client.print("Temperature: "); 
  client.print(sensor.temperature()); 
  client.println(" deg C");
  
  //print depth data to client console every clock cycle
  client.print("Depth: "); 
  client.print(sensor.depth()); 
  client.println(" m");

  //reset client input variable
  ClientInput = "";
}