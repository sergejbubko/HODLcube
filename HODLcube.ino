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
// this fork is necessary https://github.com/taranais/NTPClient
#include <NTPClient.h>
#include <WiFiUdp.h>
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
#define GREEN_LED D5
#define RED_LED D6

// Buzzer
#define BUZZER_PIN D7

// Display power
#define DISPLAY_POWER_PIN D8

// Have tested up to 5, can probably do more
#define MAX_HOLDINGS 5

// number of crypto showing in config portal
#define PAIRS_COUNT 16

// Number of seconds after reset during which a 
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 10

// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

// Time intervals in millis
#define MINUTE_INTERVAL 60000
#define HOUR_INTERVAL 3600000
#define DAY_INTERVAL 86400000

const char* VER = "v0.4.0";

struct Holding {
  String tickerId;
  float newPrice;
  float oldPrice;
  bool inUse;
  unsigned long statsReadDue;
  unsigned long weekAgoPriceReadDue;
  unsigned long YTDPriceReadDue;
  CBPTickerResponse lastTickerResponse;
  CBPStatsResponse lastStatsResponse;
  CBPCandlesResponse weekAgoPriceResponse;
  CBPCandlesResponse YTDPriceResponse;
  float priceCheckpoint;
};

struct Settings {
  float LEDtickThresh; // LED threshold for two prices in a row (ticker) in percent
  float buzzTickThresh; // buzzer threshold for two prices in a row (ticker) in percent
  float buzzCPThresh; // buzzer threshold for difference between checkpoint price and current price in percent
// We'll request a new value just before we change the screen so it's the most up to date
// Rate Limits: https://docs.cloud.coinbase.com/exchange/docs/rate-limits
// We throttle public endpoints by IP: 10 requests per second, up to 15 requests per second in bursts. Some endpoints may have custom rate limits.
  unsigned long screenChangeDelay; // milis
  String pairs[MAX_HOLDINGS];
};

char ssidAP[] = "HODLcube";  // SSID of the device
char pwdAP[] = "toTheMoon";  // password of the device

unsigned long screenChangeDue = 0;
unsigned long checkDateTimeInterval = 0;

int dataNotLoadedCounter = 0;

String dateWeekAgo = "";
String currentYear = "";
const char *filename = "/settings.txt";
Settings settings;                         // <- global settings object
int currentIndex = -1;

AsyncWebServer server(80);
WiFiClientSecure clientSSL;
CoinbaseProApi api(clientSSL);
DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);
Holding holdings[MAX_HOLDINGS];

SH1106 display(0x3c, SDA_PIN, SCL_PIN);

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// Loads the settings from a file
void loadSettings(const char *filename, Settings &settings) {
  // Open file for reading
  File file = SPIFFS.open(filename, "r");
  if (!file) {
    Serial.println(F("ERR: Failed to read file, using default values"));
    settings.LEDtickThresh = 0.01;
    // threshold for difference of last two loaded prices in a row in percent
    settings.buzzTickThresh = 0.01; 
    // threshold for daily price change in percent
    settings.buzzCPThresh = 5.0;
    // time in milis to reload new prices and/or another pair from list
    settings.screenChangeDelay = 5000;  
    // list of cryptocurrencies to choose from
    settings.pairs[0] = String("BTC-USD"); 
    settings.pairs[1] = String("null"); 
    settings.pairs[2] = String("null"); 
    settings.pairs[3] = String("null"); 
    settings.pairs[4] = String("null"); 
    return;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use www.arduinojson.org/v6/assistant to compute the capacity.
  StaticJsonDocument<768> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.print(F("ERR: Failed to deserialize json - "));
    Serial.println(error.f_str());
  }

  // Copy values from the JsonDocument to the Config
  // threshold for difference of last two loaded prices in a row in percent
  settings.LEDtickThresh = doc["LEDtickThresh"] | 0.01;
  // threshold for difference of last two loaded prices in a row in percent
  settings.buzzTickThresh = doc["buzzTickThresh"] | 0.01; 
  // threshold for daily price change in percent
  settings.buzzCPThresh = doc["buzzCPThresh"] | 5.0;
  // time in milis to reload new prices and/or another pair from list
  settings.screenChangeDelay = doc["screenChangeDelay"] | 5000;  
  // list of cryptocurrencies to choose from
  settings.pairs[0] = String(doc["pair0"] | "BTC-USD");  
  for (int i = 1; i < MAX_HOLDINGS; i++) {
    settings.pairs[i] = String(doc["pair" + String(i)]);  
  }
  
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
    Serial.println(F("ERR: Failed to create file"));
    return;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use www.arduinojson.org/assistant to compute the capacity.
  StaticJsonDocument<512> doc;
  
  // Set the values in the document 
  doc["LEDtickThresh"] = settings.LEDtickThresh;
  doc["buzzTickThresh"] = settings.buzzTickThresh;
  doc["buzzCPThresh"] = settings.buzzCPThresh;
  doc["screenChangeDelay"] = settings.screenChangeDelay;  
  for (int i = 0; i < MAX_HOLDINGS; i++) {
    doc["pair" + String(i)] = settings.pairs[i];
  } 
  
  // Serialize JSON to file
  if (serializeJson(doc, file) == 0) {
    Serial.println(F("ERR: Failed to serialize json to file"));
  }
  // Close the file
  file.close();
}

