#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include "ota.h"
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecureBearSSL.h>
#include "config.h"

void setupOTA()
{
    ArduinoOTA.onStart([]()
                       {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else { // U_SPIFFS
            type = "filesystem";
        }
        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        Serial.println("Start updating " + type); });
    ArduinoOTA.onEnd([]()
                     { Serial.println("\nEnd"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                          { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });
    ArduinoOTA.onError([](ota_error_t error)
                       {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
            Serial.println("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            Serial.println("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            Serial.println("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            Serial.println("Receive Failed");
        } else if (error == OTA_END_ERROR) {
            Serial.println("End Failed");
        } });

    ArduinoOTA.begin();
}

void handleOTA()
{
    ArduinoOTA.handle();
}

OTAUpdater::OTAUpdater() {}

void OTAUpdater::begin()
{
    // nada por ahora
}

void OTAUpdater::checkForUpdate()
{
    WiFiClient client;
    HTTPClient http;
    String version;
    Serial.println("Comprobando versión remota...");
    if (http.begin(client, UPDATE_VERSION_URL))
    {
        int code = http.GET();
        if (code == HTTP_CODE_OK)
        {
            version = http.getString();
            version.trim();
            Serial.printf("Versión remota: %s\n", version.c_str());
            if (version != CURRENT_VERSION)
            {
                Serial.println("Nueva versión disponible, descargando firmware...");
                // HTTP update (no TLS aquí; si usas https y certificación estricta,
                // configura WiFiClientSecure y fingerprint o CA)
                t_httpUpdate_return ret = ESPhttpUpdate.update(http, UPDATE_BIN_URL);
                if (ret == HTTP_UPDATE_OK)
                {
                    Serial.println("Actualización OK, reiniciando...");
                }
                else if (ret == HTTP_UPDATE_FAILED)
                {
                    Serial.printf("Update failed: %s\n", ESPhttpUpdate.getLastErrorString().c_str());
                }
                else if (ret == HTTP_UPDATE_NO_UPDATES)
                {
                    Serial.println("No updates available (unexpected).");
                }
            }
            else
            {
                Serial.println("Firmware actualizado.");
            }
        }
        else
        {
            Serial.printf("Error al obtener version.txt: %d\n", code);
        }
        http.end();
    }
    else
    {
        Serial.println("No se pudo conectar a UPDATE_VERSION_URL");
    }
}