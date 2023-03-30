#include "CoinbaseApi.h"

CoinbaseApi::CoinbaseApi(WiFiClientSecure &client)  {
  this->client = &client;
  responseTickerObject = CBPTickerResponse();
  responseStatsObject = CBPStatsResponse();
  responseCandlesObject = CBPCandlesResponse();
}

String CoinbaseApi::SendGetToCoinbase(String command) {
  String body="";
  body.reserve(700);
  bool finishedHeaders = false;
  bool currentLineIsBlank = true;
  long now;
  bool avail;
  int i;
  int j;
  
  client->setInsecure();
  if (client->connect(COINBASE_HOST, Port)) {
    // Serial.println(".... connected to server");
    char c;
    int ch_count=0;
    client->println("GET " + command + " HTTP/1.1");
    client->println("Host: " COINBASE_HOST);
    client->println(F("User-Agent: arduino/1.0.0"));
    client->println();
    now=millis();
    avail=false;
    while (millis()-now<1500) {
      while (client->available()) {
        char c = client->read();
        // Serial.write(c);

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
//         Serial.println("Body:");
//         Serial.println(body);
//         Serial.println("END");
        break;
      }
    }
    // search for left curly braces or square brackets
    for (i = 0; i < ch_count; i++) {
      if (body.charAt(i) == '{' || body.charAt(i) == '[') {
        break;
      }
    }
    // search for right curly braces or square brackets
    for (j = ch_count; j > i; j--) {
      if (body.charAt(j) == '}' || body.charAt(j) == ']') {
        break;
      }   
    }
  }
  closeClient();  
  return body.substring(i, j+1);
}

CBPTickerResponse CoinbaseApi::GetTickerInfo(String tickerId) {
  
  // https://api.exchange.coinbase.com/products/btc-eur/ticker  
  String commandTicker="/products/" + tickerId + "/ticker";
  
  String responseTicker = SendGetToCoinbase(commandTicker);
  Serial.println(String(tickerId) + " ticker: " + String(responseTicker));
  
  StaticJsonDocument<256> ticker;
  
  // Deserialize the JSON document
  DeserializationError errorTicker = deserializeJson(ticker, responseTicker);
  
  // Test if parsing succeeds.
  if (errorTicker) {
    responseTickerObject.error = errorTicker.f_str();
//    Serial.print(F("deserializeJson() failed: "));
//    Serial.println(errorTicker.f_str());
    return responseTickerObject;
  }

  // Fetch values
//    /ticker: {"trade_id":41905060,"price":"47892.67","size":"0.00201603","time":"2021-05-05T16:52:24.740835Z","bid":"47885.33","ask":"47892.67","volume":"2016.48486351"}
  responseTickerObject.price = ticker["price"].as<float>();
  responseTickerObject.error = "";

  return responseTickerObject;
}

CBPStatsResponse CoinbaseApi::GetStatsInfo(String tickerId) {

  // https://api.exchange.coinbase.com/products/btc-eur/stats
  String commandStats="/products/" + tickerId + "/stats";
  
  String responseStats = SendGetToCoinbase(commandStats);
  Serial.println(String(tickerId) + " stats: " + String(responseStats));
  
  StaticJsonDocument<256> stats;
  
  // Deserialize the JSON document
  DeserializationError errorStats = deserializeJson(stats, responseStats);

  // Test if parsing succeeds.
  if (errorStats) {
    responseStatsObject.error = errorStats.f_str();
//    Serial.print(F("deserializeJson() failed: "));
//    Serial.println(errorStats.f_str());
    return responseStatsObject;
  }

  // Fetch values
  //    /stats: {"open":"40680","high":"40789.98","low":"36445","volume":"3771.22613578","last":"39082.37","volume_30day":"102943.0112141"}
  responseStatsObject.open = stats["open"].as<float>();
  responseStatsObject.high = stats["high"].as<float>();
  responseStatsObject.low = stats["low"].as<float>();
  responseStatsObject.volume_24h = stats["volume"].as<float>();
  responseStatsObject.volume_30day = stats["volume_30day"].as<float>();
  responseStatsObject.error = "";
 
  return responseStatsObject;
}

CBPCandlesResponse CoinbaseApi::GetCandlesInfo(String tickerId, String date) {
  // https://api.exchange.coinbase.com/products/btc-eur/candles?granularity=86400&start=2021-01-01&end=2021-01-01
  String commandCandles="/products/" + tickerId + "/candles?granularity=86400&start=" + date + "&end=" + date;
  
  String responseCandles = SendGetToCoinbase(commandCandles);
  Serial.println(String(tickerId) + " candles " + date +": " + String(responseCandles));

  StaticJsonDocument<256> candles;

  // Deserialize the JSON document
  DeserializationError errorCandles = deserializeJson(candles, responseCandles);  

  // Test if parsing succeeds.
  if (errorCandles) {
    responseCandlesObject.error = errorCandles.f_str();
//    Serial.print(F("deserializeJson() failed: "));
//    Serial.println(errorCandles.f_str());
    return responseCandlesObject;
  }

  // Fetch values
  // [[1609459200,23512.7,24250,23706.73,24070.97,1830.04655405]]
  responseCandlesObject.open = candles[0][3].as<float>();
  responseCandlesObject.error = "";
 
  return responseCandlesObject;
}

void CoinbaseApi::closeClient() {
  if(client->connected()){
    client->stop();
  }
}
