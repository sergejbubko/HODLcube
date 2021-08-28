/*******************************************************************
    A project to display crypto currency prices using an ESP8266

    Main Hardware:
    - Lolin D1 mini (Any ESP8266 dev board will work)
    - OLED I2C 1.3" Display (SH1106)

 *******************************************************************/

// ----------------------------
// Standard Libraries - Already Installed if you have ESP8266 set up
// ----------------------------

#include <WiFiClientSecure.h>
#include <Wire.h>

// ----------------------------
// Additional Libraries - each one of these will need to be installed.
// ----------------------------

#include "CoinbaseProApi.h"

#include "webStrings.h"
#include "graphic_oledi2c.h"

#include "SH1106.h"
// The driver for the OLED display
// Available on the library manager (Search for "oled ssd1306")
// https://github.com/ThingPulse/esp8266-oled-ssd1306

// https://github.com/bblanchon/ArduinoJson
#include <ArduinoJson.h>
// Available on the library manager (Search for "arduino json")

#include <Arduino.h>
#include <ESP8266WiFi.h>
// https://github.com/me-no-dev/ESPAsyncTCP
#include <ESPAsyncTCP.h>
#include <Hash.h>
#include <FS.h>
// https://github.com/me-no-dev/ESPAsyncWebServer
#include <ESPAsyncWebServer.h>

#include <strings_en.h>
// https://github.com/tzapu/WiFiManager/tree/feature_asyncwebserver
#include <WiFiManager.h>        

// https://github.com/datacute/DoubleResetDetector
#include <DoubleResetDetector.h>

// Pins based on your wiring
// display
#define SCL_PIN D1
#define SDA_PIN D2

// LEDs
#define LED_PIN_UP D5
#define LED_PIN_DOWN D6

// Buzzer
#define BUZZER_PIN D7

// Display power
#define DISPLAY_POWER_PIN D8

// Have tested up to 10, can probably do more
#define MAX_HOLDINGS 10

// number of crypto showing in config portal
#define CRYPTO_COUNT 8

// Number of seconds after reset during which a 
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 10

// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

const char* VER = "v0.2.2";

char ssidAP[] = "HODLcube";  // SSID of the device
char pwdAP[] = "toTheMoon";  // password of the device

unsigned long screenChangeDue;

int dataNotLoadedCounter = 0;

const char* PARAM_LEDTHRESHOLD = "inputLEDthreshold";
const char* PARAM_BUZZERTHRESHOLD = "inputBuzzerThreshold";
const char* PARAM_SCREENCHANGEDELAY = "inputScreenChangeDelay";
const char* PARAM_CURRENCY = "inputCurrency";

struct Holding {
  String tickerId;
  float newPrice;
  float oldPrice;
  bool inUse;
  unsigned long statsReadDue;
  CBPTickerResponse lastTickerResponse;
  CBPStatsResponse lastStatsResponse;
};

struct Settings {
  float LEDthreshold; // percent
  float buzzerThreshold;
// We'll request a new value just before we change the screen so it's the most up to date
// Rate Limits: https://docs.pro.coinbase.com/#rate-limits
// We throttle public endpoints by IP: 3 requests per second, up to 6 requests per second in bursts. Some endpoints may have custom rate limits.
  unsigned long screenChangeDelay; // milis
  String cryptos[CRYPTO_COUNT];
};

const char *filename = "/settings.txt";
Settings settings;                         // <- global settings object

// Loads the settings from a file
void loadSettings(const char *filename, Settings &settings) {
  // Open file for reading
  File file = SPIFFS.open(filename, "r");

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/v6/assistant to compute the capacity.
  StaticJsonDocument<512> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);
  if (error)
    Serial.println(F("Failed to read file, using default settings"));

  // Copy values from the JsonDocument to the Config
  settings.LEDthreshold = doc["LEDthreshold"] | 0.05;
  settings.buzzerThreshold = doc["buzzerThreshold"] | 0.1;
  settings.screenChangeDelay = doc["screenChangeDelay"] | 5000;  
  settings.cryptos[0] = String(doc["cryptos0"]);
  settings.cryptos[1] = String(doc["cryptos1"]);  
  settings.cryptos[2] = String(doc["cryptos2"]);  
  settings.cryptos[3] = String(doc["cryptos3"]);  
  settings.cryptos[4] = String(doc["cryptos4"]);  
  settings.cryptos[5] = String(doc["cryptos5"]);
  settings.cryptos[6] = String(doc["cryptos6"]);
  settings.cryptos[7] = String(doc["cryptos7"]);  
  
