#include <unity.h>
#include <string>
#include "../../src/Logic/LoRaPayloadLogic.h"

void setUp(void) {}
void tearDown(void) {}

void test_SensorUplink_AllFieldsPresent(void)
{
    LoRaSensorReading reading;
    reading.temperature = 23.5;
    reading.humidity = 55.25;
    reading.moisture = 40.0;
    reading.battery = 87.0;

    std::string json = encodeLoRaSensorUplink(reading);
    TEST_ASSERT_EQUAL_STRING(
        "{\"t\":\"sensor\",\"d\":[{\"temperature\":23.50,\"humidity\":55.25,\"moisture\":40.00,\"battery\":87.00}]}",
        json.c_str());
}

void test_SensorUplink_MissingFieldsOmitted(void)
{
    LoRaSensorReading reading; // all NAN by default
    reading.temperature = 21.0;

    std::string json = encodeLoRaSensorUplink(reading);
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"sensor\",\"d\":[{\"temperature\":21.00}]}", json.c_str());
}

void test_SensorUplink_NoReadings_EmptyObject(void)
{
    LoRaSensorReading reading;
    std::string json = encodeLoRaSensorUplink(reading);
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"sensor\",\"d\":[{}]}", json.c_str());
}

void test_ConfigUplink_UsesPascalCaseFieldNames(void)
{
    LoRaConfigHeartbeat heartbeat;
    heartbeat.configVersion = 3;
    heartbeat.uptimeSeconds = 120;
    heartbeat.rssi = -80;
    heartbeat.freeHeapBytes = 45000;
    heartbeat.firmwareVersion = "1.2.3";
    heartbeat.board = "esp32-lora";

    std::string json = encodeLoRaConfigUplink(heartbeat);
    TEST_ASSERT_EQUAL_STRING(
        "{\"t\":\"config\",\"ConfigVersion\":3,\"Uptime\":120,\"Rssi\":-80,\"FreeHeap\":45000,\"FirmwareVersion\":\"1.2.3\",\"Board\":\"esp32-lora\"}",
        json.c_str());
}

void test_EventUplink_UsesPascalCaseFieldNames(void)
{
    std::string json = encodeLoRaEventUplink("Crash", "backtrace overflow");
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"event\",\"EventType\":\"Crash\",\"Message\":\"backtrace overflow\"}", json.c_str());
}

void test_EventUplink_EscapesQuotesInMessage(void)
{
    std::string json = encodeLoRaEventUplink("Crash", "bad \"pointer\"");
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"event\",\"EventType\":\"Crash\",\"Message\":\"bad \\\"pointer\\\"\"}", json.c_str());
}

void test_CommandAckUplink_UsesPascalCaseFieldName(void)
{
    std::string json = encodeLoRaCommandAckUplink(42);
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"ack\",\"CommandId\":42}", json.c_str());
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_SensorUplink_AllFieldsPresent);
    RUN_TEST(test_SensorUplink_MissingFieldsOmitted);
    RUN_TEST(test_SensorUplink_NoReadings_EmptyObject);
    RUN_TEST(test_ConfigUplink_UsesPascalCaseFieldNames);
    RUN_TEST(test_EventUplink_UsesPascalCaseFieldNames);
    RUN_TEST(test_EventUplink_EscapesQuotesInMessage);
    RUN_TEST(test_CommandAckUplink_UsesPascalCaseFieldName);
    return UNITY_END();
}
