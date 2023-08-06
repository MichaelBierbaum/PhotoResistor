#include <NTPClient.h> //https://github.com/arduino-libraries/NTPClient
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include "credentials.h"
#include <string>
#include <vector>

#define MAX_BUFF 200
char buff[MAX_BUFF];

IPAddress ip;

// Create an instance of the server
// specify the port to listen on as an argument
WiFiServer server(80);
WiFiClient client;

const int sensorValueSunIsShining = 600; //1000
int sensorPin = A0;    // select the input pin for the potentiometer
int sensorValue = 0;  // variable to store the value coming from the sensor
const bool printToDebug_Converting = false;
const bool printToDebug_loadFromEeprom = false;
const bool printToDebug_handleClient = false;
const bool printToDebug_updateTotalTimeOfSun = false;
const bool printToDebug_calculateSunTimeInSeconds = false;
const bool printToDebug_getIndexFromRequest = true;

#define ZEITZONE +2
#define NTP_ADDRESS  "de.pool.ntp.org"
#define NTP_OFFSET   (ZEITZONE*60*60)// In seconds     hh*60*60
#define NTP_INTERVAL 60 * 1000    // In miliseconds
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);
time_t epochTime;
struct ValueTimeTuple{
  bool isShining;
  int hours;
  int minutes;
  int seconds;
  int day;
  int month;
  int year;
};
ValueTimeTuple oldTuple, newTuple, eepromTuple, tempTuple;
long totalTimeOfSunInSeconds = 0;
char totalTimeOfSunAsString[] = "00:00:00";
char tempTimeString[] = "00:00:00";

/*  ---------------------------------------------------------------------------------------------
    Converting Options:
    convertTotalTimeOfSunAsSecondsToTuple : totalTimeOfSunInSeconds --> eepromTuple
    convertTotalTimeOfSunAsTupleToString  : eepromTuple             --> totalTimeOfSunAsString
    convertTotalTimeOfSunAsStringToSeconds: totalTimeOfSunAsString  --> totalTimeOfSunInSeconds
    convertTotalTimeOfSunAsSecondsToString: totalTimeOfSunInSeconds --> totalTimeOfSunAsString
    ---------------------------------------------------------------------------------------------*/

#define MAX_SIZE_EEPROM 512
std::vector <std::string> datensaetze;
int  eepromAddress;
int maxAnzahlDatensaetze = 0;
std::string message;

// #define GREY #C0C0C0
// #define GREEN #A1FB8E
// #define RED  #F08784
// #define YELLOW #FFFE91
// #define BLUE #3282F6
// #define DARK_BLUE #00063D

void printTuple(ValueTimeTuple tuple){
  Serial.println("\n---------------\nprintTuple\n---------------");
  Serial.printf("%20s: %i\n", "The sun is shining", tuple.isShining);

  char buff[] = "00:00:00";
  sprintf(buff, "%02i:%02i:%02i", tuple.hours, tuple.minutes, tuple.seconds);  
  Serial.printf("%20s: %s\n", "Time", buff);
  sprintf(buff, "%02i_%02i_%02i", tuple.day, tuple.month, tuple.year);
  Serial.printf("%20s: %s\n", "Date", buff);
}

void printTuples(){
  printTuple(oldTuple);
  printTuple(newTuple);
}

/// @brief convert totalTimeOfSun as long --> tuple
/// @param tuple 
ValueTimeTuple convertTotalTimeOfSunAsSecondsToTuple(){
  long totalSeconds = totalTimeOfSunInSeconds;

  if(printToDebug_Converting){
    Serial.println("totalTimeOfSunInSecondsToTuple");
    Serial.printf("totalSeconds: %i\n", totalSeconds);
  }
  
  long difHours = totalSeconds / (60*60);
  totalSeconds = totalSeconds - difHours * (60*60);
  if(printToDebug_Converting){
    Serial.printf("difHours: %i\n", difHours);
    Serial.printf("totalSeconds: %i\n", totalSeconds);
  }
  
  long difMinutes = totalSeconds / 60;
  totalSeconds = totalSeconds - difMinutes*60;
  if(printToDebug_Converting){
    Serial.printf("difMinutes: %i\n", difMinutes);
    Serial.printf("totalSeconds: %i\n", totalSeconds);
  }
  
  tempTuple.hours = difHours;
  tempTuple.minutes = difMinutes;
  tempTuple.seconds = totalSeconds;
  return tempTuple;
}

