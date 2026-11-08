
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>
#include "secrets.h"

const char* API_BASE = "https://api.football-data.org/v4/teams/";

#define SERIAL_BAUD 9600

const unsigned long WIFI_TIMEOUT_MS = 20000;
const unsigned long HTTP_TIMEOUT_MS = 15000;

char inputBuffer[40];
byte inputIndex = 0;

struct Club {
  int id;
  const char* name;
};

Club teams[] = {
  {57, "Arsenal"},
  {65, "Man City"},
  {64, "Liverpool"},
  {86, "Real Madrid"},
  {81, "Barcelona"},
  {66, "Man United"}
};

const int TEAM_COUNT = 6;

bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startTime < WIFI_TIMEOUT_MS) {
    delay(300);
  }

  return WiFi.status() == WL_CONNECTED;
}

bool isValidTeam(int teamId) {
  for (int i = 0; i < TEAM_COUNT; i++) {
    if (teams[i].id == teamId) {
      return true;
    }
  }

  return false;
}

String getTeamName(JsonObject team) {
  const char* shortName = team["shortName"] | "";

  if (strlen(shortName) > 0) {
    return String(shortName);
  }

  const char* name = team["name"] | "Unknown";
  return String(name);
}

bool fetchMatch(int teamId, const char* matchStatus) {
  if (!connectWiFi()) {
    Serial.println("ERROR|WiFi connection failed");
    return false;
  }

  String url = String(API_BASE) + String(teamId) +
               "/matches?status=" + String(matchStatus) +
               "&limit=1";

  BearSSL::WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);

  if (!http.begin(secureClient, url)) {
    Serial.println("ERROR|HTTP begin failed");
    return false;
  }

  http.addHeader("X-Auth-Token", API_TOKEN);
  http.addHeader("Accept", "application/json");

  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.print("ERROR|HTTP ");
    Serial.println(httpCode);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(18000);
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.print("ERROR|JSON ");
    Serial.println(error.c_str());
    return false;
  }

  JsonArray matches = doc["matches"].as<JsonArray>();

  if (matches.isNull() || matches.size() == 0) {
    return false;
  }

  JsonObject match = matches[0];

  String home = getTeamName(match["homeTeam"].as<JsonObject>());
  String away = getTeamName(match["awayTeam"].as<JsonObject>());

  const char* dateValue = match["utcDate"] | "";
  String date = String(dateValue);

  if (strcmp(matchStatus, "FINISHED") == 0) {
    int homeScore = match["score"]["fullTime"]["home"] | -1;
    int awayScore = match["score"]["fullTime"]["away"] | -1;

    Serial.print("LAST|");
    Serial.print(home);
    Serial.print("|");
    Serial.print(away);
    Serial.print("|");
    Serial.print(date);
    Serial.print("|");

    if (homeScore < 0) {
      Serial.print("-");
    } else {
      Serial.print(homeScore);
    }

    Serial.print("|");

    if (awayScore < 0) {
      Serial.println("-");
    } else {
      Serial.println(awayScore);
    }

    return true;
  }

  if (strcmp(matchStatus, "SCHEDULED") == 0) {
    Serial.print("NEXT|");
    Serial.print(home);
    Serial.print("|");
    Serial.print(away);
    Serial.print("|");
    Serial.println(date);
    return true;
  }

  return false;
}

void processTeamRequest(int teamId) {
  if (!isValidTeam(teamId)) {
    Serial.println("ERROR|Invalid team ID");
    Serial.println("END");
    return;
  }

  if (!connectWiFi()) {
    Serial.println("ERROR|WiFi connection failed");
    Serial.println("END");
    return;
  }

  bool lastFound = fetchMatch(teamId, "FINISHED");

  delay(500);

  bool nextFound = fetchMatch(teamId, "SCHEDULED");

  if (!lastFound && !nextFound) {
    Serial.println("ERROR|No match data found");
  }

  Serial.println("END");
}

void processCommand(String command) {
  command.trim();

  if (!command.startsWith("TEAM|")) {
    return;
  }

  int teamId = command.substring(5).toInt();

  if (teamId <= 0) {
    Serial.println("ERROR|Invalid team ID");
    Serial.println("END");
    return;
  }

  processTeamRequest(teamId);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  connectWiFi();
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      inputBuffer[inputIndex] = '\0';

      if (inputIndex > 0) {
        processCommand(String(inputBuffer));
      }

      inputIndex = 0;
    } else {
      if (inputIndex < sizeof(inputBuffer) - 1) {
        inputBuffer[inputIndex++] = c;
      } else {
        inputIndex = 0;
      }
    }
  }
}
