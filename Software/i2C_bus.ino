#include <Wire.h>

// I2C Sensor Addresses
#define ICM20948_ADDR      0x69
#define BNO085_ADDR        0x4A
#define BAR10_ADDR         0x76

// Analog Temperature Sensors (optional, for future hardware or testing)
#define ANALOG_TEMP_PIN_1  A0
#define ANALOG_TEMP_PIN_2  A1
#define ANALOG_TEMP_PIN_3  A2
#define ADC_REFERENCE_V    3.3f     // STM32/ESP32 reference voltage
#define ADC_MAX_COUNTS     4095.0f  // 12-bit ADC
#define TEMP_VOLT_OFFSET   0.5f     // Voltage at 0C for LM35-style sensors
#define TEMP_SCALE         100.0f   // Degrees C per volt

// Configuration
#define POLLING_PERIOD    500  // ms between sensor reads

// Sensor data array layout
// 0-2: Analog temp sensors #1-3 (°C)
// 3-5: ICM20948 accel X,Y,Z (g)
// 6-8: ICM20948 gyro X,Y,Z (°/s)
// 9-12: BNO085 quaternion W,X,Y,Z
// 13-15: BNO085 accel X,Y,Z (m/s²)
// 16: BAR10 pressure (mbar)
// 17: Depth (m)
#define SENSOR_DATA_SIZE  18

float sensorData[SENSOR_DATA_SIZE];
unsigned long lastReadTime = 0;

// BAR10 calibration PROM
uint16_t bar10_prom[8];
bool bar10_initialized = false;

float readAnalogTemperature(uint8_t pin) {
  // Read ADC value from analog pin and convert to temperature (°C)
  // Assumes LM35-style sensor: 0.5V @ 0°C, 10mV/°C
  // Formula: T = (V - 0.5V) * 100
  int rawValue = analogRead(pin);
  float voltage = (rawValue / ADC_MAX_COUNTS) * ADC_REFERENCE_V;
  return (voltage - TEMP_VOLT_OFFSET) * TEMP_SCALE;
}

// OLD HTU21D CODE (DEPRECATED - Using analog temps instead)
// The following functions are retained for reference if hardware changes back to I2C temp sensors

bool initHTU21D(uint8_t addr) {
  // DEPRECATED: Using analog temp sensors instead
  Wire.beginTransmission(addr);
  Wire.write(0xFE);  // Soft reset
  byte error = Wire.endTransmission();
  delay(15);
  
  if (error == 0) {
    Serial.print("HTU21D (0x");
    Serial.print(addr, HEX);
    Serial.println(") initialized");
    return true;
  } else {
    Serial.print("HTU21D (0x");
    Serial.print(addr, HEX);
    Serial.print(") init error: ");
    Serial.println(error);
    return false;
  }
}

float readHTU21D_Temperature(uint8_t addr) {
  // DEPRECATED: Using analog temp sensors instead
  Wire.beginTransmission(addr);
  Wire.write(0xE3);  // Trigger temperature measurement
  byte error = Wire.endTransmission();
  
  if (error != 0) return NAN;
  
  delay(50);
  Wire.requestFrom(addr, (uint8_t)2);
  if (Wire.available() == 2) {
    uint16_t raw = (Wire.read() << 8) | Wire.read();
    return -46.85 + 175.72 * (raw / 65536.0);
  }
  return NAN;
}

float readHTU21D_Humidity(uint8_t addr) {
  // DEPRECATED: Using analog temp sensors instead
  Wire.beginTransmission(addr);
  Wire.write(0xE5);  // Trigger humidity measurement
  byte error = Wire.endTransmission();
  
  if (error != 0) return NAN;
  
  delay(50);
  Wire.requestFrom(addr, (uint8_t)2);
  if (Wire.available() == 2) {
    uint16_t raw = (Wire.read() << 8) | Wire.read();
    return -6.0 + 125.0 * (raw / 65536.0);
  }
  return NAN;
}