/// @brief convert totalTimeOfSun as tuple --> "hh:mm:ss"
/// @param tuple 
void convertTotalTimeOfSunAsTupleToString(ValueTimeTuple tuple, char* timeString){
  sprintf(timeString, "%02i:%02i:%02i", tuple.hours, tuple.minutes, tuple.seconds);
}

char* convertTotalTimeOfSunAsSecondsToString(){
  tempTuple = convertTotalTimeOfSunAsSecondsToTuple(); //long --> tuple
  convertTotalTimeOfSunAsTupleToString(tempTuple, totalTimeOfSunAsString); //tuple --> "hh:mm:ss"

  return totalTimeOfSunAsString;
}

/// @brief converting totalTimeOfSun as "hh:mm:ss" --> long totalSeconds
/// @param totalTime: "hh:mm:ss" 
/// @return return the total time in seconds
long convertTotalTimeOfSunAsStringToSeconds(const char* totalTime){
  long h = 0, m = 0, s = 0;
  if (sscanf(totalTime, "%d:%d:%d", &h, &m, &s) >= 2)
  {
    long totalSeconds = h *3600 + m*60 + s;
    return totalSeconds;
  }
  return 0;
}

//https://stackoverflow.com/questions/63517768/esp32-writing-string-to-eeprom

/// @brief calculate the proper address for EEPROM from index
/// @param index 0 <= index < maxAnzahlDatensaetze
/// @return 0 <= addr < MAX_SIZE_EEPROM
int getAddrFromIndex(int index = -1){
  if(index < 0 || index >= maxAnzahlDatensaetze)
    return 0;
  else
     return index * sizeof(ValueTimeTuple);
}

/// @brief 
/// @param index 0 <= index < maxAnzahlDatensaetze
/// @return 
bool isIndexOfEepromInvalide(int index){
  if(index < 0 || index >= maxAnzahlDatensaetze)
    return true;
  else
    return false;
}

/// @brief get Address for index 0, addDatensatzToTotalTimeOfSun and finally put to EEPROM and commit⌈
void saveToEEPROM(int indexDs, ValueTimeTuple tuple){
  if(isIndexOfEepromInvalide(indexDs))
    return;

  Serial.println("\n------------------\nsaveToEEPROM\n------------------");
  eepromTuple = tuple;
  int addr = getAddrFromIndex(indexDs);
  EEPROM.put(addr, eepromTuple);
  EEPROM.commit();
}

void addDatensatzToTotalTimeOfSun(const char* ds){
  totalTimeOfSunInSeconds += convertTotalTimeOfSunAsStringToSeconds(ds);
  convertTotalTimeOfSunAsSecondsToString();
}

/*
// void setTotalTimeOfSunToDatensatz(int index){
//   if(isIndexOfEepromInvalide(index))
//     return;
//   datensaetze[index] = totalTimeOfSunAsString;
//   Serial.println("-------------\nsetTotalTimeOfSunToDatensatz\n-------------");
//   Serial.print("\t| index: "); Serial.print(index);
//   Serial.print("\t| totalTimeOfSunAsString: "); Serial.print(totalTimeOfSunAsString);
//   Serial.print("\t| datensaetze[index]: "); Serial.print(datensaetze[index].c_str());
// }*/

