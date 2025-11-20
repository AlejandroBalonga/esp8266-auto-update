#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "config.h"
#include "ota.h"

OTAUpdater updater;
unsigned long lastCheck = 0;
const unsigned long CHECK_INTERVAL = 1000UL * 60 * 10; // 10 minutos

void setup()
{
    Serial.begin(115200);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.println();
    Serial.print("Conectando a WiFi...");
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 20000)
    {
        delay(500);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\nWiFi conectado.");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println("\nNo se pudo conectar a WiFi.");
    }
    updater.begin();
    lastCheck = millis() - CHECK_INTERVAL; // comprobar inmediatamente
}

void loop()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        if (millis() - lastCheck >= CHECK_INTERVAL)
        {
            lastCheck = millis();
            updater.checkForUpdate();
        }
    }
    delay(100);
}