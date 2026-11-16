# Firmware development

This page covers the firmware toolchain, the secrets file, the optional battery monitor, and common build and flash problems. For module layering and protocols, read [architecture.md](architecture.md).

## Toolchain

The project builds from the command line only. The VSCode ESP-IDF extension is not supported.

1. Install dependencies and clone ESP-IDF v5.4.1 to `~/esp/v5.4.1/esp-idf`:

   ```bash
   make install-deps
   ```

2. Set up the ESP-IDF tools for the `esp32c3` target. Run this once per machine:

   ```bash
   make setup-esp-idf
   ```

`make build` and `make clean` source ESP-IDF's `export.sh` on every run. Source it yourself only to call `idf.py` directly: `. ~/esp/v5.4.1/esp-idf/export.sh`.

## Make targets

| Command | Purpose |
|---------|---------|
| `make build` | Compile the firmware. Does not flash. |
| `make clean` | Remove the build directory. |
| `make flash` | Build, then flash over the built-in USB-JTAG with OpenOCD. |
| `make monitor` | Open the serial monitor. |
| `make run` | Flash, then monitor. |
| `make test` | Run the PC unit tests. See [testing.md](testing.md). |
| `make format` | Run clang-format on the firmware sources. |
| `make menuconfig` | Open the ESP-IDF configuration menu. |

`make monitor` and `make run` default to port `/dev/tty.usbmodem101`. The port changes after `make flash`. Override it:

```bash
make monitor PORT=/dev/tty.usbmodemXXXX
```

## Prebuilt firmware

Each release has `pgpemu-firmware-<tag>.zip` with the app, bootloader, and partition table. It holds no secrets. Build and flash a secrets image from your own `secrets.csv` once. Later updates keep your pairings. `FLASHING.md` inside the zip lists the steps. The wiki has a [browser flasher](https://pgpemu.shrtcts.fr/install.html).

## Secrets

The firmware holds one secret set per flashed device. Other forks load secrets over serial. This one uses a static partition for simplicity and reliability.

1. Dump the secrets from your original Pokémon GO Plus. See [references](#references).
2. Copy `pgpemu-esp32/secrets.example.csv` to `pgpemu-esp32/secrets.csv`.
3. Replace the values. Keep the first three lines as they are.

```csv
key,type,encoding,value
pgpsecret,namespace,,
name,data,string,"PKLMGOPLUS"
mac,data,hex2bin,<12 hex characters>
dkey,data,hex2bin,<32 hex characters>
blob,data,hex2bin,<512 hex characters>
```

Rebuild and flash. The secrets live in their own flash partition.

To skip the rebuild, flash a device with no secrets, or run **Reset secrets** in the app. The device boots into provisioning mode and advertises as "PGPemu Setup". Connect with the companion app and tap **Import secrets.csv**. The device stores the values and restarts.

> Any phone in Bluetooth range can provision a device that has no secrets.

## Battery monitor

The monitor is off by default. When off, the device reports a fixed 96% battery level. When on, it reads the battery voltage through the ADC. The level reaches phones through the standard Battery Service and the app's Status card.

### Wiring

Put a voltage divider between the battery positive terminal and GND. Connect the midpoint to an ADC pin.

```
Battery + -- [100k] -- ADC pin -- [100k] -- GND
```

Two equal 100 kΩ resistors halve the voltage. This keeps a 3.3 to 4.2 V LiPo inside the ADC input range. Never connect the battery directly to a GPIO pin.

On the XIAO ESP32-C3, these pins work:

| Board label | GPIO | ADC channel |
|-------------|------|-------------|
| D0 | 2 | 2 (default) |
| D1 | 3 | 3 |
| D2 | 4 | 4 |

### Configuration

Run `make menuconfig`, open **PGPemu Battery Monitor**, then set:

| Option | Default | Meaning |
|--------|---------|---------|
| Enable battery monitoring | off | Turns the monitor on. |
| ADC channel number | 2 | ADC1 channel of the wired pin. |
| Full battery voltage (mV) | 4200 | Voltage at 100%. |
| Empty battery voltage (mV) | 3300 | Voltage at 0%. |
| Voltage divider ratio | 2 | Ratio of the divider. Use 2 for equal resistors. |

Run `make flash`. The device logs the voltage and percentage once at boot. The ESP32-C3 ADC is accurate to about 5%, so the reading can differ from a multimeter.

## Troubleshooting

### ESP-IDF not found

- Check that ESP-IDF v5.4.1 is at `~/esp/v5.4.1/esp-idf`. `make install-deps` installs it.
- Check that `echo $IDF_PATH` prints that path when you call `idf.py` directly.

### Tool not found

- Run `make setup-esp-idf` once.
- Source the tools: `. $IDF_PATH/tools/tools.sh`.

### `TickType_t` format warnings in `pgp_handshake_multi.c`

These come from the FreeRTOS headers. They do not affect behavior.

### Device not found when flashing

- Try another USB cable. A data-less cable does not expose JTAG.
- Try another USB port. Some ports limit current.
- Close any other OpenOCD or debugger session. Only one process can hold the JTAG adapter.
- On Linux, add your user to the `dialout` group: `sudo usermod -a -G dialout $USER`. Reconnect the device.

### Settings or session keys are not saved

- Look for NVS errors in the log. A full NVS partition blocks writes.
- Do not disconnect during the first handshake.
- As a last resort, run `idf.py erase-flash`, then flash again.

## References

### Pokémon GO Plus reverse engineering

- [yohanes/pgpemu](https://github.com/yohanes/pgpemu): the original emulator.
- [spezifisch/pgpemu](https://github.com/spezifisch/pgpemu) and [paper183/pgpemu](https://github.com/paper183/pgpemu): forks.
- [Reddit write-up](https://www.reddit.com/r/pokemongodev/comments/5ovj04/pokemon_go_plus_reverse_engineering_write_up/)
- [tinyhack, part 1](https://tinyhack.com/2018/11/21/reverse-engineering-pokemon-go-plus/) and [part 2](https://tinyhack.com/2019/05/01/reverse-engineering-pokemon-go-plus-part-2-ota-signature-bypass/)
- [coderjesus: PGP SUOTA](https://coderjesus.com/blog/pgp-suota/): the most complete write-up.
- [Suota-Go-Plus](https://github.com/Jesus805/Suota-Go-Plus): bootloader exploit and secrets dumper.

### Platform documentation

- [ESP-IDF v5.4.1 for ESP32-C3](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32c3/)
- [NVS API](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32/api-reference/storage/nvs_flash.html)
- [Bluetooth Core Specification](https://www.bluetooth.com/specifications/specs/)