// Loads the settings from a file
float loadCheckpointPrice(String filename) {
  // Open file for reading
  File file = SPIFFS.open(filename, "r");
  if (!file) {
    Serial.print(F("ERR: Failed to read file - "));
    Serial.println(filename);
    return 0.0;
  }

  float fileContent;
  while(file.available()){
    fileContent = file.parseFloat();
  }
  
// Close the file
  file.close();
  return fileContent;
}

// Saves the settings to a file
void saveCheckpointPrice(String filename, const float price) {
  // Delete existing file, otherwise the settings is appended to the file
  SPIFFS.remove(filename);

  // Open file for writing
  File file = SPIFFS.open(filename, "w");
  if (!file) {
    Serial.println(F("ERR. Failed to create file"));
    return;
  }

  if(!file.print(price)){
    Serial.print(F("ERR. File write failed: "));
    Serial.println(file.name());
  } else {
    Serial.print(F("New checkpoint saved to file "));
    Serial.print(file.name());
    Serial.print("-> ");
    Serial.println(price);
  }
  // Close the file
  file.close();
}

// Prints the content of a file to the Serial
void printFile(const char *filename) {
  // Open file for reading
  File file = SPIFFS.open(filename, "r");
  if (!file) {
    Serial.println(F("ERR: Failed to read file"));
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

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

// Replaces placeholder with stored values
// it looks for variables in format %var_name%
String processor(const String& var){
  //Serial.println(var);
  if(var == "INPUT_LED_TICK_THRESH"){
    return String(settings.LEDtickThresh);
  }
  if(var == "INPUT_BUZZ_TICK_THRESH"){
    return String(settings.buzzTickThresh);
  }
  if(var == "INPUT_BUZZ_CP_THRESH"){
    return String(settings.buzzCPThresh);
  }
  if(var == "SCREEN_CHANGE_DELAY" + String(settings.screenChangeDelay)){
     return "selected";
  }
  if(var == "CURRENCY_PAIRS") {
    String result = "";
    for (int i = 0; i < MAX_HOLDINGS; i++) {
      result += "'" + settings.pairs[i] + "',";
    }
    return result;
  }
  if (var == "CURRENCY_CHECKPOINTS") {
    String result;
    result.reserve(1200);
    for (int i = 0; i < MAX_HOLDINGS; i++) {
      if (holdings[i].inUse && holdings[i].priceCheckpoint >= 0.1) {
         result +=
         "<div class='mb-2'>"
            "<label for='chkp_" + holdings[i].tickerId + "' class='col-form-label'>" + holdings[i].tickerId + "</label>"
            "<div class='mb-2'>"
              "<input type='number' name='chkp_" + holdings[i].tickerId + "' step='0.01' min='0.01' value='" + String(holdings[i].priceCheckpoint) + "' class='form-control' required>"
            "</div>"
          "</div>";
      }
    }
    return result;
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
    holdings[index].weekAgoPriceReadDue = 0;
    holdings[index].priceCheckpoint = loadCheckpointPrice("/" + tickerId + ".txt");
  }
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println(F("Entered config mode"));
  Serial.println(WiFi.softAPIP());

  Serial.println(myWiFiManager->getConfigPortalSSID());
  
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawStringMaxWidth(0, 0, 128, F("Use phone/PC and connect to wifi SSID:"));
  display.setFont(ArialMT_Plain_16);
  display.drawString(5, 22, ssidAP);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 37, F("password:"));
  display.setFont(ArialMT_Plain_16);
  display.drawString(5, 48, pwdAP);
  display.display();
}

void setup() {
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  Serial.begin(115200);
  while (!Serial) continue;
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Initialising the display
  display.init();
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_CENTER);

  display.clear();
  display.drawXbm(27, 5, MAINLOGO_WIDTH, MAINLOGO_HEIGHT, mainLogo);
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(128, 54, VER);
  display.display();
  //delay(2000);

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
    Serial.println(F("Double Reset Detected"));
    wm.setConfigPortalTimeout(120);

    if (!wm.startConfigPortal(ssidAP, pwdAP)) {
      displayMessage("Failed to connect and hit timeout. Reseting in 5 secs.");
      delay(5000);
      //reset and try again, or maybe put it to deep sleep
      ESP.restart();
    }
  } else {
    Serial.println(F("No Double Reset Detected"));
    if (wm.autoConnect(ssidAP, pwdAP)) {
      Serial.println(F("connected...yeey :)"));
    } else {
      displayMessage(F("Connection error. No AP set. Will restart in 10 secs. " 
      "Follow instructions on screen and set network credentials."));
      delay(10000);
      ESP.restart();
    }
  }   

  // Initialize SPIFFS
  if(!SPIFFS.begin()){
    Serial.println(F("An Error has occurred while mounting SPIFFS"));
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

  for (int i = 0; i < MAX_HOLDINGS; i++) {
    if (settings.pairs[i] != "null") {
      Serial.println("Added #" + String(i) + ":" + settings.pairs[i]);
      addNewHolding(settings.pairs[i]);
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
    // GET inputLEDtickThresh value on <ESP_IP>/get?inputLEDtickThresh=<inputMessage>
    int params = request->params();
    int j = 0;
    for(int i=0;i<params;i++) {
      AsyncWebParameter* p = request->getParam(i);
      if (p->name() == "inputLEDtickThresh") {
        settings.LEDtickThresh = p->value().toFloat();
      } else if (p->name() == "inputBuzzTickThresh") {
        settings.buzzTickThresh = p->value().toFloat();
      } else if (p->name() == "inputBuzzCPThresh") {
        settings.buzzCPThresh = p->value().toFloat();
      } else if (p->name() == "inputScreenChangeDelay") {
        settings.screenChangeDelay = (long)p->value().toInt();
      } else if (p->name() == "inputCurrencyPairs") {
        if (j < MAX_HOLDINGS) {
          settings.pairs[j] = p->value();
          j++;
        }
      } else if (p->name().startsWith("chkp_")) {
        for (int i = 0; i < MAX_HOLDINGS; i++) {
          if (holdings[i].inUse && p->name() == "chkp_" + holdings[i].tickerId &&
          fabs(holdings[i].priceCheckpoint - p->value().toFloat()) >= 0.01) {
            Serial.println(holdings[i].priceCheckpoint, 10);
            Serial.println(p->value().toFloat(), 10);
            holdings[i].priceCheckpoint = p->value().toFloat();   
            saveCheckpointPrice("/" + holdings[i].tickerId + ".txt", holdings[i].priceCheckpoint);
          }
        }
      }
    }
    // reset other values
    while(j < MAX_HOLDINGS) {
      settings.pairs[j] = "null";
      j++;
    }
    Serial.println(F("Saving configuration..."));
    saveSettings(filename, settings);
    
    // Dump config file
    Serial.println(F("Print config file..."));
    printFile(filename);
    
    request->send(200);
    ESP.restart();
  });
  server.onNotFound(notFound);
  server.begin();
  
  // Initialize a NTPClient to get time
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  timeClient.setTimeOffset(0);
  
  delay(2000);
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
  display.drawString(2, 45, F("L:"));
  display.drawString(15, 45, formatCurrency(statsResponse.low));  
  display.drawString(2, 54, F("H:"));
  display.drawString(15, 54, formatCurrency(statsResponse.high));
  // c++ char formatting using +/- sign
  char percent_change_7d[8] = "N/A";
  if (holdings[index].weekAgoPriceResponse.open) {
    snprintf(percent_change_7d, sizeof(percent_change_7d), "%+3.1f", (tickerResponse.price / holdings[index].weekAgoPriceResponse.open - 1) * 100);
  }
  display.drawString(55, 45, F("7d: "));
  display.drawString(82, 45, String(percent_change_7d) + "%");
  // c++ char formatting using +/- sign
  char percent_change_YTD[8] = "N/A";
  if (holdings[index].YTDPriceResponse.open) {
    snprintf(percent_change_YTD, sizeof(percent_change_YTD), "%+3.1f", (tickerResponse.price / holdings[index].YTDPriceResponse.open - 1) * 100);
  }
  display.drawString(55, 54, F("YTD: "));
  display.drawString(82, 54, String(percent_change_YTD) + "%");
 
  if (holdings[index].newPrice < holdings[index].oldPrice) {
    display.fillTriangle(110, 29, 124, 29, 117, 36);
  } else if (holdings[index].newPrice > holdings[index].oldPrice) {
    display.fillTriangle(110, 37, 124, 37, 117, 30);
  }
  display.display();
  if (holdings[index].newPrice < (holdings[index].oldPrice * (1.0 - settings.LEDtickThresh / 100.0))) {
    LEDdown();
  }
  else if (holdings[index].newPrice > (holdings[index].oldPrice * (1.0 + settings.LEDtickThresh / 100.0)) && holdings[index].oldPrice != 0) {
    LEDup();
  }
  if (holdings[index].newPrice < (holdings[index].oldPrice * (1.0 - settings.buzzTickThresh / 100.0))) {
    buzzerDown();
  }
  else if (holdings[index].newPrice > (holdings[index].oldPrice * (1.0 + settings.buzzTickThresh / 100.0)) && holdings[index].oldPrice != 0) {
    buzzerUp();
  }  
  float priceTrend = isCPThreshReached(index);
  if (priceTrend != 0.0) {
    buzzer(priceTrend, index);
  }
}

