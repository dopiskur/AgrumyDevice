#include "SleepScheduleLogic.h"

int secondsUntilScheduleBoundary(int daysOfWeekMask, int startSeconds, int durationSeconds, int nowLocalWeekday, int nowLocalSecondsOfDay)
{
    // d=0 is today, d=7 wraps back to today's own weekday next week - needed when today is the
    // ONLY scheduled day and its window has already passed, so the search must not stop at d=6.
    for (int d = 0; d <= 7; d++)
    {
        int weekday = (nowLocalWeekday + d) % 7;
        if ((daysOfWeekMask & (1 << weekday)) == 0)
        {
            continue;
        }

        if (d == 0)
        {
            if (nowLocalSecondsOfDay < startSeconds)
            {
                return startSeconds - nowLocalSecondsOfDay;
            }
            if (nowLocalSecondsOfDay < startSeconds + durationSeconds)
            {
                return (startSeconds + durationSeconds) - nowLocalSecondsOfDay;
            }
            continue; // today's window already ended, keep scanning later days
        }

        int secondsUntilThatMidnight = d * 86400 - nowLocalSecondsOfDay;
        return secondsUntilThatMidnight + startSeconds;
    }
    return -1; // no bit set anywhere in the mask
}

int secondsUntilIntervalBoundary(int interval, int intervalLength, time_t epochSeconds)
{
    if (interval <= 0)
    {
        return -1;
    }

    unsigned long positionInCycle = (unsigned long)epochSeconds % (unsigned long)interval;
    bool isOn = positionInCycle < (unsigned long)intervalLength;
    return isOn
        ? (int)((unsigned long)intervalLength - positionInCycle)
        : (int)((unsigned long)interval - positionInCycle);
}

int clampToSleepFloor(int candidateSeconds, int floorSeconds)
{
    return candidateSeconds < floorSeconds ? floorSeconds : candidateSeconds;
}
