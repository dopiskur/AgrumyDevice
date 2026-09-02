# AgrumyFirmware

ESP32 firmware for the Agrumy greenhouse monitoring and control system.
This is the device side of the ecosystem; the backend API and admin UI live
in the separate [AgrumyService](https://github.com/dopiskur/AgrumyService) repository.

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

### Version and board (roadmap #94)

The version string the device reports (and compares OTA offers against) is
**derived at build time**, never edited by hand:

1. `FIRMWARE_VERSION` environment variable (what the release workflow sets from the tag),
2. else `git describe --tags --always --dirty` (a dev build, e.g. `1.2.3-4-gabc1234-dirty`),
3. else `0.0.0-dev`.

See `tools/firmware_version.py` (a PlatformIO `extra_scripts` pre-script). Each
environment also carries `-D AGRUMY_BOARD="<env name>"`; the device sends that as
`Board` in its config-poll heartbeat, and it is the `<board>` in the release file
name below - so the API knows which `.bin` fits which hardware.

## Releases (roadmap #94)

A release is produced only by `.github/workflows/release.yml`, triggered by a semver tag:

```
git tag v1.2.3
git push origin v1.2.3
```

It builds every environment, names the images by the convention the API enforces
on every import path -

```
agrumy-<board>-v<version>.bin     e.g. agrumy-esp32dev-v1.2.3.bin
```

- and publishes a GitHub Release with those files plus `manifest.json` (SHA-256 per
file) and `SHA256SUMS.txt`. The AgrumyService admin UI (Firmware page) reads these
releases directly (default **GitHub** source), can pull them into its own **Local**
repository (needed for air-gapped installs or a pinned self-signed API certificate),
or can point at a **Custom** repository serving the same `manifest.json` format.

### Offline USB repository (roadmap #94, part C)

For a greenhouse server with no internet: on an online machine, either use the
**Build offline repo** button on the Firmware page (Chrome/Edge/Opera, HTTPS), or
run one of the scripts in `tools/offline-repo/`:

```
tools/offline-repo/prepare-offline-repo.sh /media/usb/agrumy-firmware
.\tools\offline-repo\prepare-offline-repo.ps1 -Target E:\agrumy-firmware
```

Both download the release `.bin` files and write a `manifest.json` with SHA-256
checksums. Plug the stick into the offline server and use **Import from a directory
on the server** on the Firmware page - the import verifies every checksum before a
file enters the catalog.

## License

Copyright 2016-2026 Domagoj Piškur

Licensed under the Apache License, Version 2.0 (the "License"); you may not
use this project except in compliance with the License. You may obtain a copy
of the License at http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied.
