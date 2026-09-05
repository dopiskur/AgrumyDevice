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

void test_UrlEncode_UnreservedCharsUnchanged(void)
{
    TEST_ASSERT_EQUAL_STRING("Abc123-_.~", urlEncodeFormValue("Abc123-_.~").c_str());
}

void test_UrlEncode_SpaceAndSpecialChars_PercentEncoded(void)
{
    TEST_ASSERT_EQUAL_STRING("My%20WiFi%26Pass%2B1", urlEncodeFormValue("My WiFi&Pass+1").c_str());
}

void test_UrlEncode_EmptyString_ReturnsEmpty(void)
{
    TEST_ASSERT_EQUAL_STRING("", urlEncodeFormValue("").c_str());
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
    RUN_TEST(test_UrlEncode_UnreservedCharsUnchanged);
    RUN_TEST(test_UrlEncode_SpaceAndSpecialChars_PercentEncoded);
    RUN_TEST(test_UrlEncode_EmptyString_ReturnsEmpty);
    return UNITY_END();
}
