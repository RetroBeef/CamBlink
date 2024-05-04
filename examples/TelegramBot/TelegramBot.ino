#include <WiFiClientSecure.h>
#include <AsyncTelegram2.h>
#include "creds.h"
#include "CamBlink.h"

#define MYTZ "CET-1CEST,M3.5.0,M10.5.0/3"
WiFiClientSecure client;
AsyncTelegram2 myBot(client);
int64_t userid = CHAT_ID;

void setup() {
  Serial.begin(115200);

  WiFi.setAutoConnect(true);
  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid, password);
  delay(500);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.println("\nWiFi connected");
  Serial.println(WiFi.localIP());
  
  configTzTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
  client.setCACert(telegram_cert);
  
  myBot.setUpdateTime(1000);
  myBot.setTelegramToken(BOTtoken);

  // Check if all things are ok
  Serial.print("\nTest Telegram connection... ");
  myBot.begin() ? Serial.println("OK") : Serial.println("NOK");
}



void loop() {
  TBMessage msg;
  if (myBot.getNewMessage(msg)) {
    MessageType msgType = msg.messageType;
    if (msgType == MessageText){
      String msgText = msg.text;
      Serial.print("Text message received: ");
      Serial.println(msgText);

      if (msgText.equalsIgnoreCase("/cam")) {
        Serial.println("\nSending picture");
        CamBlink cam([&](HTTPClient* http) {
          Serial.println("in callback");
          WiFiClient* client = http->getStreamPtr();
          client->setTimeout(5000);
          size_t size = http->getSize();
          Serial.println(size);
          struct tm timeinfo;
          getLocalTime(&timeinfo);
          char s[100];
          strftime(s,sizeof(s),"%a, %d/%m/%Y %H:%M:%S", &timeinfo);
          myBot.sendPhoto(msg, http->getStream(), size, "img.jpg", s);
          Serial.println("done");
        });
        cam.getNewThumbnail();
      }
    }
  }
}
