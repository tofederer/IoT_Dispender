#include <WiFi.h>
#include <Wire.h>
#include "Esp32MQTTClient.h"

/***** Device Configuration *****/
// Please input the WiFi SSID and password
const char* ssid     = "SSID";
const char* password = "password";

/*String containing Hostname, Device Id & Device Key in the format:                         */
/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessKey=<device_key>"                */
/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessSignature=<device_sas_token>"    */
static const char* connectionString = "***";

// Generel Configuration
int sendDataInterval = 200;
int pumpRunTime = 2000;

// Liqud level calibration
const int minLiquidLevel = 3500;
const int maxLiquidLevel = 300;

// Ultrasonicsensor calibration
const int aktivatePumpDistance = 20;

/***** Variables *****/ 
static bool hasIoTHub = false;
char* errorMessage = "";
int batteryLevel = 100;
int releasesSizeLastSend = 0;
int restTime = 0;

// Pins
const int ledStatePin = 21;
const int pumpPin = 12;

// Ultrasonicsensor
unsigned char txbuf[10] = {0};
unsigned char rxbuf[10] = {0};
typedef enum {
  SLAVEADDR_INDEX = 0,
  PID_INDEX,
  VERSION_INDEX ,
  DIST_H_INDEX,
  DIST_L_INDEX,
  TEMP_H_INDEX,
  TEMP_L_INDEX,
  CFG_INDEX,
  CMD_INDEX,
  REG_NUM
} regindexTypedef;
#define    MEASURE_MODE_PASSIVE    (0x00)
#define    MEASURE_RANG_500        (0x20)
#define    CMD_DISTANCE_MEASURE    (0x01)
unsigned char addr0 = 0x11;
unsigned char i = 0, x = 0;

// Setup
void setup() {
  Serial.begin(9600);

  restTime = sendDataInterval;
  pinMode(ledStatePin, OUTPUT);
  digitalWrite(ledStatePin, HIGH);
  pinMode(pumpPin, OUTPUT);

  Wire.begin();
  txbuf[0] =  (MEASURE_MODE_PASSIVE | MEASURE_RANG_500);//the measurement mode is set to passive mode, measurement range is set to 500CM.
  i2cWriteBytes(addr0, CFG_INDEX , &txbuf[0], 1 );
  delay(100);
  
  // Connect WiFi  
  if (!connectWiFi(10)) {
    Serial.println("Error WiFi");
    errorMessage = "Error WiFi";
  }

  // Connect Azure
  if (!connectAzure()) {
    Serial.println("Error Azure");
    errorMessage = "Error Azure";
  }
}

// Main Program
void loop() {

  if (readUltrasonic() < aktivatePumpDistance) {
    digitalWrite(pumpPin, HIGH);
    releasesSizeLastSend++;
    delay(pumpRunTime);  
    digitalWrite(pumpPin, LOW);
    restTime = restTime - (pumpRunTime/100);
    delay(1000);
  }

  if (restTime <= 0) {
    if (sendData()) {
      restTime = sendDataInterval;
      releasesSizeLastSend = 0;
    }
  }
 restTime--;
}


// Connect WiFi
bool connectWiFi(int retries) { 
  
  Serial.println("Starting connecting WiFi.");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    retries--;
    if (retries <= 0) {
      return false;
    }
  }
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  return true;
}

// Connect Azure
bool connectAzure () {
  if (!Esp32MQTTClient_Init((const uint8_t*)connectionString))
  {
    hasIoTHub = false;
    Serial.println("Initializing IoT hub failed.");
    return false;
  }
  hasIoTHub = true;
  return true;
}

// Send data to Azure
bool sendData () {
  Serial.println("start sending events.");
  if (hasIoTHub)
  {
    char buff[128];
    
    // replace the following line with your data sent to Azure IoTHub
    snprintf(buff, 128, "{\"liquidlevel\":\"%d\",\"batterylevel\":\"%d\",\"releases\":\"%d\"}",readLiquid(),readBattery(),releasesSizeLastSend);
    
    if (Esp32MQTTClient_SendEvent(buff))
    {
      Serial.println("Sending data succeed");
      return true;
    }
    else
    {
      Serial.println("Failure...");
    }
  }
  return false;
}

// Read ultrasonicsensor
int16_t readUltrasonic () {
  
    int16_t  dist, temp;
    txbuf[0] = CMD_DISTANCE_MEASURE;
  
    i2cWriteBytes(addr0, CMD_INDEX , &txbuf[0], 1 );//write register, send ranging command
    delay(100);
  
    i2cReadBytes(addr0, DIST_H_INDEX , 2 );//read distance register
    dist = ((uint16_t)rxbuf[0] << 8) + rxbuf[1];
  
    i2cReadBytes(addr0, TEMP_H_INDEX , 2 );//read temperature register
    temp = ((uint16_t)rxbuf[0] << 8) + rxbuf[1];
    
    return dist;
}

// Read simulated battery 
int readBattery () {

  // Prototyp has no real battery
  if(random(0,100) < 5) {
    batteryLevel--;
  }

  if (batteryLevel < 0) {
    batteryLevel = 0;
  }
  return batteryLevel;
}

// Read liquidlevel
int readLiquid () {
  int liquidLevel = (((-100.000)/(minLiquidLevel-maxLiquidLevel)*analogRead(A3))+((minLiquidLevel*100.000)/(minLiquidLevel-maxLiquidLevel)));
  if(liquidLevel < 0){
    liquidLevel = 0;
  }
  return liquidLevel;
}

// For ultrasonicsensor
void i2cWriteBytes(unsigned char addr_t, unsigned char Reg , unsigned char *pdata, unsigned char datalen )
{
  Wire.beginTransmission(addr_t); // transmit to device
  Wire.write(Reg);              // sends one byte

  for (uint8_t i = 0; i < datalen; i++) {
    Wire.write(*pdata);
    pdata++;
  }
  Wire.endTransmission();    // stop transmitting
}

// For ultrasonicsensor
void i2cReadBytes(unsigned char addr_t, unsigned char Reg , unsigned char Num )
{
  unsigned char i = 0;
  Wire.beginTransmission(addr_t); // transmit to device
  Wire.write(Reg);              // sends one byte
  Wire.endTransmission();    // stop transmitting
  Wire.requestFrom(addr_t, Num);
  while (Wire.available())   // slave may send less than requested
  {
    rxbuf[i] = Wire.read();
    i++;
  }
}