//  strlcpy(settings.hostname,                  // <- destination
//          doc["hostname"] | "example.com",  // <- source
//          sizeof(settings.hostname));         // <- destination's capacity

// Close the file
  file.close();
}

// Saves the settings to a file
void saveSettings(const char *filename, const Settings &settings) {
  // Delete existing file, otherwise the settings is appended to the file
  SPIFFS.remove(filename);

  // Open file for writing
  File file = SPIFFS.open(filename, "w");
  if (!file) {
    Serial.println(F("Failed to create file"));
    return;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/assistant to compute the capacity.
  StaticJsonDocument<384> doc;

  // Set the values in the document 
  doc["LEDthreshold"] = settings.LEDthreshold;
  doc["buzzerThreshold"] = settings.buzzerThreshold;
  doc["screenChangeDelay"] = settings.screenChangeDelay;  
  doc["cryptos0"] = settings.cryptos[0];
  doc["cryptos1"] = settings.cryptos[1];  
  doc["cryptos2"] = settings.cryptos[2];  
  doc["cryptos3"] = settings.cryptos[3];  
  doc["cryptos4"] = settings.cryptos[4];  
  doc["cryptos5"] = settings.cryptos[5];
  doc["cryptos6"] = settings.cryptos[6];
  doc["cryptos7"] = settings.cryptos[7];  
  
  // Serialize JSON to file
  if (serializeJson(doc, file) == 0) {
    Serial.println(F("Failed to write to file"));
  }
  // Close the file
  file.close();
}

// Prints the content of a file to the Serial
void printFile(const char *filename) {
  // Open file for reading
  File file = SPIFFS.open(filename, "r");
  if (!file) {
    Serial.println(F("Failed to read file"));
    return;
  }

  // Extract each characters by one by one
  while (file.available()) {
    Serial.print((char)file.read());
  }
  Serial.println();

  // Close the file
  file.close();
}

int currentIndex = -1;

AsyncWebServer server(80);
WiFiClientSecure clientSSL;
CoinbaseProApi api(clientSSL);
DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);
Holding holdings[MAX_HOLDINGS];

SH1106 display(0x3c, SDA_PIN, SCL_PIN);

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

// Replaces placeholder with stored values
// it looks for variables in format %var_name%
String processor(const String& var){
  //Serial.println(var);
  if(var == PARAM_LEDTHRESHOLD){
    return String(settings.LEDthreshold);
  }
  if(var == PARAM_BUZZERTHRESHOLD){
    return String(settings.buzzerThreshold);
  }
  for (int i = 0; i < CRYPTO_COUNT; i++) {
    if(var == settings.cryptos[i]){
      return "checked";
    }
  }
  for (int i = 0; i < 3; i++) {
    if(var == "screenChangeDelay" + String(settings.screenChangeDelay)){
      return "selected";
    }
  }
  return String();
}

void addNewHolding(String tickerId, float newPrice = 0, float oldPrice = 0) {
  int index = getNextFreeHoldingIndex();
  if (index > -1) {
    holdings[index].tickerId = tickerId;
    holdings[index].newPrice = newPrice;
    holdings[index].oldPrice = oldPrice;
    holdings[index].inUse = true;
    holdings[index].statsReadDue = 0;
  }
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());

  Serial.println(myWiFiManager->getConfigPortalSSID());
  
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawStringMaxWidth(0, 0, 128, F("Connect your phone to wifi network called:"));
  display.setFont(ArialMT_Plain_16);
  display.drawString(5, 22, ssidAP);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 37, F("and use password:"));
  display.setFont(ArialMT_Plain_16);
  display.drawString(5, 48, pwdAP);
  display.display();
}

