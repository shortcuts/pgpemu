# PGPemu firmware bundle

Files here are built from the public repository and contain **no secrets**
and **no NVS image**. The device does not work until you flash your own NVS
image built from your own `secrets.csv` (see "Load Pokemon Go Plus Secrets"
in the project README).

| File | Offset |
|------|--------|
| `bootloader.bin` | `0x0` |
| `partition-table.bin` | `0x8000` |
| your `nvs.bin` | `0x9000` |
| `pgpemu.bin` | `0x30000` |

Target: ESP32-C3, 4MB flash.

## 1. Build your NVS image

With ESP-IDF v5.4.1 exported:

```bash
python $IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py generate secrets.csv nvs.bin 0x20000
```

## 2. Flash

```bash
esptool.py --chip esp32c3 write_flash \
  0x0 bootloader.bin \
  0x8000 partition-table.bin \
  0x9000 nvs.bin \
  0x30000 pgpemu.bin
```
