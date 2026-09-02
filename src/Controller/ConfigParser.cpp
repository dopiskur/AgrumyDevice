#include "Arduino.h"
#include "ArduinoJson.h"

#include "ConfigParser.h"
#include "ServiceController.h"

String ConfigParser::maskApiKeyInJson(const String &json)
{
  const String needle = "\"apiKey\":\"";
  int start = json.indexOf(needle);
  if (start < 0)
  {
    return json;
  }
  start += needle.length();
  int end = json.indexOf('"', start);
  if (end < 0)
  {
    return json;
  }
  return json.substring(0, start) + ServiceController::maskSecret(json.substring(start, end)) + json.substring(end);
}

DeviceConfig ConfigParser::parse(const String &configJson, DeviceConfig currentConfig)
{
  Serial.println("[Device] Load config: " + maskApiKeyInJson(configJson));

  JsonDocument config;
  DeserializationError error = deserializeJson(config, configJson);

  if (error)
  {
    Serial.print("[Device] Load Config; deserializeJson() failed: ");
    Serial.println(error.c_str());
    currentConfig.eventlog.error = true;
    currentConfig.eventlog.errorCode = 20; // 10 is reserved for deserialize fail
    currentConfig.eventlog.errorData = error.c_str();

    return currentConfig;
  }

  String servicePoint = config["servicePoint"];
  // "| """ matters here: a bare assignment from a JSON null (the normal case - no self-signed
  // cert pinned) does not reliably yield an empty ArduinoJson String, so
  // ServiceController::requestPost's `servicePublicKey.length() > 0` check was true for a device
  // with no pinned cert - it then fed that non-empty garbage to setCACert() as if it were a PEM
  // certificate, and mbedTLS rejected the HTTPS handshake with "X509 ... format is invalid".
  String servicePublicKey = config["servicePublicKey"] | "";
  String apiId = config["apiId"];
  String apiKey = config["apiKey"];

  // Roadmap #107: "{}" (or any payload a contract-drifted server field rename produces) is valid,
  // non-empty JSON, so it passes both of apiConfig()'s existing gates (isEmpty/deserializeJson) -
  // a missing key here just silently reads back "". Without this check the device's own identity
  // gets overwritten with "", the next Authenticate 401s, and #97's factory reset fires on what
  // was actually a server-side contract bug. Reject BEFORE any deviceConfig field below is
  // touched, same "keep current, signal the failure" contract as the deserializeJson gate above.
  if (apiId.isEmpty() || apiKey.isEmpty() || servicePoint.isEmpty())
  {
    Serial.println("[Device] Load Config: missing required apiId/apiKey/servicePoint - rejecting (contract drift or malformed payload), keeping current config");
    currentConfig.eventlog.error = true;
    currentConfig.eventlog.errorCode = 21; // 20 is deserializeJson failure, 10 is reserved for registerDevice's own gate
    currentConfig.eventlog.errorData = "missing apiId/apiKey/servicePoint";
    return currentConfig;
  }
  // currentConfig is re-parsed in place on every call (not rebuilt from scratch), so a failure
  // flagged above must not linger into the next call that actually succeeds.
  currentConfig.eventlog.error = false;

  currentConfig.configVersion = config["configVersion"];

  currentConfig.tenantID = config["tenantID"];
  currentConfig.deviceID = config["deviceID"];
  currentConfig.deviceUnitID = config["deviceUnitID"];
  currentConfig.deviceUnitZoneID = config["deviceUnitZoneID"];
  currentConfig.deviceTypeServiceID = config["deviceTypeServiceID"]; // 0 http, 1 https, 2 mqtt

  currentConfig.apiId = apiId;
  currentConfig.apiKey = apiKey;
  currentConfig.servicePoint = servicePoint;
  currentConfig.servicePublicKey = servicePublicKey;

  currentConfig.sleepSeconds = config["sleepSeconds"];
  currentConfig.sleepDeep = config["sleepDeep"];
  // "| 0" keeps the current offset if an older server doesn't send this key, same reasoning as the
  // hysteresis "|" fallbacks below - never silently jump to UTC just because the key was missing.
  currentConfig.utcOffsetSeconds = config["utcOffsetSeconds"] | currentConfig.utcOffsetSeconds;
  currentConfig.deviceSensorEnabled = config["deviceSensorEnabled"];
  currentConfig.deviceControllerEnabled = config["deviceControllerEnabled"];
  currentConfig.batteryEnabled = config["batteryEnabled"];
  currentConfig.enabled = config["enabled"];
  currentConfig.debug = config["debug"];
  currentConfig.reboot = config["reboot"];
  currentConfig.reset = config["reset"];
  currentConfig.firmwareUpdate = config["firmwareUpdate"];
  currentConfig.firmwareVersion = config["firmwareVersion"] | String(""); // roadmap #3 (OTA)
  currentConfig.firmwareUrl = config["firmwareUrl"] | String("");
  currentConfig.firmwareSha256 = config["firmwareSha256"] | String(""); // roadmap #131

  // Roadmap #34: "|" keeps the current value if an older server doesn't send this key, same
  // fallback convention as utcOffsetSeconds/hysteresis above.
  currentConfig.commandVersion = config["commandVersion"] | currentConfig.commandVersion;
  JsonVariant pendingCommandJson = config["pendingCommand"];
  if (pendingCommandJson.isNull())
  {
    currentConfig.pendingCommand.present = false;
  }
  else
  {
    currentConfig.pendingCommand.present = true;
    currentConfig.pendingCommand.idDeviceCommand = pendingCommandJson["idDeviceCommand"];
    currentConfig.pendingCommand.actionType = pendingCommandJson["actionType"];
    currentConfig.pendingCommand.expiresAt = pendingCommandJson["expiresAt"] | String("");
  }

  if (currentConfig.deviceSensorEnabled)
  {
    JsonObject deviceConfigSensor = config["deviceConfigSensor"];

    currentConfig.configSensor.sensorBattery = deviceConfigSensor["sensorBattery"];
    // Roadmap #12: same "fall back to whatever value is already here" rule as the hysteresis
    // fields below - an older server that doesn't send these keys yet must not zero out a
    // previously-configured divider calibration.
    currentConfig.configSensor.batteryDividerR1 = deviceConfigSensor["batteryDividerR1"] | currentConfig.configSensor.batteryDividerR1;
    currentConfig.configSensor.batteryDividerR2 = deviceConfigSensor["batteryDividerR2"] | currentConfig.configSensor.batteryDividerR2;
    currentConfig.configSensor.sensorTemp = deviceConfigSensor["sensorTemp"];
    currentConfig.configSensor.sensorTempSoil = deviceConfigSensor["sensorTempSoil"];
    currentConfig.configSensor.sensorHumid = deviceConfigSensor["sensorHumid"];
    currentConfig.configSensor.sensorMoist = deviceConfigSensor["sensorMoist"];
    currentConfig.configSensor.sensorLight = deviceConfigSensor["sensorLight"];
    currentConfig.configSensor.sensorCo2 = deviceConfigSensor["sensorCo2"];
    currentConfig.configSensor.sensorTvoc = deviceConfigSensor["sensorTvoc"];
    currentConfig.configSensor.sensorBarometer = deviceConfigSensor["sensorBarometer"];
    currentConfig.configSensor.sensorPH = deviceConfigSensor["sensorPH"];
    currentConfig.configSensor.sensorRainLevel = deviceConfigSensor["sensorRainLevel"];
    currentConfig.configSensor.sensorWaterLevel = deviceConfigSensor["sensorWaterLevel"];
    currentConfig.configSensor.sensorWind = deviceConfigSensor["sensorWind"];
  }

  if (currentConfig.deviceControllerEnabled)
  {
    JsonObject deviceConfigController = config["deviceConfigController"];

    currentConfig.configController.tempLow = deviceConfigController["tempLow"];
    currentConfig.configController.tempHigh = deviceConfigController["tempHigh"];
    currentConfig.configController.humidLow = deviceConfigController["humidLow"];
    currentConfig.configController.humidHigh = deviceConfigController["humidHigh"];
    currentConfig.configController.moistLow = deviceConfigController["moistLow"];
    currentConfig.configController.moistHigh = deviceConfigController["moistHigh"];
    currentConfig.configController.lightLow = deviceConfigController["lightLow"];
    currentConfig.configController.lightHigh = deviceConfigController["lightHigh"];
    currentConfig.configController.waterLow = deviceConfigController["waterLow"];
    currentConfig.configController.waterHigh = deviceConfigController["waterHigh"];

    // "|" fallback keeps the current value if the server doesn't send these keys (older API),
    // instead of clobbering it with 0.
    currentConfig.configController.waterLevelHysteresis = deviceConfigController["waterLevelHysteresis"] | currentConfig.configController.waterLevelHysteresis;
    currentConfig.configController.temperatureHysteresis = deviceConfigController["temperatureHysteresis"] | currentConfig.configController.temperatureHysteresis;
    currentConfig.configController.humidityHysteresis = deviceConfigController["humidityHysteresis"] | currentConfig.configController.humidityHysteresis;
    currentConfig.configController.lightHysteresis = deviceConfigController["lightHysteresis"] | currentConfig.configController.lightHysteresis;

    currentConfig.configController.ventilationIntervalEnabled = deviceConfigController["ventilationIntervalEnabled"];
    currentConfig.configController.ventilationInterval = deviceConfigController["ventilationInterval"];
    currentConfig.configController.ventilationIntervalLength = deviceConfigController["ventilationIntervalLength"];
    currentConfig.configController.lightIntervalEnabled = deviceConfigController["lightIntervalEnabled"];
    currentConfig.configController.lightInterval = deviceConfigController["lightInterval"];
    currentConfig.configController.lightIntervalLength = deviceConfigController["lightIntervalLength"];
    currentConfig.configController.heatingIntervalEnabled = deviceConfigController["heatingIntervalEnabled"];
    currentConfig.configController.heatingInterval = deviceConfigController["heatingInterval"];
    currentConfig.configController.heatingIntervalLength = deviceConfigController["heatingIntervalLength"];
    currentConfig.configController.waterPumpIntervalEnabled = deviceConfigController["waterPumpIntervalEnabled"];
    currentConfig.configController.waterPumpInterval = deviceConfigController["waterPumpInterval"];
    currentConfig.configController.waterPumpIntervalLength = deviceConfigController["waterPumpIntervalLength"];

    // Roadmap #39/#115: a JSON array of windows per relay function, capped at
    // MAX_SCHEDULE_SLOTS_PER_FUNCTION - ArduinoJson's static-buffer parsing model has no dynamic
    // growth on-device, so anything beyond the cap is silently dropped rather than overflowing the
    // fixed array (the server already enforces the same cap when saving, DeviceApiController - this
    // only matters for a pre-cap-enforcement server build or a hand-crafted payload).
    auto parseSchedule = [](JsonArray arr, ScheduleWindow slots[], int &count)
    {
        count = 0;
        for (JsonObject slot : arr)
        {
            if (count >= MAX_SCHEDULE_SLOTS_PER_FUNCTION)
            {
                break;
            }
            slots[count].daysOfWeek = slot["daysOfWeek"];
            slots[count].start = slot["start"];
            slots[count].duration = slot["duration"];
            count++;
        }
    };
    parseSchedule(deviceConfigController["ventilationSchedule"], currentConfig.configController.ventilationSchedule, currentConfig.configController.ventilationScheduleCount);
    parseSchedule(deviceConfigController["lightSchedule"], currentConfig.configController.lightSchedule, currentConfig.configController.lightScheduleCount);
    parseSchedule(deviceConfigController["heatingSchedule"], currentConfig.configController.heatingSchedule, currentConfig.configController.heatingScheduleCount);
    parseSchedule(deviceConfigController["waterPumpSchedule"], currentConfig.configController.waterPumpSchedule, currentConfig.configController.waterPumpScheduleCount);

    // Roadmap #36: same "|" current-value fallback as the hysteresis fields above - an older
    // server build that doesn't send these keys yet must not clobber whatever safety limits are
    // already in effect back down to 0 (disabled).
    currentConfig.configController.waterPumpMaxRunSeconds = deviceConfigController["waterPumpMaxRunSeconds"] | currentConfig.configController.waterPumpMaxRunSeconds;
    currentConfig.configController.waterPumpCooldownSeconds = deviceConfigController["waterPumpCooldownSeconds"] | currentConfig.configController.waterPumpCooldownSeconds;

    currentConfig.configController.relayEnabled = deviceConfigController["relayEnabled"];
    currentConfig.configController.relay1 = deviceConfigController["relay1"];
    currentConfig.configController.relay2 = deviceConfigController["relay2"];
    currentConfig.configController.relay3 = deviceConfigController["relay3"];
    currentConfig.configController.relay4 = deviceConfigController["relay4"];
    currentConfig.configController.relay5 = deviceConfigController["relay5"];
    currentConfig.configController.relay6 = deviceConfigController["relay6"];
    currentConfig.configController.relay7 = deviceConfigController["relay7"];
    currentConfig.configController.relay8 = deviceConfigController["relay8"];
  }

  return currentConfig;
}
