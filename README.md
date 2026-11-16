<p align="center">
  <img src="docs/wiki/icon.png" alt="PGPemu icon" width="96">
</p>

<h1 align="center">PGPemu</h1>

<p align="center">
  Open-source ESP32-C3 firmware and Android companion app that emulate the Pokémon GO Plus accessory.
</p>

<p align="center">
  <a href="pgpemu-esp32/LICENSE"><img src="https://img.shields.io/badge/License-BSD-blue?style=flat-square" alt="License"></a>
  <img src="https://img.shields.io/badge/minSdk-28%20(Android%209)-green?style=flat-square" alt="Android 9 or later">
</p>

The device pairs with Pokémon GO over Bluetooth like a real GO Plus. It spins Pokéstops and catches Pokémon for you while the phone stays in your pocket. The companion app handles pairing, settings, and diagnostics.

> [!WARNING]
> This repository does not contain the secrets a working emulator needs. You must dump them from your own original device. See [references](docs/development.md#references).

## Features

- Connect up to four phones at once, each with its own [Autospin and Autocatch settings](https://pgpemu.shrtcts.fr/profiles.html).
- Reconnect without a passkey after the first pairing.
- Follow the game state on the device LED and in an activity feed in the app.
- [Update the firmware](https://pgpemu.shrtcts.fr/install.html) from a browser or from the app, with automatic rollback.
- Skip the Bluetooth pairing dialog with the app's pairing assistant.
- Read [status and diagnostics](https://pgpemu.shrtcts.fr/diagnostics.html) from the app.
- Optional battery level reporting.

## Get started

1. Build or buy the [hardware](https://pgpemu.shrtcts.fr/hardware.html).
2. [Flash the firmware](https://pgpemu.shrtcts.fr/install.html).
3. Install the companion app from the [latest release](https://github.com/shortcuts/pgpemu/releases/latest).
4. [Connect and pair](https://pgpemu.shrtcts.fr/connect.html) the app and Pokémon GO.

The [wiki](https://pgpemu.shrtcts.fr/) covers every step. If something fails, read [troubleshooting](https://pgpemu.shrtcts.fr/troubleshooting.html). Release notes are in the [changelog](https://pgpemu.shrtcts.fr/changelog.html).

## Contributing

To build the firmware or the app, or to open a pull request, read [CONTRIBUTING.md](CONTRIBUTING.md).

## Credits

This project builds on [yohanes/pgpemu](https://github.com/yohanes/pgpemu), [spezifisch/pgpemu](https://github.com/spezifisch/pgpemu), and [paper183/pgpemu](https://github.com/paper183/pgpemu). The pairing assistant is based on [PoGo+LE](https://github.com/Mygod/pogoplusle).

## License

The firmware license is in [pgpemu-esp32/LICENSE](pgpemu-esp32/LICENSE). Code derived from PoGo+LE is covered by [NOTICE](NOTICE).