bool initICM20948(uint8_t addr) {
  // Wake device and enable gyro/accel
  Wire.beginTransmission(addr);
  Wire.write(0x06);  // PWR_MGMT_1
  Wire.write(0x01);  // Auto clock, no sleep
  byte error = Wire.endTransmission();
  
  if (error != 0) {
    Serial.print("ICM20948 (0x");
    Serial.print(addr, HEX);
    Serial.print(") init error: ");
    Serial.println(error);
    return false;
  }
  
  delay(100);
  Serial.print("ICM20948 (0x");
  Serial.print(addr, HEX);
  Serial.println(") initialized");
  return true;
}

void readICM20948(uint8_t addr, float *ax, float *ay, float *az, float *gx, float *gy, float *gz) {
  // Read 12 bytes: accel (6) + gyro (6)
  Wire.beginTransmission(addr);
  Wire.write(0x2D);  // ACCEL_XOUT_H
  byte error = Wire.endTransmission();
  
  if (error != 0) {
    *ax = *ay = *az = *gx = *gy = *gz = NAN;
    return;
  }
  
  Wire.requestFrom(addr, (uint8_t)12);
  if (Wire.available() == 12) {
    int16_t accelX = (Wire.read() << 8) | Wire.read();
    int16_t accelY = (Wire.read() << 8) | Wire.read();
    int16_t accelZ = (Wire.read() << 8) | Wire.read();
    int16_t gyroX = (Wire.read() << 8) | Wire.read();
    int16_t gyroY = (Wire.read() << 8) | Wire.read();
    int16_t gyroZ = (Wire.read() << 8) | Wire.read();
    
    // ±2g accel, 250°/s gyro (default ranges)
    *ax = accelX / 16384.0;
    *ay = accelY / 16384.0;
    *az = accelZ / 16384.0;
    *gx = gyroX / 131.0;
    *gy = gyroY / 131.0;
    *gz = gyroZ / 131.0;
  } else {
    *ax = *ay = *az = *gx = *gy = *gz = NAN;
  }
}

bool initBNO085(uint8_t addr) {
  // BNO085 uses SHTP protocol - simplified init
  Wire.beginTransmission(addr);
  byte error = Wire.endTransmission();
  
  if (error != 0) {
    Serial.print("BNO085 (0x");
    Serial.print(addr, HEX);
    Serial.print(") init error: ");
    Serial.println(error);
    return false;
  }
  
  delay(100);
  Serial.print("BNO085 (0x");
  Serial.print(addr, HEX);
  Serial.println(") initialized");
  return true;
}

void readBNO085(uint8_t addr, float *qw, float *qx, float *qy, float *qz, float *ax, float *ay, float *az) {
  // BNO085 quaternion rotation vector report (0x05)
  // This is a simplified read - full implementation requires SHTP protocol
  Wire.beginTransmission(addr);
  Wire.write(0x00);
  byte error = Wire.endTransmission();
  
  if (error != 0) {
    *qw = *qx = *qy = *qz = *ax = *ay = *az = NAN;
    return;
  }
  
  Wire.requestFrom(addr, (uint8_t)20);
  if (Wire.available() >= 20) {
    // Skip header bytes
    Wire.read(); Wire.read(); Wire.read(); Wire.read();
    
    // Read quaternion (16-bit fixed point)
    int16_t qw_raw = (Wire.read() << 8) | Wire.read();
    int16_t qx_raw = (Wire.read() << 8) | Wire.read();
    int16_t qy_raw = (Wire.read() << 8) | Wire.read();
    int16_t qz_raw = (Wire.read() << 8) | Wire.read();
    
    // Read accel (16-bit, m/s²)
    int16_t ax_raw = (Wire.read() << 8) | Wire.read();
    int16_t ay_raw = (Wire.read() << 8) | Wire.read();
    int16_t az_raw = (Wire.read() << 8) | Wire.read();
    
    // Convert quaternion (Q14 format: divide by 16384)
    *qw = qw_raw / 16384.0;
    *qx = qx_raw / 16384.0;
    *qy = qy_raw / 16384.0;
    *qz = qz_raw / 16384.0;
    
    // Convert accel (m/s²)
    *ax = ax_raw / 100.0;
    *ay = ay_raw / 100.0;
    *az = az_raw / 100.0;
  } else {
    *qw = *qx = *qy = *qz = *ax = *ay = *az = NAN;
  }
}

