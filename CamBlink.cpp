#include "CamBlink.h"
#include "creds.h"
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>

static const char* loginUrl = "https://rest-prod.immedia-semi.com/api/v5/account/login";
static const char* setThumbnailUrl = "https://rest-"BLINK_TIER".immedia-semi.com/api/v1/accounts/"BLINK_ACCOUNTID"/networks/"BLINK_NETWORKID"/owls/"BLINK_CAMID"/thumbnail";//POST
static const char* getCommandStatusUrl = "https://rest-"BLINK_TIER".immedia-semi.com/network/"BLINK_NETWORKID"/command/";//GET, add command id from set thumbnail
static const char* getThumbnailUrl = "https://rest-"BLINK_TIER".immedia-semi.com/api/v3/media/accounts/"BLINK_ACCOUNTID"/networks/"BLINK_NETWORKID"/owl/"BLINK_CAMID"/thumbnail/thumbnail.jpg";//GET
static const char* loginJsonBody = "{\"unique_id\":\"a32b5151-f67b-4dc8-9523-75a34830adcb\",\"password\":\""BLINK_PASSWORD"\",\"email\":\""BLINK_MAIL"\",\"app_version\":\"30.0\",\"client_name\":\"unraid-api\",\"client_type\":\"android\",\"device_identifier\":\"nodered/unraid\",\"os_version\":\"14\",\"reauth\":true}";

CamBlink::CamBlink(const std::function<void(HTTPClient*)>& thumbnailCallback) : thumbnailCallback(thumbnailCallback){
  Preferences preferences;
  preferences.begin("CamBlink", false); 
  String token = preferences.getString("authToken","");
  lastLogin = preferences.getUInt("lastLogin", 0);
  preferences.end();
  if(lastLogin && token.length()){
    memset(authToken, 0, sizeof(authToken));
    memcpy(authToken, token.c_str(), token.length());
  }
}

void CamBlink::login(){
  struct tm timeinfo;
  uint32_t nowTs;
  getLocalTime(&timeinfo);
  nowTs = (uint32_t)mktime(&timeinfo);
  if(lastLogin && authToken[0]){
    if(nowTs - lastLogin < 60 * 60* 12){
      Serial.printf("last login was at %u. probably no login needed this time.\r\n", lastLogin);
      return;
    }
  }
  HTTPClient http;
  http.begin(loginUrl);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(loginJsonBody);
  if (httpCode == 200) {
    DeserializationError error = deserializeJson(doc, http.getString());
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }
    JsonObject auth = doc["auth"].as<JsonObject>();
    const char* auth_token = auth["token"];
    memset(authToken, 0, sizeof(authToken));
    memcpy(authToken, auth_token, strlen(auth_token));
    lastLogin = nowTs;
    Serial.printf("got auth token %s\r\n", authToken);
    Preferences preferences;
    preferences.begin("CamBlink", false); 
    preferences.putString("authToken",String(authToken));
    preferences.putUInt("lastLogin", lastLogin);
    preferences.end();
  }else{
    Serial.printf("blink login failed! (%i)\r\n", httpCode);
    Serial.println(http.getString());
  }
}

uint32_t CamBlink::setThumbnail() {
  uint32_t id = 0;
  HTTPClient http;
  http.begin(setThumbnailUrl);
  http.addHeader("token-auth", authToken);
  int httpCode = http.POST("");
  if (httpCode == 200) {
    DeserializationError error = deserializeJson(doc, http.getStream());
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return 0;
    }
    id = doc["id"];
  }else{
    Serial.println("error in setThumbnail");
  }
  return id;
}

uint8_t CamBlink::pollCommandStatus(const uint32_t& commandId) {
  HTTPClient http;
  bool ready = 0;
  String url = getCommandStatusUrl + String(commandId);
  http.begin(url);
  http.addHeader("token-auth", authToken);
  int httpCode = http.GET();
  if (httpCode == 200) {
    DeserializationError error = deserializeJson(doc, http.getStream());
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return 0;
    }
    ready = doc["complete"]; // true
  }else{
    Serial.println("error in pollCommandStatus");
  }
  return !!ready;
}

void CamBlink::streamThumbnail() {
  HTTPClient http;
  http.begin(getThumbnailUrl);
  http.addHeader("token-auth", authToken);
  int httpCode = http.GET();
  if (httpCode == 200) {
    thumbnailCallback(&http);
  }else{
    Serial.println("error in streamThumbnail");
  }
}

void CamBlink::getNewThumbnail(){
  login();
  uint32_t commandId = setThumbnail();
  if (commandId) {
    uint8_t ready = pollCommandStatus(commandId);
    uint8_t timeout = 0;
    while (!ready && timeout < 10) {
      ready = pollCommandStatus(commandId);
      delay(1000);
      timeout++;
    }
    if (ready) {
      streamThumbnail();
    }else{
      Serial.println("Timeout");
    }
  }
}
