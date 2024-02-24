/*
Copyright (c) 2017 Brian Lough. All right reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/


#ifndef CoinbaseApi_h
#define CoinbaseApi_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

#define COINBASE_HOST "api.exchange.coinbase.com"

struct CBPTickerResponse{
  float price;
  String error;
};

struct CBPStatsResponse{
  float open;
  float high;
  float low;
  String error;
};

struct CBPCandlesResponse{
  float open;
  String error;
};


class CoinbaseApi
{
  public:
    CoinbaseApi (WiFiClientSecure &client);
    String SendGetToCoinbase(String &command);
    CBPTickerResponse GetTickerInfo(String &coinId);
    CBPStatsResponse GetStatsInfo(String &coinId);
    CBPCandlesResponse GetCandlesInfo(String &tickerId, String &date);
    int Port = 443;

  private:
    CBPTickerResponse responseTickerObject;
    CBPStatsResponse responseStatsObject;
    CBPCandlesResponse responseCandlesObject;
    WiFiClientSecure *client;
    void closeClient();
};

#endif
