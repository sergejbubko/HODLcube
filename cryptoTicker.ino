/*******************************************************************
    A project to display crypto currency prices using an ESP8266

    Main Hardware:
    - NodeMCU Development Board (Any ESP8266 dev board will work)
    - OLED I2C Display (SH1106)

    Written by Brian Lough
    https://www.youtube.com/channel/UCezJOfu7OtqGzd5xrP3q6WA
 *******************************************************************/

// ----------------------------
// Standard Libraries - Already Installed if you have ESP8266 set up
// ----------------------------

#include <WiFiClientSecure.h>
#include <Wire.h>

// ----------------------------
// Additional Libraries - each one of these will need to be installed.
// ----------------------------

// #include <CoinMarketCapApi.h>
// For Integrating with the CoinMarketCap.com API
// Available on the library manager (Search for "CoinMarket")
// https://github.com/witnessmenow/arduino-coinmarketcap-api

#include "CoinBaseProApi.h"
// Reused CoinMarketCapApi

#include "webStrings.h"
#include "graphic_oledi2c.h"

#include "SH1106.h"
// The driver for the OLED display
// Available on the library manager (Search for "oled ssd1306")
// https://github.com/squix78/esp8266-oled-ssd1306

#include <ArduinoJson.h>
// !! NOTE !!: When installing this select an older version than V6 from the drop down
// Required by the CoinMarketCapApi Library for parsing the response
// Available on the library manager (Search for "arduino json")
// https://github.com/squix78/esp8266-oled-ssd1306

#include <Arduino.h>
#ifdef ESP32
  #include <WiFi.h>
  #include <AsyncTCP.h>
  #include <SPIFFS.h>
#else
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
  #include <Hash.h>
  #include <FS.h>
#endif
#include <ESPAsyncWebServer.h>

#include <strings_en.h>
#include <WiFiManager.h>        //https://github.com/tzapu/WiFiManager/tree/feature_asyncwebserver

AsyncWebServer server(80);

// ----------------------------
// Configurations - Update these
// ----------------------------

char ssidAP[] = "HODLdisplay";  // SSID of the device
char pwdAP[] = "toTheM00n";  // password of the device

// Pins based on your wiring
// display
#define SCL_PIN D1
#define SDA_PIN D2

// LEDs
#define LED_PIN_UP D3
#define LED_PIN_DOWN D5

// Buzzer
#define BUZZER_PIN D0

// Battery
#define BATTERY_PIN A0

// Have tested up to 10, can probably do more
#define MAX_HOLDINGS 10

float LEDthreshold; // percent
// We'll request a new value just before we change the screen so it's the most up to date
// Rate Limits: https://docs.pro.coinbase.com/#rate-limits
// We throttle public endpoints by IP: 3 requests per second, up to 6 requests per second in bursts. Some endpoints may have custom rate limits.
unsigned long screenChangeDelay; // milis
unsigned long screenChangeDue;
String currency; // in 3-char format like eur, gbp, usd
float buzzerThreshold; // percent
// Selected crypto currecnies by user to show on display stored in SPIFFS (in format btcethltc etc.)
String selectedCryptos;
//int dispBrightness;
float batVoltage;
unsigned long batVreadDelay = 60000;
unsigned long batVreadDue;

#define CRYPTO_COUNT 3
// Available crypto currecnies
String cryptos[CRYPTO_COUNT];

WiFiClientSecure clientSSL;
CoinBaseProApi api(clientSSL);

const char* PARAM_LEDTHRESHOLD = "inputLEDthreshold";
const char* PARAM_BUZZERTHRESHOLD = "inputBuzzerThreshold";
const char* PARAM_SCREENCHANGEDELAY = "inputScreenChangeDelay";
const char* PARAM_CURRENCY = "inputCurrency";
//const char* PARAM_DISPBRIGHTNESS = "inputDispBrightness";

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

String readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if(!file || file.isDirectory()){
    Serial.println("- empty file or failed to open file");
    return String();
  }
  Serial.println("- read from file:");
  String fileContent;
  while(file.available()){
    fileContent+=String((char)file.read());
  }
  Serial.println(fileContent);
  return fileContent;
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, "w");
  if(!file){
    Serial.println("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
}

// Replaces placeholder with stored values
// it looks for variables in format %var_name%
String processor(const String& var){
  //Serial.println(var);
  if(var == PARAM_LEDTHRESHOLD){
    return readFile(SPIFFS, "/inputLEDthreshold.txt");
  }
  if(var == PARAM_BUZZERTHRESHOLD){
    return readFile(SPIFFS, "/inputBuzzerThreshold.txt");
  }
/*  if(var == PARAM_DISPBRIGHTNESS){
    return readFile(SPIFFS, "/inputDispBrightness.txt");
  }*/
  for (int i = 0; i < CRYPTO_COUNT; i++) {
    if(var == "crypto" + String(i) and selectedCryptos.indexOf(cryptos[i]) > -1){
      return "checked";
    }
  }
  for (int i = 0; i < 3; i++) {
    if(var == "screenChangeDelay" + String(screenChangeDelay)){
      return "selected";
    }
  }
  for (int i = 0; i < 3; i++) {
    if(var == currency){
      return "selected";
    }
  }
  return String();
}

SH1106 display(0x3c, SDA_PIN, SCL_PIN);

struct Holding {
  String tickerId;
  float newPrice;
  float oldPrice;
  bool inUse;
  CMCTickerResponse lastResponse;
};

Holding holdings[MAX_HOLDINGS];

int currentIndex = -1;
//String ipAddressString;

void addNewHolding(String tickerId, float newPrice = 0, float oldPrice = 0) {
  int index = getNextFreeHoldingIndex();
  if (index > -1) {
    holdings[index].tickerId = tickerId;
    holdings[index].newPrice = newPrice;
    holdings[index].oldPrice = oldPrice;
    holdings[index].inUse = true;
  }
}

void setup() {
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  Serial.begin(115200);
  pinMode(LED_PIN_DOWN, OUTPUT);
  pinMode(LED_PIN_UP, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BATTERY_PIN, INPUT);

  // Initialising the display
  display.init();
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  //display.setBrightness(dispBrightness);
  
  batVoltage = batteryVoltage();
  if (batVoltage > 2.0 && batVoltage < 3.3) {
    lowBattery();
  }

  // Initialize SPIFFS
  #ifdef ESP32
    if(!SPIFFS.begin(true)){
      Serial.println("An Error has occurred while mounting SPIFFS");
      return;
    }
  #else
    if(!SPIFFS.begin()){
      Serial.println("An Error has occurred while mounting SPIFFS");
      return;
    }
  #endif
  cryptos[0]="btc";
  cryptos[1]="eth";
  cryptos[2]="ltc";
  
  LEDthreshold = readFile(SPIFFS, "/inputLEDthreshold.txt").toFloat();
  buzzerThreshold = readFile(SPIFFS, "/inputBuzzerThreshold.txt").toFloat();
  // data from text file in format "btcethltc" etc., 1 crypto = 3 characters
  selectedCryptos = readFile(SPIFFS, "/inputCrypto.txt");
  screenChangeDelay = readFile(SPIFFS, "/inputScreenChangeDelay.txt").toInt();
  //dispBrightness = readFile(SPIFFS, "/inputDispBrightness.txt").toInt();
  currency = readFile(SPIFFS, "/inputCurrency.txt");

  for (int i = 0; i < selectedCryptos.length(); i += 3) {
    addNewHolding(selectedCryptos.substring(i, i + 3));
  }

  display.clear();
  display.setFont(ArialMT_Plain_24);
  display.drawString(64, 3, F("#HODL"));
  display.drawString(64, 31, F("Display"));
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(128, 50, F("v1.0"));
  display.display();
  delay(3000);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;
  //reset saved settings - testing purpose
  //wm.resetSettings();
  //fetches ssid and pass from eeprom and tries to connect
  //if it does not connect it starts an access point with the specified name
  //automatically connect using saved credentials if they exist
  //If connection fails it starts an access point with the specified name   
  Serial.println("Configportal running");
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, F("Connect using wifi"));
  display.drawString(0, 11, "SSID:");
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 22, ssidAP);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 37, "pass:");
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 48, pwdAP);
  display.display();   
  if (wm.autoConnect(ssidAP, pwdAP)) {
    Serial.println("connected...yeey :)");
  }
  