// Analog Temperature Sensors (Optional)
// These are placeholders for future analog temp sensor hardware (e.g., LM35-style sensors).
// Currently unused, but included for completeness if the design reverts to analog inputs.
// To use: configure ADC_REFERENCE_V and TEMP_SCALE based on your actual sensor.

float readAnalogTemperature(uint8_t pin) {
  int rawValue = analogRead(pin);
  float voltage = rawValue * (ADC_REFERENCE_V / ADC_MAX_COUNTS);
  float temperature = (voltage - TEMP_VOLT_OFFSET) * TEMP_SCALE;
  return temperature;
}

bool initBAR10() {
  // Reset device
  Wire.beginTransmission(BAR10_ADDR);
  Wire.write(0x1E);
  byte error = Wire.endTransmission();
  delay(10);
  
  if (error != 0) {
    Serial.print("BAR10 reset error: ");
    Serial.println(error);
    return false;
  }
  
  // Read calibration PROM (8 coefficients)
  for (int i = 0; i < 8; i++) {
    Wire.beginTransmission(BAR10_ADDR);
    Wire.write(0xA0 + (i * 2));
    error = Wire.endTransmission();
    
    if (error != 0) {
      Serial.print("BAR10 PROM error: ");
      Serial.println(i);
      return false;
    }
    
    Wire.requestFrom(BAR10_ADDR, (uint8_t)2);
    if (Wire.available() == 2) {
      bar10_prom[i] = (Wire.read() << 8) | Wire.read();
    }
  }
  
  bar10_initialized = true;
  Serial.println("BAR10 initialized");
  return true;
}

float readBAR10Pressure() {
  if (!bar10_initialized) return NAN;
  
  // Trigger pressure conversion (OSR 4096)
  Wire.beginTransmission(BAR10_ADDR);
  Wire.write(0x48);
  byte error = Wire.endTransmission();
  if (error != 0) return NAN;
  delay(10);
  
  // Read pressure ADC
  Wire.beginTransmission(BAR10_ADDR);
  Wire.write(0x00);
  error = Wire.endTransmission();
  if (error != 0) return NAN;
  
  Wire.requestFrom(BAR10_ADDR, (uint8_t)3);
  if (Wire.available() != 3) return NAN;
  uint32_t D1 = ((uint32_t)Wire.read() << 16) | ((uint32_t)Wire.read() << 8) | Wire.read();
  
  // Trigger temperature conversion
  Wire.beginTransmission(BAR10_ADDR);
  Wire.write(0x58);
  Wire.endTransmission();
  delay(10);
  
  // Read temperature ADC
  Wire.beginTransmission(BAR10_ADDR);
  Wire.write(0x00);
  Wire.endTransmission();
  
  Wire.requestFrom(BAR10_ADDR, (uint8_t)3);
  if (Wire.available() != 3) return NAN;
  uint32_t D2 = ((uint32_t)Wire.read() << 16) | ((uint32_t)Wire.read() << 8) | Wire.read();
  
  // MS5837 conversion algorithm
  int32_t dT = D2 - ((uint32_t)bar10_prom[5] << 8);
  int32_t TEMP = 2000 + ((int64_t)dT * bar10_prom[6] >> 23);
  int64_t OFF = ((int64_t)bar10_prom[2] << 16) + ((int64_t)dT * bar10_prom[4] >> 7);
  int64_t SENS = ((int64_t)bar10_prom[1] << 15) + ((int64_t)dT * bar10_prom[3] >> 8);
  int32_t P = ((D1 * SENS >> 21) - OFF) >> 15;
  
  return P / 100.0;  // Convert to mbar
}

float calculateDepth(float pressureMbar) {
  if (isnan(pressureMbar)) return NAN;
  float pressurePa = pressureMbar * 100.0;
  float freshwaterDensity = 1000.0;  // kg/m^3
  float gravity = 9.80665;
  float atmosphericPressure = 101325.0;  // Pa
  return (pressurePa - atmosphericPressure) / (freshwaterDensity * gravity);
}

