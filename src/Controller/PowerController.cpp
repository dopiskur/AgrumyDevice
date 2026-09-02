#include "Arduino.h"
#include "LittleFS.h"

#include "PowerController.h"

void PowerController::railPrimary(int pin, bool state)
{
  pinMode(pin, OUTPUT);

  if (state)
  {
    digitalWrite(pin, HIGH);
    Serial.println("[Power rail on]");
  }
  else
  {
    digitalWrite(pin, LOW);
    Serial.println("[Power rail off]");
  }
  delay(500);
}

void PowerController::railSecondary(int pin, bool state)
{
  pinMode(pin, OUTPUT);

  delay(1000);

  if (state)
  {
    digitalWrite(pin, HIGH);
    Serial.println("[Power analog sensor on]");
  }
  else
  {
    digitalWrite(pin, LOW);
    Serial.println("[Power analog sensor off]");
  }
  delay(500);
}

void PowerController::sleep(int sleepSeconds, bool sleepDeep)
{
  // uint64 math: seconds * 1e6 overflows int32 for anything past ~35 minutes, and
  // esp_sleep_enable_timer_wakeup takes uint64 microseconds anyway.
  const uint64_t uS_TO_S_FACTOR = 1000000ULL;
  int TIME_TO_SLEEP = sleepSeconds;
  esp_sleep_enable_timer_wakeup((uint64_t)TIME_TO_SLEEP * uS_TO_S_FACTOR);

  if (sleepDeep)
  {
    Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");
    Serial.println("Going to sleep now");
    Serial.flush();
    esp_deep_sleep_start(); // never returns
  }
  else
  {
    Serial.println("Deep sleep disabled");
  }
}

void PowerController::reboot()
{
  Serial.println("[Rebooting...]");
  ESP.restart();
}

void PowerController::reset()
{
  LittleFS.format();
  ESP.restart();
}
