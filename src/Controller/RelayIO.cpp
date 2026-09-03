#include "RelayIO.h"
#include <Wire.h>

// Roadmap #149: PCF8574 has no output-state readback distinct from an external pull (Wire can only
// read the CURRENT electrical level, which for a relay-driving expander is exactly what was last
// written), so relayRead() below answers from this shadow byte instead of an I2C read - same
// convention as digitalRead() on a real output pin reading back what digitalWrite() last set. One
// byte covers up to 8 relays; every I2C kit today (KC868-A6) uses 6.
static uint8_t i2cRelayShadow = 0xFF; // all bits high = every relay OFF (active-low expander)
static bool i2cBegun = false;

static void i2cWriteShadow(int i2cAddress)
{
    Wire.beginTransmission(i2cAddress);
    Wire.write(i2cRelayShadow);
    Wire.endTransmission();
}

// Lazy, idempotent - ActuatorController::setupController() is never called from main.cpp's setup()
// today (dead stub), so init happens on first real relay touch instead of depending on a setup()
// wiring change this feature doesn't otherwise need.
static void ensureI2CReady(int i2cAddress, int sdaPin, int sclPin)
{
    if (i2cAddress == 0 || i2cBegun)
    {
        return;
    }
    Wire.begin(sdaPin, sclPin);
    i2cWriteShadow(i2cAddress); // start with every relay off, not whatever power-on-reset left them
    i2cBegun = true;
}

void relayPinMode(int pin, int i2cAddress, int sdaPin, int sclPin)
{
    if (i2cAddress != 0)
    {
        ensureI2CReady(i2cAddress, sdaPin, sclPin); // PCF8574 quasi-bidirectional I/O needs no pinMode-equivalent beyond this
        return;
    }
    pinMode(pin, OUTPUT);
}

void relayWrite(int pin, bool on, int i2cAddress, int sdaPin, int sclPin)
{
    if (i2cAddress != 0)
    {
        ensureI2CReady(i2cAddress, sdaPin, sclPin);
        // Roadmap #149: PCF8574 relay expander is active-LOW (confirmed against the KC868-A6
        // community ESPHome config's "inverted: true" on every relay GPIO switch) - writing 0
        // turns the relay ON, 1 turns it OFF, opposite of the direct-GPIO HIGH-is-on convention
        // every other kit in this codebase uses.
        if (on)
        {
            i2cRelayShadow &= ~(1 << pin);
        }
        else
        {
            i2cRelayShadow |= (1 << pin);
        }
        i2cWriteShadow(i2cAddress);
        return;
    }
    digitalWrite(pin, on ? HIGH : LOW);
}

bool relayRead(int pin, int i2cAddress, int sdaPin, int sclPin)
{
    if (i2cAddress != 0)
    {
        ensureI2CReady(i2cAddress, sdaPin, sclPin);
        return (i2cRelayShadow & (1 << pin)) == 0; // 0 bit = relay on (active-low)
    }
    return digitalRead(pin) == HIGH;
}
