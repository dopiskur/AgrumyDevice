# AgrumyDevice

ESP32 firmware for the Agrumy greenhouse monitoring and control system.
This is the device side of the ecosystem; the backend API and admin UI live
in the separate [AgrumyApi](https://github.com/dopiskur/AgrumyApi) repository.

> Agrumy core (API, firmware, enclosures) is free and open source under the
> [Apache 2.0 license](LICENSE.txt). The mobile apps are proprietary.
> If you use Agrumy, I'd genuinely love to hear about it — open an issue or
> drop me a line.

## Supported hardware

Built with PlatformIO. Two environments:

| Environment | Board | Role |
| --- | --- | --- |
| `esp32dev` | ESP32-WROOM-32 dev board | Controller (relays + sensors) |
| `esp32s3usbotg` | ESP32-S3 | Controller |

## Build

```
pio run -e esp32dev
```

The firmware ↔ API contract is defined in `contracts/device-api/` (JSON
Schema) and enforced in CI on both repositories.

## License

Copyright 2016-2026 Domagoj Piškur

Licensed under the Apache License, Version 2.0 (the "License"); you may not
use this project except in compliance with the License. You may obtain a copy
of the License at http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied.
