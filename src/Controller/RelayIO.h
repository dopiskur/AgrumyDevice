#ifndef RELAYIO_H
#define RELAYIO_H
#include "Arduino.h"

// Routes relay I/O through either direct GPIO or a PCF8574 I2C expander depending on ConfigPin.RELAY_I2C_ADDRESS. `pin` means a GPIO number when i2cAddress==0, a PCF8574 output bit index (0-7) otherwise; sdaPin/sclPin are only read the first time an I2C address is seen (lazy Wire.begin()).
void relayPinMode(int pin, int i2cAddress, int sdaPin, int sclPin);
void relayWrite(int pin, bool on, int i2cAddress, int sdaPin, int sclPin);
bool relayRead(int pin, int i2cAddress, int sdaPin, int sclPin);

#endif
