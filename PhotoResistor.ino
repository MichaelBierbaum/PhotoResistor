#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "credentials.h"
#include <string>
using std::string;

int sensorPin = A0;    // select the input pin for the potentiometer
int sensorValue = 0;  // variable to store the value coming from the sensor

const bool printToDebug = false;

#define ZEITZONE +2
#define NTP_ADDRESS  "de.pool.ntp.org"
#define NTP_OFFSET   (ZEITZONE*60*60)// In seconds     hh*60*60
#define NTP_INTERVAL 60 * 1000    // In miliseconds
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);
struct ValueTimeTuple{
  bool isShining;
  int hours;
  int minutes;
  int seconds;
};
ValueTimeTuple oldTuple, newTuple;
long totalTimeOfSunInSeconds = 0;
char totalTimeOfSunAsString[9];//"00:00:00";

void setup() {
  Serial.begin(115200);Serial.println();
  initWifi();
  initTuples();
  timeClient.begin();  
}

void loop() {
  // read the value from the sensor:
  // max Value is 1024 (full light)
  sensorValue = analogRead(sensorPin);

  newTuple.isShining = isShining(sensorValue);
  if(newTuple.isShining != oldTuple.isShining){
    updateTotalTimeOfSun();
  }

  // stop the program for <sensorValue> milliseconds:
  delay(sensorValue);
}

void initWifi(){
  WiFi.begin(ssid, password);
  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.print ( "." );
  }
  Serial.println(WiFi.localIP());
}

void initTuples(){
  oldTuple.isShining = false;
  oldTuple.hours = oldTuple.minutes = oldTuple.seconds = 0;
  newTuple = oldTuple;
}

void updateTotalTimeOfSun(){
  setTimeInTuple(newTuple);
  printTuples();
  
  if(printToDebug){
    Serial.print("totalTime before: "); Serial.println(totalTimeOfSunInSeconds);
  }
  totalTimeOfSunInSeconds += calculateSunTimeInSeconds();
  totalTimeOfSunSecondsToString();
  if(printToDebug){
    Serial.print("totalTime  after: "); Serial.println(totalTimeOfSunInSeconds);
  }

  oldTuple = newTuple;
  Serial.println("new total time of sun: "); 
  Serial.printf("\tin seconds: %i\n", totalTimeOfSunInSeconds);
  Serial.printf("\tas string : %s\n", totalTimeOfSunAsString);
}

void setTimeInTuple(ValueTimeTuple &tuple){
  timeClient.update();
  tuple.hours = timeClient.getHours();
  tuple.minutes = timeClient.getMinutes();
  tuple.seconds = timeClient.getSeconds();
}

long calculateSunTimeInSeconds(){
  if(oldTuple.isShining == false)
    return 0;
  if(printToDebug){
    Serial.println("calculateSunTimeInSeconds");
    printTuples();
  }
  long totalSeconds 
    = newTuple.hours  *60*60
    + newTuple.minutes*60
    + newTuple.seconds
    - oldTuple.hours  *60*60
    - oldTuple.minutes*60
    - oldTuple.seconds;
  
  if(printToDebug)
    Serial.printf("totalSeconds: %i\n", totalSeconds);

  return totalSeconds;
}

void printTuples(){
  printTuple(oldTuple);
  printTuple(newTuple);
}

void printTuple(ValueTimeTuple tuple){
  Serial.printf("%20s: %i\n", "The sun is shining", tuple.isShining);

  char buff[] = "00:00:00";
  sprintf(buff, "%02i:%02i:%02i", tuple.hours, tuple.minutes, tuple.seconds); 
  Serial.printf("%20s: %s\n", "Time", buff);
}

char* totalTimeOfSunSecondsToString(){
  long totalSeconds = totalTimeOfSunInSeconds;

  if(printToDebug){
    Serial.println("totalTimeOfSunSecondsToString");
    Serial.printf("totalSeconds: %i\n", totalSeconds);
  }
  
  long difHours = totalSeconds / (60*60);
  totalSeconds = totalSeconds - difHours * (60*60);
  if(printToDebug){
    Serial.printf("difHours: %i\n", difHours);
    Serial.printf("totalSeconds: %i\n", totalSeconds);
  }
  
  long difMinutes = totalSeconds / 60;
  totalSeconds = totalSeconds - difMinutes*60;
  if(printToDebug){
    Serial.printf("difMinutes: %i\n", difMinutes);
    Serial.printf("totalSeconds: %i\n", totalSeconds);
  }

  sprintf(totalTimeOfSunAsString, "%02i:%02i:%02i", difHours, difMinutes, totalSeconds);

  return totalTimeOfSunAsString;
}

/// @brief is the sun is shining?
/// @param value 
/// @return true, if value is big enough
bool isShining(int value){
    if(value > 1000)
        return true;
    else
        return false;
}