void setup() {
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  Serial.begin(115200);
  while (!Serial) continue;
  pinMode(LED_PIN_DOWN, OUTPUT);
  pinMode(LED_PIN_UP, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Initialising the display
  display.init();
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_CENTER);

  display.clear();
  display.setFont(ArialMT_Plain_24);
  display.drawString(64, 8, F("#HODL"));
  display.drawString(64, 32, F("cube"));
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(128, 50, VER);
  display.display();
  delay(3000);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;  
  //reset saved settings - testing purpose
  //wm.resetSettings();
  //fetches ssid and pass from eeprom and tries to connect
  //if it does not connect it starts an access point with the specified name

  std::vector<const char *> menu = {
    "wifi",
    "info",
    "restart",
    "exit"};
  wm.setMenu(menu);
  wm.setAPCallback(configModeCallback);
  if (drd.detectDoubleReset()) {
    Serial.println("Double Reset Detected");
    wm.setConfigPortalTimeout(120);

    if (!wm.startConfigPortal(ssidAP, pwdAP)) {
      displayMessage("Failed to connect and hit timeout. Reseting in 5 secs.");
      delay(5000);
      //reset and try again, or maybe put it to deep sleep
      ESP.restart();
    }
  } else {
    Serial.println("No Double Reset Detected");
    if (wm.autoConnect(ssidAP, pwdAP)) {
      Serial.println("connected...yeey :)");
    } else {
      displayMessage("Connection error. No AP set. Will restart in 10 secs. " 
      "Follow instructions on screen and set network credentials.");
      delay(10000);
      ESP.restart();
    }
  }   

  // Initialize SPIFFS
  if(!SPIFFS.begin()){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Should load default config if run for the first time
  Serial.println(F("Loading settings..."));
  loadSettings(filename, settings);

  // Create settings file
  Serial.println(F("Saving settings..."));
  saveSettings(filename, settings);

  // Dump config file
  Serial.println(F("Print config file..."));
  printFile(filename);
  
  for (int i = 0; i < CRYPTO_COUNT; i++) {
    if (settings.cryptos[i] != "null") {    
      Serial.println("Added: " + settings.cryptos[i]);
      addNewHolding(settings.cryptos[i]);
    }
  }
  IPAddress ip = WiFi.localIP();
//  Serial.println(ip);

  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 4, F("Browse to"));
  display.drawString(64, 22, ip.toString());
  display.drawString(64, 40, F("for config"));
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
      settings.LEDthreshold = (float)request->getParam(PARAM_LEDTHRESHOLD)->value().toFloat();
    }
    if (request->hasParam(PARAM_BUZZERTHRESHOLD)) {
      settings.buzzerThreshold = request->getParam(PARAM_BUZZERTHRESHOLD)->value().toFloat();
    }
    if (request->hasParam(PARAM_SCREENCHANGEDELAY)) {
      settings.screenChangeDelay = (long)request->getParam(PARAM_SCREENCHANGEDELAY)->value().toInt();
    }
    for (int i = 0; i < CRYPTO_COUNT; i++) {
      String param = "crypto" + String(i);
      if (request->hasParam(param)) {
        settings.cryptos[i] = request->getParam(param)->value();
      } else {
        settings.cryptos[i] = "null";
      }
    }
    Serial.println(F("Saving configuration..."));
    saveSettings(filename, settings);
    
    // Dump config file
    Serial.println(F("Print config file..."));
    printFile(filename);
    
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

  CBPTickerResponse tickerResponse = holdings[index].lastTickerResponse;
  CBPStatsResponse statsResponse = holdings[index].lastStatsResponse;

  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  String tickerId = holdings[index].tickerId;
  tickerId.toUpperCase();
  // c++ char formatting using +/- sign
  char percent_change_24h[6];
  snprintf(percent_change_24h, sizeof(percent_change_24h), "%+3.1f", (tickerResponse.price / statsResponse.open - 1) * 100);
  display.drawString(64, 0, tickerId + " " + percent_change_24h + "%");
  display.setFont(ArialMT_Plain_24);
  float price = (float)tickerResponse.price;
  holdings[index].oldPrice = holdings[index].newPrice;
  holdings[index].newPrice = price;
  display.drawString(64, 20, formatCurrency(price));
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(2, 45, "Lo:" + String(statsResponse.low, 0));  
  display.drawString(2, 54, "Hi:" + String(statsResponse.high, 0));
  display.drawString(60, 45, "vol 24h:" + formatVolume(statsResponse.volume_24h));
  display.drawString(60, 54, "vol 30d:" + formatVolume(statsResponse.volume_30day));
 
  if (holdings[index].newPrice < holdings[index].oldPrice) {
    display.fillTriangle(110, 29, 124, 29, 117, 36);
  } else if (holdings[index].newPrice > holdings[index].oldPrice) {
    display.fillTriangle(110, 37, 124, 37, 117, 30);
  }
  display.display();
  if (holdings[index].newPrice < (holdings[index].oldPrice * (1.0 - settings.LEDthreshold / 100.0))) {
    LEDdown();
  }
  else if (holdings[index].newPrice > (holdings[index].oldPrice * (1.0 + settings.LEDthreshold / 100.0)) && holdings[index].oldPrice != 0) {
    LEDup();
  }
  if (holdings[index].newPrice < (holdings[index].oldPrice * (1.0 - settings.buzzerThreshold / 100.0))) {
    buzzerDown();
  }
  else if (holdings[index].newPrice > (holdings[index].oldPrice * (1.0 + settings.buzzerThreshold / 100.0)) && holdings[index].oldPrice != 0) {
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
  display.drawStringMaxWidth(2, 0, 126, message);
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
    pointsAfterDecimal = 5;
  }
  formattedCurrency.concat(String(price, pointsAfterDecimal));
  return formattedCurrency;
}

