#include "CoinBaseProApi.h"

CoinBaseProApi::CoinBaseProApi(WiFiClientSecure &client)  {
  this->client = &client;
  responseObject = CMCTickerResponse();
  getStatsDue = 0;
}

String CoinBaseProApi::SendGetToCoinBasePro(String command) {
  String body="";
  body.reserve(700);
  bool finishedHeaders = false;
  bool currentLineIsBlank = true;
  long now;
  bool avail;
  
  client->setInsecure();
  if (client->connect(COINBASEPRO_HOST, Port)) {
    // Serial.println(".... connected to server");
    String a="";
    char c;
    int ch_count=0;
    client->println("GET " + command + " HTTP/1.1");
    client->println("Host: " COINBASEPRO_HOST);
    client->println(F("User-Agent: arduino/1.0.0"));
    client->println();
    now=millis();
    avail=false;
    while (millis()-now<1500) {
      while (client->available()) {
        char c = client->read();
        //Serial.write(c);

        if(!finishedHeaders){
          if (currentLineIsBlank && c == '\n') {
            finishedHeaders = true;
          }
        } else {
          body=body+c;
          ch_count++;
        }

        if (c == '\n') {
          currentLineIsBlank = true;
        }else if (c != '\r') {
          currentLineIsBlank = false;
        }

        avail=true;
      }
      if (avail) {
        // Serial.println("Body:");
        // Serial.println(body);
        // Serial.println("END");
        break;
      }
    }
  }
  closeClient();
  return body;
}

CMCTickerResponse CoinBaseProApi::GetTickerInfo(String coinId, String currency) {
  unsigned long timeNow = millis();
  DeserializationError errorStats = DeserializationError::Ok;
  
  // https://api.pro.coinbase.com/products/btc-eur/ticker  
  String commandTicker="/products/" + coinId + "-" + currency + "/ticker";

  if (timeNow > getStatsDue) {
    // https://api.pro.coinbase.com/products/btc-eur/stats
    String commandStats="/products/" + coinId + "-" + currency + "/stats";
    String responseStats = SendGetToCoinBasePro(commandStats);
    StaticJsonDocument<256> stats;
    DeserializationError errorStats = deserializeJson(stats, responseStats);
    //    /stats: {"open":"40680","high":"40789.98","low":"36445","volume":"3771.22613578","last":"39082.37","volume_30day":"102943.0112141"}
    responseObject.open = stats["open"].as<float>();
    responseObject.high = stats["high"].as<float>();
    responseObject.low = stats["low"].as<float>();
    responseObject.volume_24h = stats["volume"].as<float>();
    responseObject.last = stats["last"].as<float>();
    responseObject.volume_30day = stats["volume_30day"].as<float>();
    getStatsDue = timeNow + 20000;
    Serial.println(responseStats);
  }
  // Serial.println(command);  
  String responseTicker = SendGetToCoinBasePro(commandTicker);
  Serial.println(responseTicker); 
  
  StaticJsonDocument<256> ticker;
  
  // Deserialize the JSON document
  DeserializationError errorTicker = deserializeJson(ticker, responseTicker);
  
  // Test if parsing succeeds.
  if (errorStats || errorTicker) {
    responseObject.error = "deserializeJson() failed: " + String("error.f_str()");
//    Serial.print(F("deserializeJson() failed: "));
//    Serial.println(error.f_str());
    return responseObject;
  }

  // Fetch values.
  //
  // Most of the time, you can rely on the implicit casts.
  // In other case, you can do doc["time"].as<long>();
  // const char* sensor = doc["sensor"];

//    /ticker: {"trade_id":41905060,"price":"47892.67","size":"0.00201603","time":"2021-05-05T16:52:24.740835Z","bid":"47885.33","ask":"47892.67","volume":"2016.48486351"}
  responseObject.price = ticker["price"].as<float>();

  return responseObject;
}

void CoinBaseProApi::closeClient() {
  if(client->connected()){
    client->stop();
  }
}
