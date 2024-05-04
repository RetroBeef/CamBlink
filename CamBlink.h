#pragma once
#include <Arduino.h>
#include <cstdint>
#include <functional>
#include <ArduinoJson.h>
#include <HTTPClient.h>

class CamBlink{
private:
protected:
    std::function<void(HTTPClient*)> thumbnailCallback;
    StaticJsonDocument<2048> doc;
    char authToken[32] = {0};
    uint32_t lastLogin = 0;

    uint32_t setThumbnail();
    uint8_t pollCommandStatus(const uint32_t& commandId);
    void streamThumbnail();
    void login();
public:
    CamBlink(const std::function<void(HTTPClient*)>& thumbnailCallback);
    virtual ~CamBlink(){}
    void getNewThumbnail();
};