/// @brief //https://stackoverflow.com/questions/63517768/esp32-writing-string-to-eeprom
/// @param index 0 <= index < maxAnzahlDatensaetze
void loadFromEepromToDatensatz(int index){
  if(printToDebug_loadFromEeprom) Serial.println("----------------\nloadFromEEPROM\n----------------");
  if(printToDebug_loadFromEeprom){Serial.print("totalTimeOfSunAsString: "); Serial.println(totalTimeOfSunAsString);}
  if(isIndexOfEepromInvalide(index))
    return;
  if(printToDebug_loadFromEeprom){Serial.print("index                 : "); Serial.println(index);}

  eepromAddress = getAddrFromIndex(index);
  if(printToDebug_loadFromEeprom){Serial.print("eepromAddress         : "); Serial.println(eepromAddress);}

  EEPROM.get(eepromAddress, eepromTuple);    
  // eepromTuple.day = eepromTuple.month = eepromTuple.year = 
  // eepromTuple.hours = eepromTuple.minutes = eepromTuple.seconds = index;
  if(printToDebug_loadFromEeprom)printTuple(eepromTuple);

  convertTotalTimeOfSunAsTupleToString(eepromTuple, tempTimeString);
  datensaetze[index] = tempTimeString;
  if(printToDebug_loadFromEeprom){Serial.print("datensaetze[index]    : "); Serial.println(datensaetze[index].c_str());}
}

/*
/// @brief Get index for datensaetze
/// @param addr 0 <= addr < MAX_SIZE_EEPROM
/// @return 0 <= i < maxAnzahlDatensaetze
int getIndexFromAddr(int addr = -1){
  if(isEepromAddressInvalide(addr))
    return 0;
  
  int index = addr / sizeof(ValueTimeTuple);
  if (index < 0 || index > maxAnzahlDatensaetze)
    return 0;
  else
    return index;
}*/

void initEEPROM(int index){
  if(isIndexOfEepromInvalide(index))
    return;
  Serial.println("\n---------------\ninitEEPROM\n---------------");
  
  initTuples();
  printTuple(eepromTuple);
  saveToEEPROM(index, eepromTuple);
  
  datensaetze[index] = "00:00:00";
  Serial.print("datensaetze[index]: "); Serial.println(datensaetze[index].c_str());  
}

void initdatensaetze(){
  Serial.println("initdatensaetze");

  maxAnzahlDatensaetze = MAX_SIZE_EEPROM / sizeof(ValueTimeTuple);
  Serial.print("maxAnzahlDatensaetze: "); Serial.println(maxAnzahlDatensaetze);

  datensaetze.clear();
  for(int i = 0; i < maxAnzahlDatensaetze; ++i){
    datensaetze.push_back("00:00:00");
  }
}

/*
/// @brief EEPROM has max 512 byte, messEEPROM > 1 byte --> next adress hat to be validated
int getNextValideEepromAdress(int addr){
  if(isEepromAddressInvalide(addr))
    return 0;

  return getValideEepromAddr(addr + sizeof(ValueTimeTuple));
}

/// @brief EEPROM has max 512 byte
/// @param addr 
/// @return 
int getValideEepromAddr(int addr){
  if(isEepromAddressInvalide(addr))
    return eepromAddress;
  else
    return addr;
}*/

/*
bool isEepromAddressInvalide(int addr){
  if(addr < 0 || addr + sizeof(ValueTimeTuple) >= MAX_SIZE_EEPROM)
    return true;
  else
    return false;
}*/

void setupEEPROM(){
  Serial.println("----------------\nsetupEEPROM\n----------------");
  EEPROM.begin(MAX_SIZE_EEPROM);
    
  initdatensaetze();

  for(int i = 0; i < maxAnzahlDatensaetze; ++i)
    loadFromEepromToDatensatz(i);
}

void setup() {
  Serial.begin(115200);Serial.println();
  initWifi();
  initTuples();
  timeClient.begin();

  // Start the server
  server.begin();
  Serial.println("Server started");

  // prepare LED
  // pinMode(LED_BUILTIN, OUTPUT);
  // digitalWrite(LED_BUILTIN, 0);

  setupEEPROM();
}

void loop() {
  // read the value from the sensor:
  // max Value is 1024 (full light)
  sensorValue = analogRead(sensorPin);

  newTuple.isShining = isShining(sensorValue);
  if(newTuple.isShining != oldTuple.isShining){
    updateTotalTimeOfSun();
  }

  handleClient();
}

