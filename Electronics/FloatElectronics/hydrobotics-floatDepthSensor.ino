#include <Wire.h>

#define SENSOR_ADDR 0x76
#define CMD_RESET 0x1E
#define CMD_CONVERT_D1 0x48
#define CMD_CONVERT_D2 0x58
#define CMD_ADC_READ 0x00

uint16_t C[7];

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
  Serial.begin(115200);
  Wire.begin();

  // reset sensor
  Wire.beginTransmission(SENSOR_ADDR);
  Wire.write(CMD_RESET);
  Wire.endTransmission();
  
  delay(10);

  readPROM();

  Serial.println("PROM:");
  for (int i=0; i<7; i++) Serial.println(C[i]);
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

void loop() {
  // start pressure conversion
  Wire.beginTransmission(SENSOR_ADDR);
  Wire.write(CMD_CONVERT_D1);
  Wire.endTransmission();
  delay(10);

  uint32_t D1 = readADC();

  // start temperature conversion
  Wire.beginTransmission(SENSOR_ADDR);
  Wire.write(CMD_CONVERT_D2);
  Wire.endTransmission();
  delay(10);

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

  Serial.println(temperature_C);
  Serial.println(pressure_mbar);
  Serial.println(depth_m);

  delay(1000);
}