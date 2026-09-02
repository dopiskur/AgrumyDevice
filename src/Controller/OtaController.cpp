#include "Arduino.h"
#include <WiFi.h>
#include <HTTPClient.h>       // roadmap #3 (OTA): firmware download
#include <WiFiClientSecure.h> // roadmap #3 (OTA): https firmware download
#include <Update.h>           // roadmap #3 (OTA): Arduino-ESP32 built-in updater

#include "OtaController.h"

// Same CA bundle ServiceController::requestPost() validates against; re-declared here for the OTA download.
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_data_cert_x509_crt_bundle_bin_start");

// roadmap #3 (OTA): stream `url` into the inactive OTA partition; returns true once fully written and verified (caller reboots), false leaves the running firmware untouched.
bool OtaController::update(String url, bool isHttps, const String &servicePublicKey, const String &servicePoint)
{
  Serial.println("[Firmware] Starting OTA update from: " + url);

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[Firmware] No WiFi, aborting OTA");
    return false;
  }

  HTTPClient http;
  WiFiClientSecure secureClient;

  if (isHttps)
  {
    // Roadmap #94: the download host decides which trust to use. A Local-repository URL is served
    // by our own API - if the operator pinned a (self-signed) servicePublicKey, only that cert
    // validates it, exactly as requestPost() does. Any other host (GitHub release asset, a Custom
    // repository) validates against the embedded CA bundle - a pinned cert can never match those,
    // which is precisely why the Local mode exists for pinned-cert installs.
    bool ownServer = servicePublicKey.length() > 0 &&
                     servicePoint.length() > 0 &&
                     url.indexOf(servicePoint) >= 0;
    if (ownServer)
    {
      secureClient.setCACert(servicePublicKey.c_str());
    }
    else
    {
      secureClient.setCACert(nullptr);
      secureClient.setCACertBundle(rootca_crt_bundle_start);
    }
    http.begin(secureClient, url);
  }
  else
  {
    http.begin(url);
  }
  // Roadmap #94: a GitHub release asset URL answers 302 to objects.githubusercontent.com -
  // without this the GET returns the redirect itself (HTTP 302, no body) and OTA "fails".
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK)
  {
    Serial.printf("[Firmware] Download failed, HTTP code: %d\n", httpCode);
    http.end();
    return false;
  }

  int contentLength = http.getSize();
  if (contentLength <= 0)
  {
    Serial.printf("[Firmware] Invalid content length: %d\n", contentLength);
    http.end();
    return false;
  }

  if (!Update.begin(contentLength))
  {
    Serial.printf("[Firmware] Update.begin failed (need %d bytes free in OTA partition): %s\n",
                  contentLength, Update.errorString());
    http.end();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  size_t written = Update.writeStream(*stream);
  if (written != (size_t)contentLength)
  {
    Serial.printf("[Firmware] Wrote %u/%d bytes\n", (unsigned)written, contentLength);
    Update.abort();
    http.end();
    return false;
  }

  if (!Update.end() || !Update.isFinished())
  {
    Serial.printf("[Firmware] Update did not finish: %s\n", Update.errorString());
    http.end();
    return false;
  }

  http.end();
  Serial.println("[Firmware] Update written and verified");
  return true;
}
