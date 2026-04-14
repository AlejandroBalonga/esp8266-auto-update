#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "config.h"
#include "ota.h"

// D4 es el LED integrado en la mayoría de las placas D1 Mini (activo bajo)
#define LED_PIN D4

OTAUpdater updater;
unsigned long lastCheck = 0;
unsigned long lastReconnectAttempt = 0;

void blinkLed(int times, int onMs, int offMs)
{
    for (int i = 0; i < times; i++)
    {
        digitalWrite(LED_PIN, LOW);
        delay(onMs);
        digitalWrite(LED_PIN, HIGH);
        delay(offMs);
    }
}

void setup()
{
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH); // LED apagado por defecto

    Serial.begin(115200);
    delay(100);
    Serial.println();
    Serial.println("=====================================");
    Serial.println("ESP8266 OTA GitHub Updater");
    Serial.printf("Versión: %s\n", FIRMWARE_VERSION);
    Serial.println("=====================================");

    blinkLed(3, 100, 100);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    Serial.print("Conectando a WiFi: ");
    Serial.println(WIFI_SSID);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 20000)
    {
        delay(500);
        Serial.print(".");
        digitalWrite(LED_PIN, LOW);
        delay(100);
        digitalWrite(LED_PIN, HIGH);
        delay(100);
    }

    Serial.println();
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("WiFi conectado.");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        blinkLed(5, 50, 50);
    }
    else
    {
        Serial.println("No se pudo conectar a WiFi.");
        blinkLed(5, 200, 200);
    }

    updater.begin();

    // FIX: esperar 30s antes del primer check para que el sistema esté estable
    lastCheck = millis() - OTA_CHECK_INTERVAL_MS + 30000UL;
}

void loop()
{
    static unsigned long lastToggle = 0;
    if (millis() - lastToggle > 2000)
    {
        lastToggle = millis();
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        if (millis() - lastCheck >= OTA_CHECK_INTERVAL_MS)
        {
            lastCheck = millis();
            Serial.println("\nIniciando comprobación OTA...");
            Serial.printf("Free heap antes de OTA: %u bytes\n", ESP.getFreeHeap());
            updater.checkForUpdate();
        }
    }
    else if (millis() - lastReconnectAttempt > 30000)
    {
        lastReconnectAttempt = millis();
        Serial.println("WiFi desconectado, intentando reconectar...");
        WiFi.reconnect();
    }

    delay(100);
}
