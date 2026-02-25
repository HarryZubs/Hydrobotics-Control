#include <InterruptEncoder.h>
#include <Wire.h>
#include <WiFi.h>

//wifi stuff
const char* ssid = "hydroboticsDataTransmission";
const char* password = "controlfloat";

WiFiServer server(1234);

//load encoder library and create new encoder class
InterruptEncoder encoder;

//initialise empty variables to be used
unsigned long timeElapsed = 0;
unsigned long previousTime = 0;
float currentTicks = 0;
float previousTicks = 0;
float deltaTime = 0;
float deltaTicks = 0;

//PID constants
float propGain = 5;
float derivativeGain = 1;
float integralGain = 0.1;

//depth sensor initialisations
#define SENSOR_ADDR 0x76
#define CMD_RESET 0x1E
#define CMD_CONVERT_D1 0x48
#define CMD_CONVERT_D2 0x58
#define CMD_ADC_READ 0x00
uint16_t C[7];

//initialise empty PID variables
float error; 
float previousError; 
float integral;
float derivative; 
float output;
float targetDepth = 0.0f; 
static unsigned long lastPrint = 0;

// the setup routine runs once when you press reset:

void readPROM() {
  for (uint8_t i = 0; i < 7; i++) {
    Wire.beginTransmission(SENSOR_ADDR);
    Wire.write(0xA0 + (i * 2));
    Wire.endTransmission();

    Wire.requestFrom(SENSOR_ADDR, 2);
    C[i]  = Wire.read() << 8;
    C[i] |= Wire.read();
  }
}


void setup() {
// initialize serial communication at 9600 bits per second:
  pinMode(D3, OUTPUT);
  pinMode(D4, OUTPUT);
  pinMode(A3, INPUT);
  pinMode(A4, INPUT);
  encoder.attach(D9, D10);
  Serial.begin(115200);

  //start sensor 
  Wire.begin();

  // reset sensor
  Wire.beginTransmission(SENSOR_ADDR);
  Wire.write(CMD_RESET);
  Wire.endTransmission();

  //Wifi network start
  WiFi.mode(WIFI_AP);
  server.begin();
  Wire.begin();

  bool ok = WiFi.softAP(ssid, password);
  
  delay(10);

  readPROM();

  Serial.println("PROM:");
  for (int i=0; i<7; i++) Serial.println(C[i]);

  if (ok) {
    Serial.println("Network started");
    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("failed");
  }

  float targetDepth = 5.0f; 
}

uint32_t readADC() {
  Wire.beginTransmission(SENSOR_ADDR);
  Wire.write(CMD_ADC_READ);
  Wire.endTransmission();

  Wire.requestFrom(SENSOR_ADDR, 3);
  uint32_t value = 0;
  value = Wire.read() << 16;
  value |= Wire.read() << 8;
  value |= Wire.read();
  return value;
}

// the loop routine runs over and over again forever:

void loop() {
  //initialise wifi network
  WiFiClient client = server.available();

    //depth sensor stuff
  // start pressure conversion
  Wire.beginTransmission(SENSOR_ADDR);
  Wire.write(CMD_CONVERT_D1);
  Wire.endTransmission();

  uint32_t D1 = readADC();

  // start temperature conversion
  Wire.beginTransmission(SENSOR_ADDR);
  Wire.write(CMD_CONVERT_D2);
  Wire.endTransmission();

  uint32_t D2 = readADC();

  int32_t dT = D2 - (uint32_t)C[5] * 256;
  int32_t TEMP = 2000 + (int64_t)dT * C[6] / 8388608;

  int64_t OFF  = (int64_t)C[2] * 65536 + ((int64_t)C[4] * dT) / 128;
  int64_t SENS = (int64_t)C[1] * 32768 + ((int64_t)C[3] * dT) / 256;

  int32_t Praw = (((D1 * SENS) / 2097152) - OFF) / 32768;

  // ------------------------------
  // FIX: MS5803 pressure scaling
  // ------------------------------

  // Final pressure in mbar (no division)
  float pressure_mbar = Praw;

  float temperature_C = TEMP / 100.0f;

  // Depth formula (seawater density)
  float depth_m = (pressure_mbar - 1013.25f) / (1029.0f / 10.0f);
  
  //calculate time elapsed in previous clock cycle, calculate time elapsed in current clock cycle and find the difference in seconds
  previousTime = timeElapsed;
  timeElapsed = millis();
  deltaTime = (timeElapsed - previousTime)/1000.0;

  //get ticks from previous cycle, read current ticks and find the difference between the current and previous ticks
  previousTicks = currentTicks;
  currentTicks = encoder.read();
  deltaTicks = currentTicks - previousTicks;
  
  //assign error to error from previous cycle and overwrite the error variable with current error
  previousError = error;
  error = depth_m - targetDepth;

  //prevent change in time from being zero to ensure derivative is always a real value
  if (deltaTime <= 0) {
    deltaTime = 0.01;
  }

  //calculate derivative of PID by dividing change in error by change in time
  derivative = (error - previousError) / deltaTime;
  //calculate integral by summing error multiplied by time
  integral += error * deltaTime;
  //limit integral values between 1000 and -1000 to prevent excess adjustments leading to the motor spinning in one direction
  integral = constrain(integral, -1000, 1000);

  //calculate output from previous results and constants defined at the start of code
  output = error * propGain + derivative * derivativeGain + integral * integralGain;
  //limit output to value between 255 and -255 to prevent overcorrection
  output = constrain(output, -255, 255);
  digitalWrite(D4, HIGH);
  digitalWrite(D3, LOW);


  if (timeElapsed - lastPrint >= 250) {
    //Serial.print(currentTicks);
    //Serial.print(",");
    //Serial.println(targetDepth);
    lastPrint = timeElapsed;
  }
  float rpm = (deltaTicks/4095)*(1/(deltaTime)) * 60;

  // Accept new client
  if (!client || !client.connected()) {
      client = server.available();
  }

  // If client is connected, send data every loop cycle
  if (client && client.connected()) {
      client.println(temperature_C);   // send data every loop
      Serial.println(temperature_C);
  }
}