//  // Set WiFi to station mode and disconnect from an AP if it was Previously
//  // connected
//  WiFi.mode(WIFI_STA);
//  WiFi.disconnect();
//  delay(100);
//
//  // Attempt to connect to Wifi network:
//  Serial.print("Connecting Wifi: ");
//  Serial.println(ssid);
//  WiFi.begin(ssid, password);
//  while (WiFi.status() != WL_CONNECTED) {
//    Serial.print(".");
//    delay(500);
//  }
//  Serial.println("");
//  Serial.println("WiFi connected");
//  Serial.println("IP address: ");
  IPAddress ip = WiFi.localIP();
//  Serial.println(ip);

  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 0, F("Connect to"));
  display.setFont(ArialMT_Plain_24);
  display.drawString(64, 18, ip.toString());
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 45, F("for config"));
  display.display();
  //ipAddressString = ip.toString();

  // Send web page with input fields to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  // Send a GET request to <ESP_IP>/get?inputString=<inputMessage>
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage = "";
    // GET inputLEDthreshold value on <ESP_IP>/get?inputLEDthreshold=<inputMessage>
    if (request->hasParam(PARAM_LEDTHRESHOLD)) {
      inputMessage = request->getParam(PARAM_LEDTHRESHOLD)->value();
      writeFile(SPIFFS, "/inputLEDthreshold.txt", inputMessage.c_str());
    }
    else if (request->hasParam(PARAM_BUZZERTHRESHOLD)) {
      inputMessage = request->getParam(PARAM_BUZZERTHRESHOLD)->value();
      writeFile(SPIFFS, "/inputBuzzerThreshold.txt", inputMessage.c_str());
    }
    else if (request->hasParam(PARAM_SCREENCHANGEDELAY)) {
      inputMessage = request->getParam(PARAM_SCREENCHANGEDELAY)->value();
      writeFile(SPIFFS, "/inputScreenChangeDelay.txt", inputMessage.c_str());
    }
    else if (request->hasParam(PARAM_CURRENCY)) {
      inputMessage = request->getParam(PARAM_CURRENCY)->value();
      writeFile(SPIFFS, "/inputCurrency.txt", inputMessage.c_str());
    }
    /*else if (request->hasParam(PARAM_DISPBRIGHTNESS)) {
      inputMessage = request->getParam(PARAM_DISPBRIGHTNESS)->value();
      writeFile(SPIFFS, "/inputDispBrightness.txt", inputMessage.c_str());
    }*/
    else {
      for (int i = 0; i < CRYPTO_COUNT; i++) {
        String param = "crypto" + String(i);
        if (request->hasParam(param)) {
          inputMessage += request->getParam(param)->value();
        }
      }
      if (inputMessage != "") {
        writeFile(SPIFFS, "/inputCrypto.txt", inputMessage.c_str());
      }
    }
//    else {
//      inputMessage = "No message sent";
//    }
    Serial.println(inputMessage);
    request->send(200, "text/text", inputMessage);
    ESP.restart();
  });
  server.onNotFound(notFound);
  server.begin();
  delay(5000);
}

int getNextFreeHoldingIndex() {
  for (int i = 0; i < MAX_HOLDINGS; i++) {
    if (!holdings[i].inUse) {
      return i;
    }
  }

  return -1;
}

