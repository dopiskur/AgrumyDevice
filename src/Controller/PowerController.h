#ifndef PowerController_H
#define PowerController_H
#include "Arduino.h"

// Roadmap #127: power-rail/sleep/reboot/reset split out of DeviceController.cpp's God Class -
// takes whatever config values a call needs as parameters instead of reaching into DeviceConfig
// itself, so this class carries no dependency on where/how the caller stores its config.
class PowerController
{
public:
    // Mosfet activation.
    static void railPrimary(int pin, bool state);
    static void railSecondary(int pin, bool state);

    // Roadmap #26: powers the chip down between cycles; the timer wake is a full reset back
    // through setup(). Caller (main loop) decides WHEN sleeping is safe - notably never while
    // the device drives relays, since deep sleep drops GPIO outputs and wipes the millis-based
    // interval state in ActuatorController. Never returns when sleepDeep is true.
    static void sleep(int sleepSeconds, bool sleepDeep);

    static void reboot();
    static void reset();
};

#endif
