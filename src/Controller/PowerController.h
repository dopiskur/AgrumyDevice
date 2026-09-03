#ifndef PowerController_H
#define PowerController_H
#include "Arduino.h"

class PowerController
{
public:
    // Mosfet activation.
    static void railPrimary(int pin, bool state);
    static void railSecondary(int pin, bool state);

    // Powers the chip down between cycles; the timer wake is a full reset back through setup(). Caller decides WHEN sleeping is safe - never while the device drives relays, since deep sleep drops GPIO outputs and wipes ActuatorController's interval state. Never returns when sleepDeep is true.
    static void sleep(int sleepSeconds, bool sleepDeep);

    static void reboot();
    static void reset();
};

#endif