int getNextIndex() {
  for (int i = currentIndex + 1; i < MAX_HOLDINGS; i++) {
    if (holdings[i].inUse) {
      return i;
    }
  }

  for (int j = 0; j <= currentIndex; j++) {
    if (holdings[j].inUse) {
      return j;
    }
  }

  return -1;
}

void displayHolding(int index) {

  CMCTickerResponse response = holdings[index].lastResponse;

  display.clear();

  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  currency.toUpperCase();
  String crypto = holdings[index].tickerId;
  crypto.toUpperCase();
  // c++ formatting not using String
  char percent_change_24h[6];
  snprintf(percent_change_24h, sizeof(percent_change_24h), "%+3.1f", (response.price / response.open - 1) * 100);
  display.drawString(64, 0, crypto + currency + " " + percent_change_24h + "%");
  display.setFont(ArialMT_Plain_24);
  float price = (float)response.price;
  holdings[index].oldPrice = holdings[index].newPrice;
  holdings[index].newPrice = price;
  display.drawString(64, 20, formatCurrency(price));
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  //display.drawString(64, 48, "L:" + String(response.low, 0) + ", H:" + String(response.high, 0) + ", B:" + batteryVoltage() + "V");
  display.drawString(0, 45, "L:" + String(response.low, 0));  
  display.drawString(0, 54, "H:" + String(response.high, 0));
  display.drawString(50, 45, "vol:" + formatVolume(response.volume_24h));  
  display.drawString(100, 54, String(batVoltage) + "V");
  if (batVoltage > 3.8) {
    display.drawXbm(80, 54, BATTERY_3_3_WIDTH, BATTERY_3_3_HEIGHT, battery_3_3);
  } else if (batVoltage > 3.6) {
    display.drawXbm(80, 54, BATTERY_2_3_WIDTH, BATTERY_2_3_HEIGHT, battery_2_3);
  } else if (batVoltage > 3.4) {
    display.drawXbm(80, 54, BATTERY_1_3_WIDTH, BATTERY_1_3_HEIGHT, battery_1_3);
  } 
  if (holdings[index].newPrice < holdings[index].oldPrice) {
    display.fillTriangle(100, 29, 114, 29, 107, 36);
  } else if (holdings[index].newPrice > holdings[index].oldPrice) {
    display.fillTriangle(100, 37, 114, 37, 107, 30);
  }
  display.display();
  if (holdings[index].newPrice < (holdings[index].oldPrice * (1.0 - LEDthreshold / 100.0))) {
    LEDdown();
  }
  else if (holdings[index].newPrice > (holdings[index].oldPrice * (1.0 + LEDthreshold / 100.0))) {
    LEDup();
  }
  if (holdings[index].newPrice < (holdings[index].oldPrice * (1.0 - buzzerThreshold / 100.0))) {
    buzzerDown();
  }
  else if (holdings[index].newPrice > (holdings[index].oldPrice * (1.0 + buzzerThreshold / 100.0))) {
    buzzerUp();
  }
}

void LEDdown(){    
  // Flash LED
  digitalWrite(LED_PIN_DOWN, HIGH);
  delay(150);
  digitalWrite(LED_PIN_DOWN, LOW);
}
void LEDup(){    
  // Flash LED
  digitalWrite(LED_PIN_UP, HIGH);
  delay(150);
  digitalWrite(LED_PIN_UP, LOW); 
}

void buzzerDown(){    
  // Flash LED
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
}

void buzzerUp(){    
  // Flash LED
  digitalWrite(BUZZER_PIN, HIGH);
  delay(300);
  digitalWrite(BUZZER_PIN, LOW); 
  delay(300);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(300);
  digitalWrite(BUZZER_PIN, LOW);
}

void displayMessage(String message){
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawStringMaxWidth(0, 0, 128, message);
  display.display();
}