void LEDdown(){    
  // Flash LED
  digitalWrite(RED_LED, HIGH);
  delay(50);
  digitalWrite(RED_LED, LOW);
}
void LEDup(){    
  // Flash LED
  digitalWrite(GREEN_LED, HIGH);
  delay(50);
  digitalWrite(GREEN_LED, LOW); 
}

void buzzer(float priceTrend, int index){    
  // Sound buzzer DI-DI-DI-DI-DI
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_24);
  String tickerId = holdings[index].tickerId;
  tickerId.toUpperCase();
  display.drawString(64, 6, tickerId);
  char priceChange[6];
  snprintf(priceChange, sizeof(priceChange), "%+3.1f", priceTrend);
  //display.drawString(64, 21, String(holdings[index].priceCheckpoint) + "->" + String(holdings[index].newPrice));
  display.drawString(64, 32, String(priceChange) + "%");
  display.display();
  for (int i = 0; i < 5; i++) {
    digitalWrite(RED_LED,HIGH) ;
    digitalWrite(GREEN_LED,HIGH) ;
    digitalWrite(BUZZER_PIN,HIGH) ;
    delay (50); 
    digitalWrite(RED_LED,LOW) ;
    digitalWrite(GREEN_LED,LOW) ;
    digitalWrite(BUZZER_PIN,LOW) ;
    delay (75); 
  }    
  delay(7500);
}

