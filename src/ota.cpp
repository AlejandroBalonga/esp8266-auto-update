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
    Serial.println("Iniciando check de versión...");

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi no conectado, abortando check.");
        return;
    }

    Serial.print("IP local: ");
    Serial.println(WiFi.localIP());

    // DNS check (opcional, ayuda a diagnosticar)
    IPAddress ip;
    Serial.println("Resolviendo raw.githubusercontent.com...");
    if (WiFi.hostByName("raw.githubusercontent.com", ip))
    {
        Serial.print("raw.githubusercontent.com -> ");
        Serial.println(ip);
    }
    else
    {
        Serial.println("Fallo en resolución DNS para raw.githubusercontent.com");
        // No continue: puede ser problema de DNS/Network
    }
    
    BearSSL::WiFiClientSecure client;
    client.setInsecure(); // solo para pruebas. En producción validar CA/fingerprint.
    HTTPClient http;

    Serial.print("Conectando a: ");
    Serial.println(UPDATE_VERSION_URL);

    if (!http.begin(client, UPDATE_VERSION_URL))
    {
        Serial.println("http.begin() falló (no pudo iniciar conexión HTTPS)");
        return;
    }

    int code = http.GET();
    Serial.printf("http.GET() -> %d\n", code);

    if (code == HTTP_CODE_OK)
    {
        String version = http.getString();
        version.trim();
        Serial.printf("Versión remota: %s\n", version.c_str());
        if (version != CURRENT_VERSION)
        {
            Serial.println("Nueva versión disponible, procediendo a OTA...");
            t_httpUpdate_return ret = ESPhttpUpdate.update(client, UPDATE_BIN_URL);
            if (ret == HTTP_UPDATE_OK)
            {
                Serial.println("Actualización OK.");
            }
            else if (ret == HTTP_UPDATE_FAILED)
            {
                Serial.printf("Update failed: %s\n", ESPhttpUpdate.getLastErrorString().c_str());
            }
            else if (ret == HTTP_UPDATE_NO_UPDATES)
            {
                Serial.println("No updates available.");
            }
        }
        else
        {
            Serial.println("Firmware ya actualizado.");
        }
    }
    else
    {
        Serial.printf("Error al obtener version.txt: %d\n", code);
        Serial.println("Posibles causas: DNS, TLS/fallo certificado, o no hay conectividad hacia raw.githubusercontent.com");
    }

    http.end();
}