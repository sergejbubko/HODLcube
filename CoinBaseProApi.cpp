#include "CoinBaseProApi.h"

CoinBaseProApi::CoinBaseProApi(WiFiClientSecure &client)  {
  this->client = &client;
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
  // https://api.pro.coinbase.com/products/btc-eur/stats
  String command="/products/" + coinId + "-" + currency + "/stats";
//  if (currency != "") {
//    command = command + "?convert=" + currency;
//  }

  // Serial.println(command);
  String response = SendGetToCoinBasePro(command);
  CMCTickerResponse responseObject = CMCTickerResponse();
  StaticJsonDocument<256> doc;
  
  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, response);
  
  // Test if parsing succeeds.
  if (error) {
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

//    {"open":"40680","high":"40789.98","low":"36445","volume":"3771.22613578","last":"39082.37","volume_30day":"102943.0112141"}
      responseObject.open = doc["open"].as<double>();
      responseObject.high = doc["high"].as<double>();
      responseObject.low = doc["low"].as<double>();
      responseObject.volume = doc["volume"].as<double>();
      responseObject.last = doc["last"].as<double>();
      responseObject.volume_30day = doc["volume_30day"].as<double>();

      
//    responseObject.id = root[0]["id"].as<String>();
//    responseObject.name = root[0]["name"].as<String>();
//    responseObject.symbol = root[0]["symbol"].as<String>();
//    responseObject.rank = root[0]["rank"].as<int>();
//    responseObject.price_usd = root[0]["price_usd"].as<double>();
//
//    responseObject.price_btc = root[0]["price_btc"].as<double>();
//    responseObject.volume_usd_24h = root[0]["24h_volume_usd"].as<double>();
//    responseObject.market_cap_usd = root[0]["market_cap_usd"].as<double>();
//    responseObject.available_supply = root[0]["available_supply"].as<double>();
//    responseObject.total_supply = root[0]["total_supply"].as<double>();
//
//    responseObject.percent_change_1h = root[0]["percent_change_1h"].as<double>();
//    responseObject.percent_change_24h = root[0]["percent_change_24h"].as<double>();
//    responseObject.percent_change_7d = root[0]["percent_change_7d"].as<double>();
//    responseObject.last_updated = root[0]["last_updated"].as<double>();

//    currency.toLowerCase();
//    responseObject.price_currency = root[0]["price_" + currency].as<double>();
//    responseObject.volume_currency_24h = root[0]["volume_" + currency + "_24h"].as<double>();
//    responseObject.market_cap_currency = root[0]["market_cap_" + currency].as<double>();

  return responseObject;
}

void CoinBaseProApi::closeClient() {
  if(client->connected()){
    client->stop();
  }
}