void buzzerUp(){
  // Sound buzzer DI-DI
  digitalWrite(BUZZER_PIN,HIGH) ;//Turn on active buzzer
  delay (25); 
  digitalWrite(BUZZER_PIN,LOW) ; //Turn off active buzzer
  delay (25); 
  digitalWrite(BUZZER_PIN,HIGH) ; //Turn on active buzzer
  delay (50); 
  digitalWrite(BUZZER_PIN,LOW) ; //Turn off active buzzer
}

void buzzerDown(){
  // Sound buzzer DI
  digitalWrite(BUZZER_PIN,HIGH) ; //Turn on active buzzer
  delay (25); 
  digitalWrite(BUZZER_PIN,LOW) ; //Turn off active buzzer
}

float isCPThreshReached (int index) {  
  float result = 0.0;
  if ((holdings[index].priceCheckpoint * (1.0 + settings.buzzCPThresh / 100) < holdings[index].newPrice) || 
     (holdings[index].priceCheckpoint * (1.0 - settings.buzzCPThresh / 100) > holdings[index].newPrice)) {
    result = holdings[index].priceCheckpoint == 0.0 ? 0.0 : (holdings[index].newPrice / holdings[index].priceCheckpoint - 1.0) * 100.0;
    holdings[index].priceCheckpoint = holdings[index].newPrice;
    saveCheckpointPrice("/" + holdings[index].tickerId + ".txt", holdings[index].newPrice);
    Serial.print(String(holdings[index].tickerId) + " - new price checkpoint: ");
    Serial.println(holdings[index].priceCheckpoint);
  }
  return result;
}

