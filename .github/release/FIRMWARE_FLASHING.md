# PGPemu firmware bundle

Files here are built from the public repository and contain **no secrets**
and **no NVS image**. The device does not work until you flash your own
secrets image built from your own `secrets.csv` (see "Load Pokemon Go Plus
Secrets" in the project README).

Target: ESP32-C3, 4MB flash.

| File | Offset | First install | Update |
|------|--------|---------------|--------|
| `bootloader.bin` | `0x0` | yes | yes |
| `partition-table.bin` | `0x8000` | yes | yes |
| your `secrets.bin` | `0x29000` | yes | never |
| `pgpemu.bin` | `0x40000` | yes | yes |

## Migration notes (older firmware)

This version changes the flash layout. Older firmware cannot update in place.

- Do the **First install** once over USB. The full erase removes the old pairings.
- Pair Pokemon GO once more afterwards.
- After that, use **Update**: it keeps pairings.

## First install

### 1. Build your secrets image

With ESP-IDF v5.4.1 exported:

```bash
python $IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py generate secrets.csv secrets.bin 0x4000
```

### 2. Flash

```bash
esptool.py --chip esp32c3 erase_flash
esptool.py --chip esp32c3 write_flash \
  0x0 bootloader.bin \
  0x8000 partition-table.bin \
  0x29000 secrets.bin \
  0x40000 pgpemu.bin
```

Without `secrets.bin` the device logs `NO PGP SECRETS AVAILABLE!` and does not start Bluetooth.

## Update (keeps pairings)

Do not erase the flash and do not write `secrets.bin`. The bonds and session keys stay.

```bash
esptool.py --chip esp32c3 write_flash \
  0x0 bootloader.bin \
  0x8000 partition-table.bin \
  0x40000 pgpemu.bin
esptool.py --chip esp32c3 erase_region 0x2d000 0x2000
```

The last command blanks the OTA state so the bootloader starts the app in the first slot.