void handleClient(){
  // Check if a client has connected
  client = server.available();
  if (!client) {
    delay(100);
    return;
  }
  if(printToDebug_handleClient) Serial.println(F("new client"));

  client.setTimeout(60*1000); // default is 1000
  matchRequest();

  // read/ignore the rest of the request
  // do not client.flush(): it is for output only, see below
  while (client.available()) {
    // byte by byte is not very efficient
    client.read();
  }

  displayHTMLwebpage();

  // Close the connection
  client.stop();
  if(printToDebug_handleClient) Serial.println("Client disconnected.");
}

void displayHTMLwebpage(){
  // Send the response to the client
  // it is OK for multiple small client.print/write,
  // because nagle algorithm will group them into one single packet
  
  // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
  // and a content-type so the client knows what's coming, then a blank line:
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println("Connection: close");
  client.println();
  
  client.println("<!DOCTYPE html><html>");
  printHead();
  printBody();
  client.println("</html>");
}

void printHead(){
  client.println("<head>");
  client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" http-equiv=\"refresh\" content=\"5\">");
  client.println("<link rel=\"icon\" href=\"data:,\">");
  printStyle();
  client.println("</head>");
}

/// @brief css styling
void printStyle(){
  // CSS to style the on/off buttons 
  // Feel free to change the background-color and font-size attributes to fit your preferences

  client.println("<style>");
  
  client.println(F("html {"
      "font-family: Helvetica;"
      "display: inline-block;"
      "margin: 0px auto;"
      "text-align: center;"
      "background-color: #00063D;"//DARK_BLUE
      "color: white;"
    "}"));

  printStyleForButtons();
  printStyleForTables();

  client.println("</style>");
}

/// @brief buttons and classes
void printStyleForButtons(){
  client.println(F(".button {"
      "background-color: #C0C0C0;"//GREY
      "color: black;"
      "border: none;"
      "padding: 16px 40px;"
      "text-decoration: none;"
      "font-size: 30px;"
      "margin: 2px;"
      "cursor: pointer;"
      "}"));
  client.println(".init   {background-color: #F08784;}");//RED
  client.println(".update {background-color: #A1FB8E;}");//GREEN
  client.println(".day    {background-color: #FFFE91;}");//YELLOW
  client.println(".night  {background-color: #3282F6;}");//BLUE
}

/// @brief tables, rows, columns, captions etc.
void printStyleForTables(){
  client.println(F("table, th, td, caption {"
      "border: thin solid #a0a0a0;"
      "text-align: center;"
      "}"));

  client.println(F("table {"
      "border-collapse: collapse;"
      "border-spacing: 0;"
      "border-width: thin 0 0 thin;"
      "margin: 0 0 1em;"
      "table-layout: auto;"
      "width:100%;"
      "max-width: 100%;"
      "}"));
  client.println(F("th, td {"
      "font-weight: normal;"
      "}"));
  client.println(F("th, caption {"
      "background-color: #3282F6;"//BLUE
      "font-weight: 700;"
      "}"));
}
void printBody(){
  char buff[MAX_BUFF];

  client.println("<body>");
  
  // Web Page Heading
  client.println("<h1>ESP8266 MessStation f&uuml;r Sonnenstunden</h1>");
  client.println("<p>");
  sprintf(buff, "aktueller Sensorwert: %4i", sensorValue);
  client.println(buff);
  client.print("</p>");
  if(newTuple.isShining){
    client.println("<p><button class=\"button day\">Die Sonne scheint</button></p>");
  }else{
    client.println("<p><button class=\"button night\">Die Sonne scheint gerade nicht</button></p>");
  }
  client.print("<div>Heute: ");
  client.print(convertTotalTimeOfSunAsSecondsToString());client.println("</div>");
  
  sprintf(buff, "<p>letzter Messzeitpunkt (hh:mm:ss): %02i:%02i:%02i</p>", newTuple.hours, newTuple.minutes, newTuple.seconds);
  client.println(buff);

  client.print("<table style=\"background-color: #C0C0C0;\">");//#C0C0C0 grey
  client.print("<tr>");
    client.print("<th>");
      client.print("<a href='http://");client.print(ip);client.print("/update'>");
      client.print("<button class=\"button update\">Aktualisieren</button></a>");
    client.print("</th>");

    client.print("<th>");
      client.print("<a href='http://");client.print(ip);client.print("/initTotalTimeOfSun'>");
      client.print("<button class=\"button init\">init Heute</button></a>");
      client.print("<br>setze die gemessene Zeit von Heute zur&uuml;ck.");
    client.print("</th>");

    client.print("<th>");
      client.print("<a href='http://");client.print(ip);client.print("/initAllEEPROM'>");
      client.print("<button class=\"button init\">init alle EEPROM-Daten</button></a>");
      client.print("<br>setze die bisher insgesamt gemessene Zeit zur&uuml;ck.");
    client.print("</th>");
  client.print("</tr>");
  client.println("</table>");
  
  printTableWithButtons();

  client.println("</body>");
}