void sendDataToJetson() {
  Serial.print("{\"ts\":");
  Serial.print(millis());
  
  // Analog temperature sensors
  Serial.print(",\"temp1_c\":");
  Serial.print(sensorData[0], 2);
  Serial.print(",\"temp2_c\":");
  Serial.print(sensorData[1], 2);
  Serial.print(",\"temp3_c\":");
  Serial.print(sensorData[2], 2);
  
  // ICM20948 accel & gyro
  Serial.print(",\"icm_ax_g\":");
  Serial.print(sensorData[3], 3);
  Serial.print(",\"icm_ay_g\":");
  Serial.print(sensorData[4], 3);
  Serial.print(",\"icm_az_g\":");
  Serial.print(sensorData[5], 3);
  Serial.print(",\"icm_gx_dps\":");
  Serial.print(sensorData[6], 2);
  Serial.print(",\"icm_gy_dps\":");
  Serial.print(sensorData[7], 2);
  Serial.print(",\"icm_gz_dps\":");
  Serial.print(sensorData[8], 2);
  
  // BNO085 quaternion & accel
  Serial.print(",\"bno_qw\":");
  Serial.print(sensorData[9], 4);
  Serial.print(",\"bno_qx\":");
  Serial.print(sensorData[10], 4);
  Serial.print(",\"bno_qy\":");
  Serial.print(sensorData[11], 4);
  Serial.print(",\"bno_qz\":");
  Serial.print(sensorData[12], 4);
  Serial.print(",\"bno_ax_ms2\":");
  Serial.print(sensorData[13], 2);
  Serial.print(",\"bno_ay_ms2\":");
  Serial.print(sensorData[14], 2);
  Serial.print(",\"bno_az_ms2\":");
  Serial.print(sensorData[15], 2);
  
  // BAR10 pressure & depth
  Serial.print(",\"pressure_mbar\":");
  Serial.print(sensorData[16], 2);
  Serial.print(",\"depth_m\":");
  Serial.print(sensorData[17], 3);
  
  Serial.println("}");
}

void setup() {
  // Configure analog inputs for temperature sensors
  pinMode(ANALOG_TEMP_PIN_1, INPUT);
  pinMode(ANALOG_TEMP_PIN_2, INPUT);
  pinMode(ANALOG_TEMP_PIN_3, INPUT);
  
  Wire.begin();
  Serial.begin(115200);
  
  while (!Serial) {
    delay(10);
  }
  
  delay(1000);
  Serial.println("\nInitializing sensors...\n");
  
  Serial.println("Analog Temp Sensors: READY (ADC pins A0/A1/A2)");
  
  bool icm_ok = initICM20948(ICM20948_ADDR);
  bool bno_ok = initBNO085(BNO085_ADDR);
  bool bar_ok = initBAR10();
  
  Serial.println("\nInitialization complete.");
  Serial.print("ICM20948: ");
  Serial.println(icm_ok ? "OK" : "FAILED");
  Serial.print("BNO085: ");
  Serial.println(bno_ok ? "OK" : "FAILED");
  Serial.print("BAR10: ");
  Serial.println(bar_ok ? "OK" : "FAILED");
  Serial.print("\nPolling every ");
  Serial.print(POLLING_PERIOD);
  Serial.println(" ms\n");
  
  lastReadTime = millis();
}

void loop() {
  unsigned long currentTime = millis();
  if (currentTime - lastReadTime < POLLING_PERIOD) {
    delay(10);
    return;
  }
  
  lastReadTime = currentTime;
  
  // Read analog temperature sensors
  sensorData[0] = readAnalogTemperature(ANALOG_TEMP_PIN_1);
  sensorData[1] = readAnalogTemperature(ANALOG_TEMP_PIN_2);
  sensorData[2] = readAnalogTemperature(ANALOG_TEMP_PIN_3);
  
  // Read ICM20948 (accel + gyro)
  readICM20948(ICM20948_ADDR, &sensorData[3], &sensorData[4], &sensorData[5],
               &sensorData[6], &sensorData[7], &sensorData[8]);
  
  // Read BNO085 (quaternion + accel)
  readBNO085(BNO085_ADDR, &sensorData[9], &sensorData[10], &sensorData[11], &sensorData[12],
             &sensorData[13], &sensorData[14], &sensorData[15]);
  
  // Read pressure sensor and calculate depth
  sensorData[16] = readBAR10Pressure();
  sensorData[17] = calculateDepth(sensorData[16]);
  
  // Send to Jetson
  sendDataToJetson();
}

