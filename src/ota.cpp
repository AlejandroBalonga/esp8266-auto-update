#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>
#include <time.h>
#include "ota.h"
#include "config.h"

// ---------------------------------------------------------------------------
// Sincronización NTP
// ---------------------------------------------------------------------------
static bool isTimeValid()
{
    return time(nullptr) > 8 * 3600 * 2;
}

static void syncTime()
{
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.print("Sincronizando hora NTP...");
    unsigned long start = millis();
    while (!isTimeValid() && millis() - start < 15000)
    {
        delay(500);
        yield();
        Serial.print(".");
    }
    Serial.println();
    if (isTimeValid())
    {
        time_t now = time(nullptr);
        struct tm timeinfo;
        gmtime_r(&now, &timeinfo);
        Serial.printf("Hora actual: %s", asctime(&timeinfo));
    }
    else
    {
        Serial.println("No se pudo sincronizar la hora (no es critico).");
    }
}

// ---------------------------------------------------------------------------
// Cliente HTTPS sin verificacion de CA
// ---------------------------------------------------------------------------
static BearSSL::WiFiClientSecure *createSecureClient()
{
    auto *client = new BearSSL::WiFiClientSecure();
    client->setInsecure();
    client->setBufferSizes(1024, 1024);
    return client;
}

// ---------------------------------------------------------------------------
// Consulta la GitHub API y devuelve tag y URL de descarga del firmware
// ---------------------------------------------------------------------------
static bool getLatestReleaseInfo(String &tagName, String &downloadUrl)
{
    Serial.println("Consultando GitHub API...");
    Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());

    String apiUrl = String("https://api.github.com/repos/") + GHOTA_USER + "/" + GHOTA_REPO + "/releases/latest";

    BearSSL::WiFiClientSecure *client = createSecureClient();
    HTTPClient http;

    http.begin(*client, apiUrl);
    http.setTimeout(15000);
    http.addHeader("User-Agent", OTA_USER_AGENT);
    http.addHeader("Accept", "application/vnd.github+json");

    int httpCode = http.GET();
    Serial.printf("GitHub API HTTP: %d\n", httpCode);

    if (httpCode != HTTP_CODE_OK)
    {
        Serial.printf("GitHub API error: %d (%s)\n", httpCode,
                      http.errorToString(httpCode).c_str());
        http.end();
        delete client;
        return false;
    }

    // Filtro: solo los campos que necesitamos -> ahorra heap
    StaticJsonDocument<128> filter;
    filter["tag_name"] = true;
    filter["draft"] = true;
    filter["prerelease"] = true;
    filter["assets"][0]["name"] = true;
    filter["assets"][0]["browser_download_url"] = true;

    DynamicJsonDocument doc(3072);
    WiFiClient *stream = http.getStreamPtr();
    DeserializationError error = deserializeJson(doc, *stream,
                                                 DeserializationOption::Filter(filter));
    http.end();
    delete client;
    yield();

    if (error)
    {
        Serial.printf("JSON parse error: %s\n", error.c_str());
        return false;
    }

    tagName = doc["tag_name"].as<String>();
    if (tagName.isEmpty())
    {
        Serial.println("No se encontro tag_name.");
        return false;
    }

    if (doc["draft"].as<bool>())
    {
        Serial.println("Release es draft, ignorando.");
        return false;
    }

    if (doc["prerelease"].as<bool>() && !GHOTA_ACCEPT_PRERELEASE)
    {
        Serial.println("Release es prerelease y no esta aceptada.");
        return false;
    }

    for (JsonObject asset : doc["assets"].as<JsonArray>())
    {
        if (asset["name"].as<String>() == GHOTA_BIN_FILE)
        {
            downloadUrl = asset["browser_download_url"].as<String>();
            break;
        }
    }

    if (downloadUrl.isEmpty())
    {
        Serial.printf("Asset '%s' no encontrado en la release.\n", GHOTA_BIN_FILE);
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Descarga y flashea el firmware usando ESPhttpUpdate.
// Esta libreria maneja internamente: redirects, stream y escritura en flash.
// ---------------------------------------------------------------------------
static bool downloadFirmware(const String &downloadUrl)
{
    Serial.println("Iniciando descarga con ESPhttpUpdate...");
    Serial.printf("URL: %s\n", downloadUrl.c_str());
    Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());

    BearSSL::WiFiClientSecure *client = createSecureClient();
    client->setBufferSizes(4096, 512);

    ESPhttpUpdate.onStart([]()
                          { Serial.println("ESPhttpUpdate: inicio de descarga"); });
    ESPhttpUpdate.onProgress([](int current, int total)
                             {
        static int lastPct = -10;
        int pct = (total > 0) ? (current * 100 / total) : 0;
        if (pct - lastPct >= 10) {
            Serial.printf("Progreso: %d%% (%d/%d bytes)\n", pct, current, total);
            lastPct = pct;
        } });
    ESPhttpUpdate.onEnd([]()
                        { Serial.println("ESPhttpUpdate: descarga completa"); });
    ESPhttpUpdate.onError([](int err)
                          { Serial.printf("ESPhttpUpdate error %d: %s\n", err,
                                          ESPhttpUpdate.getLastErrorString().c_str()); });

    // No reiniciar automaticamente: queremos loguear el resultado primero
    ESPhttpUpdate.rebootOnUpdate(false);

    t_httpUpdate_return ret = ESPhttpUpdate.update(*client, downloadUrl);
    delete client;

    switch (ret)
    {
    case HTTP_UPDATE_OK:
        Serial.println("Firmware actualizado correctamente.");
        return true;

    case HTTP_UPDATE_NO_UPDATES:
        Serial.println("ESPhttpUpdate: sin actualizacion disponible.");
        return false;

    case HTTP_UPDATE_FAILED:
    default:
        Serial.printf("ESPhttpUpdate fallo: %s\n",
                      ESPhttpUpdate.getLastErrorString().c_str());
        return false;
    }
}

// ---------------------------------------------------------------------------
// Clase publica OTAUpdater
// ---------------------------------------------------------------------------
OTAUpdater::OTAUpdater() {}

void OTAUpdater::begin()
{
    Serial.println("OTAUpdater::begin()");
    syncTime();
    Serial.printf("Free heap tras begin: %u bytes\n", ESP.getFreeHeap());
}

void OTAUpdater::checkForUpdate()
{
    Serial.println("Comprobando actualizacion OTA en GitHub...");
    Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi no conectado, abortando.");
        return;
    }

    String latestTag;
    String downloadUrl;

    if (!getLatestReleaseInfo(latestTag, downloadUrl))
    {
        Serial.println("No se pudo obtener informacion de la ultima release.");
        return;
    }

    Serial.printf("Version actual:  %s\n", FIRMWARE_VERSION);
    Serial.printf("Ultima version:  %s\n", latestTag.c_str());

    if (latestTag == FIRMWARE_VERSION)
    {
        Serial.println("Firmware al dia, no hay actualizacion.");
        return;
    }

    Serial.println("Nueva version encontrada, descargando firmware...");
    if (downloadFirmware(downloadUrl))
    {
        Serial.println("Reiniciando...");
        delay(500);
        ESP.restart();
    }
    else
    {
        Serial.println("Fallo al actualizar el firmware.");
    }
}