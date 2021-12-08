#include <Wire.h>
#include <stdio.h>
 
// SI7021 addr is 0x40
#define TempHum 0x40
// defining pins for the relay board
#define MISTSOL 40
#define FANSOL 39
#define IRGSOL 38

const unsigned long check_interval = 5000;
unsigned long lastcheck = 0;
// check for water content interval, in millis
const unsigned long water_interval = 30000; //21600000, 120000
unsigned long lastwater = 0;


//sensor datatype
struct Sensors{
  int temp;
  int humidity;
  int moisture;
};

//photon config
const size_t READ_BUF_SIZE = 256;
char sendBuf[256];
char readBuf[READ_BUF_SIZE];
size_t readBufOffset = 0;

void sendSensorData(const Sensors &data);


//function to grab data from all sensors
struct Sensors GetData();
//global for currentdata and targetdata
struct Sensors CurrentData;
struct Sensors TargetData;
//constants for algorithm control
int FanState = 0;
int SMState = 0;
int MistState = 0;

int Tempthresh = 1;
int emergency = 0;
int SMthresh = 5;
int watered = -1;


//control functions
void Misters(int val);
void Irrigation(int val);
void Fan(int val);

void setup() {
  //debug led's
   pinMode(RED_LED, OUTPUT);
   pinMode(GREEN_LED, OUTPUT);
   pinMode(BLUE_LED, OUTPUT);

   digitalWrite(RED_LED, LOW);
   digitalWrite(GREEN_LED, LOW);
   digitalWrite(BLUE_LED, LOW);
  
  // relay config
   pinMode(MISTSOL, OUTPUT);
   pinMode(IRGSOL, OUTPUT);
   pinMode(FANSOL, OUTPUT);

  //temperature/humidity sensor config
  Wire.begin();
  Wire.beginTransmission(TempHum);
  Wire.endTransmission();

  //photon connection config, serial prints to consol, serial1 prints to the uart wires
  Serial.begin(9600);
  Serial1.begin(19200);

  //Set fans/misters/irrigation to off 
  Misters(0);
  Irrigation(0);
  Fan(0);
  //default target values
  TargetData.humidity = 50;
  TargetData.temp = 25;
  TargetData.moisture = 1000;
}

void loop() {
  //grab sensor data
  CurrentData = GetData();

  
  // Read data from serial
  Serial.print("getting photon \n");
  while(Serial1.available()) {
    if (readBufOffset < READ_BUF_SIZE) {
      char c = Serial1.read();
      if (c != '\n') {
        // Add character to buffer
        readBuf[readBufOffset++] = c;
      }
      else {
        // End of line character found, process line
        readBuf[readBufOffset] = 0;
        readBufOffset = 0;
        sendSensorData(CurrentData);
      }
    }
    else {
      Serial.println("readBuf overflow, emptying buffer");
      readBufOffset = 0;
    }
  }
  Serial.print("end photon \n");

  if (abs(millis() - lastcheck) >= check_interval) {
    lastcheck = millis();
    

    if(CurrentData.temp - TargetData.temp >= Tempthresh+4){
    emergency = 1;
    FanState = 1;
    }
  else if(CurrentData.temp - TargetData.temp >= Tempthresh){
    emergency = 0;
    FanState = 1;
    }
  else{
    emergency = 0;
    FanState = 0;
    }

   if(TargetData.humidity - CurrentData.humidity >= 2){
    MistState = 1;
   }
   else{
    MistState = 0;
   }

  }

  
  //waters every water_interval, once threshold is met once it will wait till the next interval to water again. 
  if (abs(millis() - lastwater) >= water_interval) {
    Serial.println(abs(millis() - lastwater));
    lastwater = millis();
    watered = 0;
  }
  
  if( watered == 0){
    if(CurrentData.moisture >= TargetData.moisture){
      SMState = 1;
    }
    else{
      SMState = 0;
    }
  }

  if(CurrentData.moisture < TargetData.moisture){
    watered = 1;
  }
  
//controlling the environment actors
  if (FanState == 1 || emergency == 1){
    Fan(1);
  }
  else{
    Fan(0);
  }
  //MistState == 1 || 
  if(MistState == 1 || emergency == 1){
    Misters(1); 
  }
  else{
    Misters(0);
  }
  //SMState == 1 && 
  if (SMState == 1){
    Irrigation(1);
  }
  else{
    Irrigation(0);
  }
  
  

  //Serial.print("humidity - ");
  //Serial.print((int)CurrentData.humidity);
  //Serial.println(" %");
  //Serial.print("temperature - ");
  //Serial.print(CurrentData.temp);
  //Serial.println(" C");
  Serial.print(" Watered: ");
  Serial.println(watered);
  delay(100);
  
}
//sending sensor data
void sendSensorData(const Sensors &data) {
  if (sscanf(readBuf, "%d,%d,%d", &TargetData.temp, &TargetData.humidity, &TargetData.moisture) == 3) {
      snprintf(sendBuf, sizeof(sendBuf), "%d,%d,%d\n",
      data.temp, data.humidity, data.moisture);
  Serial1.print(sendBuf);
  Serial.print("sent buf \n");
  }
}

