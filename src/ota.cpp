#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <CertStoreBearSSL.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Updater.h>
#include <memory>
#include "ota.h"
#include "config.h"

static const size_t CHUNK_BUFFER_SIZE = 4096;
static uint8_t chunkBuffer[CHUNK_BUFFER_SIZE];

BearSSL::CertStore certStore;
bool secureMode = false;

static bool initCertificateStore()
{
    if (!LittleFS.begin())
    {
        Serial.println("LittleFS no pudo inicializarse.");
        return false;
    }

    int numCerts = certStore.initCertStore(LittleFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
    Serial.printf("Number of CA certs read: %d\n", numCerts);

    if (numCerts > 0)
    {
        Serial.println("Certificados cargados correctamente.");
        secureMode = true;
        return true;
    }

    Serial.println("No se encontraron certificados o el archivo no es válido.");
    secureMode = false;
    return false;
}

static std::unique_ptr<BearSSL::WiFiClientSecure> createSecureClient()
{
    auto client = std::make_unique<BearSSL::WiFiClientSecure>();
    if (secureMode)
    {
        client->setCertStore(&certStore);
    }
    else
    {
        client->setInsecure();
    }
    return client;
}

static bool followRedirects(HTTPClient &http, BearSSL::WiFiClientSecure &client, const String &url, int &httpCode)
{
    String currentUrl = url;
    for (int redirect = 0; redirect < 4; redirect++)
    {
        if (!http.begin(client, currentUrl))
        {
            return false;
        }
        http.addHeader("User-Agent", OTA_USER_AGENT);
        httpCode = http.GET();
        if (httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_FOUND || httpCode == HTTP_CODE_SEE_OTHER || httpCode == HTTP_CODE_TEMPORARY_REDIRECT || httpCode == HTTP_CODE_PERMANENT_REDIRECT)
        {
            currentUrl = http.header("Location");
            http.end();
            if (currentUrl.length() == 0)
            {
                return false;
            }
            continue;
        }
        return true;
    }
    return false;
}

static bool getLatestReleaseInfo(String &tagName, String &downloadUrl)
{
    String apiUrl = String("https://api.github.com/repos/") + GHOTA_USER + "/" + GHOTA_REPO + "/releases/latest";
    auto client = createSecureClient();
    HTTPClient http;
    int httpCode = 0;

    if (!followRedirects(http, *client, apiUrl, httpCode))
    {
        Serial.println("Error al inicializar la petición al API de GitHub.");
        return false;
    }

    if (httpCode != HTTP_CODE_OK)
    {
        Serial.printf("GitHub API HTTP error: %d\n", httpCode);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    StaticJsonDocument<16384> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        Serial.print("Error parsing JSON: ");
        Serial.println(error.c_str());
        return false;
    }

    tagName = doc["tag_name"].as<String>();
    if (tagName.length() == 0)
    {
        Serial.println("No se encontró tag_name en la respuesta de GitHub.");
        return false;
    }

    if (doc["draft"].as<bool>())
    {
        Serial.println("La última release es un draft. No se actualizará.");
        return false;
    }

    if (doc["prerelease"].as<bool>() && !GHOTA_ACCEPT_PRERELEASE)
    {
        Serial.println("La última release es prerelease y no está aceptada.");
        return false;
    }

    JsonArray assets = doc["assets"].as<JsonArray>();
    for (JsonObject asset : assets)
    {
        String assetName = asset["name"].as<String>();
        if (assetName == GHOTA_BIN_FILE)
        {
            downloadUrl = asset["browser_download_url"].as<String>();
            break;
        }
    }

    if (downloadUrl.length() == 0)
    {
        Serial.println("No se encontró el asset de firmware en la release.");
        return false;
    }

    return true;
}

static bool downloadFirmware(const String &downloadUrl)
{
    auto client = createSecureClient();
    HTTPClient http;
    int httpCode = 0;

    if (!followRedirects(http, *client, downloadUrl, httpCode))
    {
        Serial.println("Error al inicializar la descarga de firmware.");
        return false;
    }

    if (httpCode != HTTP_CODE_OK)
    {
        Serial.printf("Firmware download HTTP error: %d\n", httpCode);
        http.end();
        return false;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0)
    {
        Serial.println("Tamaño de firmware inválido.");
        http.end();
        return false;
    }

    Serial.printf("Tamaño del firmware: %d bytes\n", contentLength);
    if (!Update.begin((uint32_t)contentLength, U_FLASH))
    {
        Serial.println("No hay espacio suficiente para la actualización.");
        http.end();
        return false;
    }

    WiFiClient *stream = http.getStreamPtr();
    int written = 0;
    int lastProgress = 0;

    while (http.connected() && written < contentLength)
    {
        size_t available = stream->available();
        if (available)
        {
            size_t toRead = available;
            if (toRead > CHUNK_BUFFER_SIZE)
            {
                toRead = CHUNK_BUFFER_SIZE;
            }
            int bytesRead = stream->readBytes(chunkBuffer, toRead);
            if (bytesRead > 0)
            {
                size_t bytesWritten = Update.write(chunkBuffer, bytesRead);
                if (bytesWritten != (size_t)bytesRead)
                {
                    Serial.println("Error escribiendo el chunk de firmware.");
                    http.end();
                    Update.end();
                    return false;
                }

                written += bytesWritten;
                int progress = (written * 100) / contentLength;
                if (progress - lastProgress >= 10)
                {
                    Serial.printf("Progreso: %d%% (%d/%d)\n", progress, written, contentLength);
                    lastProgress = progress;
                }
                yield();
            }
        }
        else
        {
            delay(1);
        }
    }

    http.end();

    if (Update.end(true))
    {
        Serial.printf("Actualización completa: %d bytes escritos\n", written);
        return true;
    }

    Serial.printf("Update failed: %s\n", Update.getErrorString().c_str());
    return false;
}

OTAUpdater::OTAUpdater() {}

void OTAUpdater::begin()
{
    if (!initCertificateStore())
    {
        Serial.println("Usando modo inseguro para conexiones HTTPS.");
    }
}

void OTAUpdater::checkForUpdate()
{
    Serial.println("Comprobando actualización OTA en GitHub...");

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi no conectado, abortando.");
        return;
    }

    Serial.print("IP local: ");
    Serial.println(WiFi.localIP());

    String latestTag;
    String downloadUrl;
    if (!getLatestReleaseInfo(latestTag, downloadUrl))
    {
        Serial.println("No se pudo obtener información de la última release.");
        return;
    }

    Serial.printf("Versión actual: %s\n", FIRMWARE_VERSION);
    Serial.printf("Última versión: %s\n", latestTag.c_str());

    if (latestTag == FIRMWARE_VERSION)
    {
        Serial.println("No hay actualización disponible.");
        return;
    }

    Serial.println("Nueva versión encontrada, descargando firmware...");
    if (downloadFirmware(downloadUrl))
    {
        Serial.println("Firmware descargado correctamente. Reiniciando...");
        delay(100);
        ESP.restart();
    }
    else
    {
        Serial.println("Fallo al descargar o aplicar la actualización.");
    }
}