String formatCurrency(float price) {
//  String formattedCurrency = CURRENCY_SYMBOL;
  String formattedCurrency = "";
  int pointsAfterDecimal;
  if (price > 1000) {
    pointsAfterDecimal = 0;
  } else if (price > 100) {
    pointsAfterDecimal = 1;
  } else if (price >= 1) {
    pointsAfterDecimal = 4;
  } else {
    pointsAfterDecimal = 6;
  }
  formattedCurrency.concat(String(price, pointsAfterDecimal));
  return formattedCurrency;
}

String formatVolume(float volume) {
  if (volume > 999) {
    return String(volume/1000.0, 1) + "k";
  } else if (volume > 999999) {
    return String(volume/1000000.0, 1) + "M";
  }
}

bool loadDataForHolding(int index) {
  int nextIndex = getNextIndex();
  if (nextIndex > -1 ) {
    holdings[index].lastResponse = api.GetTickerInfo(holdings[index].tickerId, currency);
    //Serial.println("nextIndex: " + String(holdings[index].lastResponse.error));
    return holdings[index].lastResponse.error == "";
  }

  return false;
}

float batteryVoltage() {
  unsigned int raw=0;
  float voltage=0.0;
//  float voltageMatrix[22][2] = {
//    {4.2,  100},
//    {4.15, 95},
//    {4.11, 90},
//    {4.08, 85},
//    {4.02, 80},
//    {3.98, 75},
//    {3.95, 70},
//    {3.91, 65},
//    {3.87, 60},
//    {3.85, 55},
//    {3.84, 50},
//    {3.82, 45},
//    {3.80, 40},
//    {3.79, 35},
//    {3.77, 30},
//    {3.75, 25},
//    {3.73, 20},
//    {3.71, 15},
//    {3.69, 10},
//    {3.61, 5},
//    {3.27, 0},
//    {0, 0}};
  raw = analogRead(BATTERY_PIN);
  // Wemos D1 Internal Voltage divider (220kOhm+100kOhm) and Wemos battery shield is used with J2 jumper soldered (130kOhm)
  // 4.5V is maximum LiPo battery voltage
  // due to voltage measurements constant is added
  voltage=raw/1023.0*4.5*0.98;
  return (voltage < 2 ? 0.0 : voltage);
//  for(int i=20; i>=0; i--) {
//    if(voltageMatrix[i][0] >= voltage) {
//      return (int) voltageMatrix[i + 1][1];
//    }
//  }
}
void lowBattery(void) {
  display.clear();
  display.drawXbm(47, 12, BATTERY_0_3_WIDTH, BATTERY_0_3_HEIGHT, battery_0_3);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 37, F("low battery"));
  display.display();
  tone(BUZZER_PIN, 33, 500);
  delay(5000);
  display.displayOff();
  ESP.deepSleep(0);
}

void loop() {
  // To access your stored values on inputString, inputInt, inputLEDthreshold
//  LEDthreshold = readFile(SPIFFS, "/inputLEDthreshold.txt").toFloat();
//  Serial.print("*** Your inputLEDthreshold: ");
//  Serial.println(LEDthreshold);
//  String crypto = readFile(SPIFFS, "/inputCrypto.txt");
//  Serial.print("*** Your crypto: ");
//  Serial.println(crypto);
//  delay(5000);
  unsigned long timeNow = millis();
  if (timeNow > batVreadDue) {
    batVoltage = batteryVoltage();
    batVreadDue = timeNow + batVreadDelay;
  }
  if (batVoltage > 2.0 && batVoltage < 3.3) {
    lowBattery();
  }
  if ((timeNow > screenChangeDue))  {
    currentIndex = getNextIndex();
    if (currentIndex > -1) {
      if (loadDataForHolding(currentIndex)) {
        displayHolding(currentIndex);
      } else {
        displayMessage(F("Error loading data."));
      }
    } else {
      displayMessage(F("No funds to display. Edit the setup to add them"));
    }
    screenChangeDue = timeNow + screenChangeDelay;
  }
}