void printTableWithButtons(){  
  client.println("<table>");
  for(int zeile = 1; zeile <= maxAnzahlDatensaetze; ++zeile){
    printTableRow(zeile);
  }
  client.println("</table>");
}

void  printTableRow(int zeile){
  char buff[MAX_BUFF];

  client.println("<tr>");
    client.print("<th style=\"width:10px\">");client.print(zeile);client.println("</th>");
    client.print("<th>Zeit: ");
    client.print(datensaetze[zeile-1].c_str());
    client.println("</th>");
    client.println("<th>");
      sprintf(buff, "<a href='http://%i.%i.%i.%i/initEEPROM/%02i'>", ip[0], ip[1], ip[2], ip[3], zeile);
      client.println(buff);
      client.print("<button class=\"button init\">init EEPROM</button></a>");
      client.println("</a>");
    client.println("</th>");
    client.println("<th>");
      sprintf(buff, "<a href='http://%i.%i.%i.%i/AddToDay/%02i'>", ip[0], ip[1], ip[2], ip[3], zeile);
      client.println(buff);
      client.println("<button class=\"button\">Addiere zu Heute</button>");
      client.println("</a>");
    client.println("</th>");
    client.println("<th>");
      sprintf(buff, "<a href='http://%i.%i.%i.%i/saveTotalTimeToEeprom/%02i'>", ip[0], ip[1], ip[2], ip[3], zeile);
      client.println(buff);
      client.println("<button class=\"button\">totalTimeOfSun speichern</button>");
      client.println("</a>");
    client.println("</th>");
  client.println("</tr>");
}

void matchRequest()
{
  int val = 0;

  // Read the first line of the request
  String reqS = client.readStringUntil('\r');
  std::string req = reqS.c_str();
  Serial.printf("request: %s\n-----------------\n", req.c_str()); //z.B. "GET /initEEPROM/2 HTTP/1.1"

  if (req.find("/initTotalTimeOfSun") != std::string::npos) {
    initTotalTimeOfSun();
  } 
  
  else if(req.find("initAllEEPROM") != std::string::npos)
  {
    for(int i = 0; i < maxAnzahlDatensaetze; ++i)
      initEEPROM(i);
  } 
  
  else if (req.find("/update") != std::string::npos){
    updateTotalTimeOfSun();
  } 
  
  else if (req.find("/initEEPROM/") != std::string::npos)
  {
    Serial.println("req.find('/initEEPROM/'");
    std::string pattern = "/initEEPROM/";
    int index = getIndexFromRequest(req, pattern);
    if(isIndexOfEepromInvalide(index))
      return;
    
    Serial.print("index: "); Serial.println(index);
    initEEPROM(index);
  } 
  
  else if (req.find("/AddToDay/") != std::string::npos)
  {
    std::string pattern = "/AddToDay/";
    int index = getIndexFromRequest(req, pattern);
    if(isIndexOfEepromInvalide(index))
      return;
    
    loadFromEepromToDatensatz(index);
    addDatensatzToTotalTimeOfSun(datensaetze[index].c_str());
  }
  
  else if (req.find("/saveTotalTimeToEeprom/") != std::string::npos)
  {
    std::string pattern = "/saveTotalTimeToEeprom/";
    int index = getIndexFromRequest(req, pattern);
    if(isIndexOfEepromInvalide(index))
      return;

    eepromTuple = convertTotalTimeOfSunAsSecondsToTuple();
    saveToEEPROM(index, eepromTuple);

    updateTotalTimeOfSun();
  }
  
  else {
    Serial.println("invalid request");
  }
}

