// Roadmap #19/#95/#36: native (host-based) tests for runTimeCeilingHit()/cooldownActive() -
// WaterPump's device-side hard ceiling and cooldown, no ESP32/Arduino dependency, runs via
// `pio test -e native`.
#include <unity.h>
#include "../../src/Logic/RelayLogic.h"

void setUp(void) {}
void tearDown(void) {}

// ---- runTimeCeilingHit -------------------------------------------------------------------

void test_Ceiling_NotTracked_NeverHits(void)
{
    // onSinceEpoch == 0 means "not currently on" - no stretch to measure.
    TEST_ASSERT_FALSE(runTimeCeilingHit(1000000, 0, 60));
}

void test_Ceiling_JustBelowMax_NotHit(void)
{
    TEST_ASSERT_FALSE(runTimeCeilingHit(1000059, 1000000, 60));
}

void test_Ceiling_AtMax_Hit(void)
{
    // elapsed >= maxRunSeconds is the hit condition - inclusive of the boundary itself.
    TEST_ASSERT_TRUE(runTimeCeilingHit(1000060, 1000000, 60));
}

void test_Ceiling_WellPastMax_Hit(void)
{
    TEST_ASSERT_TRUE(runTimeCeilingHit(1003600, 1000000, 60));
}

void test_Ceiling_MaxRunSecondsZero_Disabled(void)
{
    // 0 (or negative) means "no ceiling configured", not "hits immediately".
    TEST_ASSERT_FALSE(runTimeCeilingHit(1003600, 1000000, 0));
}

void test_Ceiling_MaxRunSecondsNegative_Disabled(void)
{
    TEST_ASSERT_FALSE(runTimeCeilingHit(1003600, 1000000, -1));
}

// ---- cooldownActive -----------------------------------------------------------------------

void test_Cooldown_NeverBeenOff_NotActive(void)
{
    // offSinceEpoch == 0 means "never been off since boot" - a fresh reboot must not be blocked
    // by a cooldown it never actually observed (roadmap #36's own "acceptable to lose" decision).
    TEST_ASSERT_FALSE(cooldownActive(1000000, 0, 300));
}

void test_Cooldown_JustAfterOff_Active(void)
{
    TEST_ASSERT_TRUE(cooldownActive(1000001, 1000000, 300));
}

void test_Cooldown_JustBeforeElapsed_Active(void)
{
    TEST_ASSERT_TRUE(cooldownActive(1000299, 1000000, 300));
}

void test_Cooldown_AtElapsed_NotActive(void)
{
    // elapsed >= cooldownSeconds clears it - inclusive of the boundary itself.
    TEST_ASSERT_FALSE(cooldownActive(1000300, 1000000, 300));
}

void test_Cooldown_WellPastElapsed_NotActive(void)
{
    TEST_ASSERT_FALSE(cooldownActive(1010000, 1000000, 300));
}

void test_Cooldown_SecondsZero_Disabled(void)
{
    TEST_ASSERT_FALSE(cooldownActive(1000001, 1000000, 0));
}

void test_Cooldown_SecondsNegative_Disabled(void)
{
    TEST_ASSERT_FALSE(cooldownActive(1000001, 1000000, -1));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_Ceiling_NotTracked_NeverHits);
    RUN_TEST(test_Ceiling_JustBelowMax_NotHit);
    RUN_TEST(test_Ceiling_AtMax_Hit);
    RUN_TEST(test_Ceiling_WellPastMax_Hit);
    RUN_TEST(test_Ceiling_MaxRunSecondsZero_Disabled);
    RUN_TEST(test_Ceiling_MaxRunSecondsNegative_Disabled);
    RUN_TEST(test_Cooldown_NeverBeenOff_NotActive);
    RUN_TEST(test_Cooldown_JustAfterOff_Active);
    RUN_TEST(test_Cooldown_JustBeforeElapsed_Active);
    RUN_TEST(test_Cooldown_AtElapsed_NotActive);
    RUN_TEST(test_Cooldown_WellPastElapsed_NotActive);
    RUN_TEST(test_Cooldown_SecondsZero_Disabled);
    RUN_TEST(test_Cooldown_SecondsNegative_Disabled);
    return UNITY_END();
}