String formatVolume(float volume) {
  if (volume <= 999) {
    return String(volume, 1);
  } else if (volume > 999) {
    return String(volume/1000.0, 1) + "k";
  } else if (volume > 999999) {
    return String(volume/1000000.0, 1) + "M";
  } else if (volume > 999999999) {
    return String(volume/1000000000.0, 1) + "G";
  }
}

bool loadDataForHolding(int index, unsigned long timeNow) {
  int nextIndex = getNextIndex();
  if (nextIndex > -1 ) {
    holdings[index].lastTickerResponse = api.GetTickerInfo(holdings[index].tickerId);
    // stats reading every 30 s
    if (holdings[index].statsReadDue < timeNow) {
      holdings[index].lastStatsResponse = api.GetStatsInfo(holdings[index].tickerId);
      holdings[index].statsReadDue = timeNow + 30000;
    }    
    if (holdings[index].lastTickerResponse.error != "" || holdings[index].lastStatsResponse.error != "") {
      Serial.println("holdings[index].lastTickerResponse: " + String(holdings[index].lastTickerResponse.error));
      Serial.println("holdings[index].lastStatsResponse: " + String(holdings[index].lastStatsResponse.error));
    }
    return (holdings[index].lastTickerResponse.error == "" && holdings[index].lastStatsResponse.error == "");
  }

  return false;
}

void loop() {

  // Call the double reset detector loop method every so often,
  // so that it can recognise when the timeout expires.
  // You can also call drd.stop() when you wish to no longer
  // consider the next reset as a double reset.
  drd.loop();


  unsigned long timeNow = millis();
  if ((timeNow > screenChangeDue))  {
    currentIndex = getNextIndex();
    if (currentIndex > -1) {
//      Serial.print("Current holding index: ");
//      Serial.println(currentIndex);
      if (loadDataForHolding(currentIndex, timeNow)) {
        displayHolding(currentIndex);
        dataNotLoadedCounter = 0;
      } else {
        dataNotLoadedCounter++;
        Serial.println("Number of data loading errors in a row: ");
        Serial.println(dataNotLoadedCounter);
      }
      if (dataNotLoadedCounter > 5) {
        displayMessage(F("Error loading data. Check wifi connection or increase screen change delay in config."));
      }
    } else {
      displayMessage(F("No funds to display. Edit the setup to add them"));
    }
    screenChangeDue = timeNow + settings.screenChangeDelay;
  }
}