int getIndexFromRequest(std::string req, std::string pattern){
  if(printToDebug_getIndexFromRequest) Serial.println("\t\t\t\t\tgetIndexFromRequest");
  if(printToDebug_getIndexFromRequest){Serial.print("\t\t\t\t\tpattern: "); Serial.println(pattern.c_str());}
  
  int offset = pattern.length();
  if(printToDebug_getIndexFromRequest){Serial.print("\t\t\t\t\toffset: "); Serial.println(offset);}
  
  std::string subStr = req.substr(req.find(pattern) + offset, 2);
  if(printToDebug_getIndexFromRequest){Serial.print("\t\t\t\t\tsubStr: "); Serial.println(subStr.c_str());}
  
  int index = std::stoi(subStr)-1;//index = zeile -1
  if(printToDebug_getIndexFromRequest){Serial.print("\t\t\t\t\tindex: "); Serial.println(index);}
  
  return index;
}

void initWifi(){
  WiFi.begin(ssid, password);
  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.print ( "." );
  }
  ip = WiFi.localIP();
  Serial.println(ip);
}


void initTuples(){
  Serial.println("\n---------------\ninitTuples\n---------------");
  oldTuple.isShining = false;
  oldTuple.hours = oldTuple.minutes = oldTuple.seconds = 0;
  oldTuple.day = oldTuple.month = oldTuple.year = 0;
  eepromTuple = newTuple = oldTuple;
}

void initTotalTimeOfSun(){
  totalTimeOfSunInSeconds = 0;
  convertTotalTimeOfSunAsSecondsToString();
}

void updateTotalTimeOfSun(){
  setTimeInTuple(newTuple);

  printTuple(newTuple);
  
  if(printToDebug_updateTotalTimeOfSun){
    printTuple(oldTuple);
    Serial.print("totalTime before: "); Serial.println(totalTimeOfSunInSeconds);
  }
  totalTimeOfSunInSeconds += calculateSunTimeInSeconds();
  convertTotalTimeOfSunAsSecondsToString();
  if(printToDebug_updateTotalTimeOfSun){
    Serial.print("totalTime  after: "); Serial.println(totalTimeOfSunInSeconds);
  }

  oldTuple = newTuple;
  if(printToDebug_updateTotalTimeOfSun){
    Serial.println("new total time of sun: "); 
    Serial.printf("\tin seconds: %i\n", totalTimeOfSunInSeconds);
    Serial.printf("\tas string : %s\n", totalTimeOfSunAsString);
  }
  else
    Serial.printf("new total time of sun: %s\n", totalTimeOfSunAsString);

  for(int index = 0; index < maxAnzahlDatensaetze; ++index)
    loadFromEepromToDatensatz(index);
}

/// @brief vgl. https://randomnerdtutorials.com/esp8266-nodemcu-date-time-ntp-client-server-arduino/
/// @param tuple 
void setTimeInTuple(ValueTimeTuple &tuple){
  timeClient.update();
  
  tuple.hours = timeClient.getHours();
  tuple.minutes = timeClient.getMinutes();
  tuple.seconds = timeClient.getSeconds();

  epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime ((time_t *)&epochTime);

  int monthDay = ptm->tm_mday;
  Serial.print("Month day: ");
  Serial.println(monthDay);

  int currentMonth = ptm->tm_mon+1;
  Serial.print("Month: ");
  Serial.println(currentMonth);

  int currentYear = ptm->tm_year+1900;
  Serial.print("Year: ");
  Serial.println(currentYear);

  tuple.day = monthDay;
  tuple.month = currentMonth;
  tuple.year = currentYear;
}

long calculateSunTimeInSeconds(){
  if(oldTuple.isShining == false)
    return 0;
  if(printToDebug_calculateSunTimeInSeconds){
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
  
  if(printToDebug_calculateSunTimeInSeconds)
    Serial.printf("totalSeconds: %i\n", totalSeconds);

  return totalSeconds;
}

/// @brief is the sun is shining?
/// @param value 
/// @return true, if value is big enough
bool isShining(int value){
    if(value > sensorValueSunIsShining)
        return true;
    else
        return false;
}