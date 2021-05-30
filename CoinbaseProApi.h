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


#ifndef CoinbaseProApi_h
#define CoinbaseProApi_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

#define COINBASEPRO_HOST "api.pro.coinbase.com"

struct CBPTickerResponse{
  float price;
  String error;
};

struct CBPStatsResponse{
  float open;
  float high;
  float low;
  float volume_24h;
  float volume_30day;
  String error;
};


class CoinbaseProApi
{
  public:
    CoinbaseProApi (WiFiClientSecure &client);
    String SendGetToCoinbasePro(String command);
    CBPTickerResponse GetTickerInfo(String coinId, String currency = "");
    CBPStatsResponse GetStatsInfo(String coinId, String currency = "");
    int Port = 443;

  private:
    CBPTickerResponse responseTickerObject;
    CBPStatsResponse responseStatsObject;
    WiFiClientSecure *client;
    void closeClient();
};

#endif
