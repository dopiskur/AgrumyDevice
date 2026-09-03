#ifndef RELAYIO_H
#define RELAYIO_H
#include "Arduino.h"

// Roadmap #149: routes relay I/O through either direct GPIO (every kit so far) or a PCF8574 I2C
// expander (KC868-A6) depending on ConfigPin.RELAY_I2C_ADDRESS - the one place ActuatorController
// needs to know which hardware path a kit uses, so its threshold/interval/schedule/safety-limit
// logic keeps calling one function instead of branching on kit at every call site. `pin` means a
// GPIO number when i2cAddress==0, a PCF8574 output bit index (0-7) otherwise; sdaPin/sclPin are
// only read the first time an I2C address is seen (lazy Wire.begin(), no explicit setup() wiring
// needed). NOT physically verified against real KC868-A6 hardware by this project - see
// DeviceModel.h's ConfigPin comment.
void relayPinMode(int pin, int i2cAddress, int sdaPin, int sclPin);
void relayWrite(int pin, bool on, int i2cAddress, int sdaPin, int sclPin);
bool relayRead(int pin, int i2cAddress, int sdaPin, int sclPin);

#endif