void displayMessage(String message){
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawStringMaxWidth(2, 0, 126, message);
  display.display();
}

String formatCurrency(float price) {
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
  if (volume <= 999.9) {
    return String(volume, 1);
  } else if (volume > 999.9 and volume <= 999999.9) {
    return String(volume/1000.0, 1) + "k";
  } else if (volume > 999999.9 and volume <= 999999999.9) {
    return String(volume/1000000.0, 1) + "M";
  } else if (volume > 999999999.9) {
    return String(volume/1000000000.0, 1) + "G";
  }
}

void updateDate(void) {
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  dateWeekAgo = timeClient.getFormattedDate(epochTime - 604800L);
  dateWeekAgo.remove(dateWeekAgo.indexOf("T"));
  currentYear = timeClient.getFormattedDate().substring(0, 4);

  Serial.print(F("Current time: "));
  Serial.println(timeClient.getFormattedTime());  
  Serial.print(F("Current date: "));
  Serial.println(timeClient.getFormattedDate());  
  Serial.print(F("Week ago date: "));
  Serial.println(dateWeekAgo);  
}

bool loadDataForHolding(int index, unsigned long timeNow) {
  int nextIndex = getNextIndex();
  if (nextIndex > -1 ) {
    holdings[index].lastTickerResponse = api.GetTickerInfo(holdings[index].tickerId);
    // stats reading every 30 s or more
    if (holdings[index].statsReadDue < timeNow) {
      holdings[index].lastStatsResponse = api.GetStatsInfo(holdings[index].tickerId);
      holdings[index].statsReadDue = timeNow + MINUTE_INTERVAL / 2;
    }    
    if (holdings[index].weekAgoPriceReadDue < timeNow) {
      holdings[index].weekAgoPriceResponse = api.GetCandlesInfo(holdings[index].tickerId, dateWeekAgo);
      holdings[index].weekAgoPriceReadDue = timeNow + HOUR_INTERVAL;
    }
    if (holdings[index].YTDPriceReadDue < timeNow) {
      holdings[index].YTDPriceResponse = api.GetCandlesInfo(holdings[index].tickerId, currentYear + "-01-01");
      holdings[index].YTDPriceReadDue = timeNow + DAY_INTERVAL / 2;
    }
    if (holdings[index].lastTickerResponse.error != "") {
      Serial.println("ERR: holdings[index].lastTickerResponse: " + String(holdings[index].lastTickerResponse.error));
    }
    if (holdings[index].lastStatsResponse.error != "") {
      Serial.println("ERR: holdings[index].lastStatsResponse: " + String(holdings[index].lastStatsResponse.error));
    }
    if (holdings[index].weekAgoPriceResponse.error != "") {
      Serial.println("ERR: holdings[index].weekAgoPriceResponse: " + String(holdings[index].weekAgoPriceResponse.error));
    }
    if (holdings[index].YTDPriceResponse.error != "") {
      Serial.println("ERR: holdings[index].YTDPriceResponse: " + String(holdings[index].YTDPriceResponse.error));
    }
    return (holdings[index].lastTickerResponse.error == "" && holdings[index].lastStatsResponse.error == "" 
    && holdings[index].weekAgoPriceResponse.error == "" && holdings[index].YTDPriceResponse.error == "");
  }

  return false;
}

void loop() {

  // so that it can recognise when the timeout expires.
  // You can also call drd.stop() when you wish to no longer
  // consider the next reset as a double reset.
  drd.loop();

  unsigned long timeNow = millis();
  if (timeNow > checkDateTimeInterval) {
    updateDate();    
    // check date and time every hour
    checkDateTimeInterval = timeNow + HOUR_INTERVAL;
  }
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
        Serial.print(F("Number of data loading errors in a row: "));
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
