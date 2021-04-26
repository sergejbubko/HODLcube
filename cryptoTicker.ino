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

// Have tested up to 10, can probably do more
#define MAX_HOLDINGS 10

// You also need to define a variable to hold the previous price and the threshold of amount of change we need to flash the LED
double previousValue = 0.0;
float LEDthreshold = 0.05; // percent
// We'll request a new value just before we change the screen so it's the most up to date
// Rate Limits: https://docs.pro.coinbase.com/#rate-limits
// We throttle public endpoints by IP: 3 requests per second, up to 6 requests per second in bursts. Some endpoints may have custom rate limits.
unsigned long screenChangeDelay = 10000; // milis
String currency = "eur"; // in 3-char format like eur, gbp, usd
float buzzerThreshold; // percent
// Selected crypto currecnies by user to show on display stored in SPIFFS (in format btcethltc etc.)
String selectedCryptos = "btc";

#define CRYPTO_COUNT 3
// Available crypto currecnies
String cryptos[CRYPTO_COUNT];

WiFiClientSecure clientSSL;
CoinBaseProApi api(clientSSL);

const char* PARAM_LEDTHRESHOLD = "inputLEDthreshold";
const char* PARAM_BUZZERTHRESHOLD = "inputBuzzerThreshold";
const char* PARAM_SCREENCHANGEDELAY = "inputScreenChangeDelay";
const char* PARAM_CURRENCY = "inputCurrency";

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
  if(var == "inputLEDthreshold"){
    return readFile(SPIFFS, "/inputLEDthreshold.txt");
  }
  if(var == "inputBuzzerThreshold"){
    return readFile(SPIFFS, "/inputBuzzerThreshold.txt");
  }
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

unsigned long screenChangeDue;

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
  currency = readFile(SPIFFS, "/inputCurrency.txt");

  for (int i = 0; i < selectedCryptos.length(); i += 3) {
    addNewHolding(selectedCryptos.substring(i, i + 3));
  }
  
  pinMode(LED_PIN_DOWN, OUTPUT);
  pinMode(LED_PIN_UP, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Initialising the display
  display.init();
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_24);
  display.drawString(64, 5, F("#HODL"));
  display.drawString(64, 34, F("Display"));
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
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 0, F("Connect to"));
  display.setFont(ArialMT_Plain_24);
  display.drawString(64, 18, ip.toString());
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 50, F("for configuration"));
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
  snprintf(percent_change_24h, sizeof(percent_change_24h), "%+3.1f", (response.last / response.open - 1) * 100);
  display.drawString(64, 0, crypto + currency + " " + percent_change_24h + "%");
  display.setFont(ArialMT_Plain_24);
  double price = response.last;
  holdings[index].oldPrice = holdings[index].newPrice;
  holdings[index].newPrice = price;
  display.drawString(64, 20, formatCurrency(price));
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 48, "L:" + String(response.low, 0) + ", H:" + String(response.high, 0));
  display.display();
  if (holdings[index].newPrice < (holdings[index].oldPrice * (1.0 - LEDthreshold / 100.0))) {
    LEDdown();
  }
  if (holdings[index].newPrice > (holdings[index].oldPrice * (1.0 + LEDthreshold / 100.0))) {
    LEDup();
  }
  if (holdings[index].newPrice < (holdings[index].oldPrice * (1.0 - buzzerThreshold / 100.0))) {
    buzzerDown();
  }
  if (holdings[index].newPrice > (holdings[index].oldPrice * (1.0 + buzzerThreshold / 100.0))) {
    buzzerUp();
  }
}

void LEDdown(){    
  // Flash LED
  digitalWrite(LED_PIN_DOWN, HIGH);
  delay(150);
  digitalWrite(LED_PIN_DOWN, LOW);
  delay(150);
  digitalWrite(LED_PIN_DOWN, HIGH);
  delay(150);
  digitalWrite(LED_PIN_DOWN, LOW);
  delay(150);
  digitalWrite(LED_PIN_DOWN, HIGH);
  delay(150);
  digitalWrite(LED_PIN_DOWN, LOW);
}
void LEDup(){    
  // Flash LED
  digitalWrite(LED_PIN_UP, HIGH);
  delay(150);
  digitalWrite(LED_PIN_UP, LOW); 
  delay(150);
  digitalWrite(LED_PIN_UP, HIGH);
  delay(150);
  digitalWrite(LED_PIN_UP, LOW);
  delay(150);
  digitalWrite(LED_PIN_UP, HIGH);
  delay(150);
  digitalWrite(LED_PIN_UP, LOW);
}

void buzzerDown(){    
  // Flash LED
  digitalWrite(BUZZER_PIN, HIGH);
  delay(150);
  digitalWrite(BUZZER_PIN, LOW);
  delay(150);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(150);
  digitalWrite(BUZZER_PIN, LOW);
}

void buzzerUp(){    
  // Flash LED
  digitalWrite(BUZZER_PIN, HIGH);
  delay(300);
  digitalWrite(BUZZER_PIN, LOW); 
  delay(150);
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
  int pointsAfterDecimal = 6;
  if (price > 10000) {
    pointsAfterDecimal = 0;
  } else if (price > 100) {
    pointsAfterDecimal = 2;
  } else if (price > 1) {
    pointsAfterDecimal = 4;
  }
  formattedCurrency.concat(String(price, pointsAfterDecimal));
  return formattedCurrency;
}

bool loadDataForHolding(int index) {
  int nextIndex = getNextIndex();
  if (nextIndex > -1 ) {
    holdings[index].lastResponse = api.GetTickerInfo(holdings[index].tickerId, currency);
    Serial.println("nextIndex: " + String(holdings[index].lastResponse.error));
    return holdings[index].lastResponse.error == "";
  }

  return false;
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
