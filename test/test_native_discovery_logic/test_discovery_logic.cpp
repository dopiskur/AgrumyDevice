#include <unity.h>
#include "../../src/Logic/DiscoveryLogic.h"

void setUp(void) {}
void tearDown(void) {}

void test_AgrumyPrefix_ExtractsMacSuffix(void)
{
    TEST_ASSERT_EQUAL_STRING("AABBCCDDEEFF", extractAgrumyApMac("Agrumy_AABBCCDDEEFF").c_str());
}

void test_NoPrefix_ReturnsEmpty(void)
{
    TEST_ASSERT_EQUAL_STRING("", extractAgrumyApMac("HomeWiFi").c_str());
}

void test_PrefixNotAtStart_ReturnsEmpty(void)
{
    TEST_ASSERT_EQUAL_STRING("", extractAgrumyApMac("NotAgrumy_AABBCCDDEEFF").c_str());
}

void test_PrefixOnly_ReturnsEmptySuffix(void)
{
    TEST_ASSERT_EQUAL_STRING("", extractAgrumyApMac("Agrumy_").c_str());
}

void test_EmptySsid_ReturnsEmpty(void)
{
    TEST_ASSERT_EQUAL_STRING("", extractAgrumyApMac("").c_str());
}

void test_CaseSensitive_LowercasePrefixDoesNotMatch(void)
{
    // WiFiManager always emits the exact "Agrumy_" case - a lowercase neighbor network is not one of ours.
    TEST_ASSERT_EQUAL_STRING("", extractAgrumyApMac("agrumy_AABBCCDDEEFF").c_str());
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_AgrumyPrefix_ExtractsMacSuffix);
    RUN_TEST(test_NoPrefix_ReturnsEmpty);
    RUN_TEST(test_PrefixNotAtStart_ReturnsEmpty);
    RUN_TEST(test_PrefixOnly_ReturnsEmptySuffix);
    RUN_TEST(test_EmptySsid_ReturnsEmpty);
    RUN_TEST(test_CaseSensitive_LowercasePrefixDoesNotMatch);
    return UNITY_END();
}