struct Sensors GetData(){
  Serial.print("getting sensors");
  Serial.print("\n");

  struct Sensors Sensordata;

  Sensordata.moisture = analogRead(A3);

  unsigned int humdata[2];
  unsigned int tempdata[2];
 //getting humidity data
 Serial.print("start hum transmission \n");
  Wire.beginTransmission(TempHum);
  Wire.write(0xF5);
  Wire.endTransmission();
  delay(50);
  Serial.print("end hum transmission \n");

  Serial.print("Start hum data \n");
  Wire.requestFrom(TempHum, 2);
  delay(50);
  if(Wire.available() == 2)
  {
    humdata[0] = Wire.read();
    humdata[1] = Wire.read();
  }
  else{
    Serial.print("hum read error \n");
  }
  Serial.print("End hum data \n");
  
  Sensordata.humidity  = (int)(125 *((humdata[0] * 256.0) + humdata[1])/65536.0)-6;

Serial.print("start temp transmission \n");
  //getting temperature data
  Wire.beginTransmission(TempHum);
  Wire.write(0xF3);
  Wire.endTransmission();
  delay(50);
  Serial.print("end temp transmission \n");

  Serial.print("Start temp data \n");
  Wire.requestFrom(TempHum, 2);
  delay(50);
  if(Wire.available() == 2)
  {
    tempdata[0] = Wire.read();
    tempdata[1] = Wire.read();
  }
  else{
    Serial.print("temp read error \n");
  }
  Serial.print("End temp data \n");

  Sensordata.temp  = (int)((175.72*((tempdata[0] * 256.0) + tempdata[1]))/65536.0)-46.85;

  Serial.print("got sensors");
  Serial.print("\n");

  return Sensordata;
  }

//control functions
  void Misters(int val){
    if (val == 1 ){
      digitalWrite(MISTSOL, LOW);
      digitalWrite(RED_LED, HIGH);
    }
    if (val == 0 ){
      digitalWrite(MISTSOL, HIGH);
      digitalWrite(RED_LED, LOW);
    }
  }
  void Irrigation(int val){
    if (val == 1 ){
      digitalWrite(IRGSOL, LOW);
      digitalWrite(GREEN_LED, HIGH);
    }
    if (val == 0 ){
      digitalWrite(IRGSOL, HIGH);
      digitalWrite(GREEN_LED, LOW);
    }
  }
  void Fan(int val){
    if (val == 1 ){
      digitalWrite(FANSOL, LOW);
      digitalWrite(BLUE_LED, HIGH);
    }
    if (val == 0 ){
      digitalWrite(FANSOL, HIGH);
      digitalWrite(BLUE_LED, LOW);
    }
  }
