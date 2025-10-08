# Pokemon Go Plus Emulator for ESP32C3

Autocatcher/Gotcha/Pokemon Go Plus device emulator for Pokemon Go, with autospin and autocatch capabilities.

> [!NOTE]  
> This repository is based on [yohane's initial work](https://github.com/yohanes/pgpemu), [spezifisch's fork](https://github.com/spezifisch/pgpemu) and [paper183's fork](https://github.com/paper183/pgpemu).

> [!WARNING]  
> This repository doesn't contain the secret blobs needed to make a working emulator for a PGP! You need to dump these secrets yourself from your own original device (see [References](#references) for some pointers).

## Features

### Hardware and Configuration

- ESP-IDF with BLE support
- Tailored for ESP32-C3, draws around 0.05A on average
- Secrets stored in [ESP32 NVS](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html) instead of being compiled in
- Settings menu using serial port
- Secrets upload using serial port (see [Upload Secrets](#upload-secrets))
- Toggle autocatch/autospin

### Pokemon Go Plus

- Connect up to 4 different devices simultaneously, with 1 or many secrets
- LED patterns parsing for many use cases:
    - Pokemon caught
    - Pokemon fled
    - Pokestop spin
    - Bag full
    - Box full
    - Pokeballs empty
- Randomized delay for button press and duration
- Settings saved on reboot

## Installation

### Flash and Monitor your ESP32-C3

You need ESP-IDF v4.5.1, see the [get started with ESP-IDF for ESP32-C3 guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/get-started/index.html)

I'm using the VSCode extension, see the [tools guide](https://docs.espressif.com/projects/vscode-esp-idf-extension/en/latest/installation.html), which requires you to configure the extension in the following way:
1. Open the folder [./pgpemu-esp32](https://github.com/shortcuts/pgpemu/tree/main/pgpemu-esp32) as a new project in VSCode.
2. Shift+⌘+P > ESP-IDF: Select Port to Use > choose your device (e.g. `/dev/tty.usbmodem101`)
3. Shift+⌘+P > ESP-IDF: Set Espressif Device Target > esp32c3
4. Shift+⌘+P > ESP-IDF: Select Flash Method > JTAG
5. Shift+⌘+P > ESP-IDF: Build, Flash and Monitor

### Upload Pokemon Go Plus secrets

> You must have Go 1.25.1+ installed

In the [./secrets](https://github.com/shortcuts/pgpemu/tree/main/secrets) directory, rename the `secrets.example.yaml` to `secrets.yaml` and edit it with your dumped Pokemon Go Plus secrets.

Run:

```sh
go run main.go -port /dev/tty.usbmodem101 -file secrets.yaml -baud 115200 -timeout 2
```

#### Usage

```console
reading input secrets file
- DeviceSecrets(RedPGP)
- DeviceSecrets(BluePGP)
- DeviceSecrets(YellowPGP)
enter secrets mode
> D (13911) uart_events: uart[0] event:
> D (13911) uart_events: [UART DATA]: 1 bytes
> 
> !
listing current secrets:
I (13921) config_secrets: - pgpsecret_0: (none)
I (13921) config_secrets: - pgpsecret_1: (none)
I (13921) config_secrets: - pgpsecret_2: (none)
I (13931) config_secrets: - pgpsecret_3: (none)
I (13931) config_secrets: - pgpsecret_4: (none)
I (13941) config_secrets: - pgpsecret_5: (none)
I (13941) config_secrets: - pgpsecret_6: (none)
I (13951) config_secrets: - pgpsecret_7: (none)
I (13951) config_secrets: - pgpsecret_8: (none)
I (13961) config_secrets: - pgpsecret_9: (none)

uploading secrets
> W (15981) uart_events: slot=0
> W (15991) uart_events: set=N                     
> W (15991) uart_events: N=[OK]
> W (16001) uart_events: set=M
> W (16011) uart_events: M=[OK]
> W (16011) uart_events: set=K
> W (16021) uart_events: K=[OK]
> W (16031) uart_events: set=B
> W (16061) uart_events: B=[OK]
> I (16071) uart_events: slot=tmp crc=7f454c46
> I (16151) config_secrets: [OK] wrote secrets to slot 0
> W (16081) uart_events: write=1
writing to nvs slot 0 OK
> I (16161) uart_events: slot=0 crc=7f454c46
readback OK

> W (16171) uart_events: slot=1
> W (16181) uart_events: set=N
> W (16181) uart_events: N=[OK]
> W (16191) uart_events: set=M
> W (16201) uart_events: M=[OK]
> W (16201) uart_events: set=K
> W (16211) uart_events: K=[OK]
> W (16221) uart_events: set=B
> W (16251) uart_events: B=[OK]
> I (16261) uart_events: slot=tmp crc=c8350200
> I (16281) config_secrets: [OK] wrote secrets to slot 1
> W (16271) uart_events: write=1
writing to nvs slot 1 OK
> I (16291) uart_events: slot=1 crc=c8350200
readback OK

> W (16301) uart_events: slot=2
> W (16311) uart_events: set=N
> W (16311) uart_events: N=[OK]
> W (16321) uart_events: set=M
> W (16331) uart_events: M=[OK]
> W (16331) uart_events: set=K
> W (16341) uart_events: K=[OK]
> W (16351) uart_events: set=B
> W (16381) uart_events: B=[OK]
> I (16391) uart_events: slot=tmp crc=72616d65
> I (16411) config_secrets: [OK] wrote secrets to slot 2
> W (16401) uart_events: write=1
writing to nvs slot 2 OK
> I (16421) uart_events: slot=2 crc=72616d65
readback OK

listing new secrets:
I (16431) config_secrets: - pgpsecret_0: device=RedPGP mac=7c:bb:8a:...
I (16431) config_secrets: - pgpsecret_1: device=BluePGP mac=7c:bb:8a:...
I (16441) config_secrets: - pgpsecret_2: device=YellowPGP mac=7c:bb:8a:...
I (16441) config_secrets: - pgpsecret_3: (none)
I (16451) config_secrets: - pgpsecret_4: (none)
I (16451) config_secrets: - pgpsecret_5: (none)
I (16461) config_secrets: - pgpsecret_6: (none)
I (16461) config_secrets: - pgpsecret_7: (none)
I (16471) config_secrets: - pgpsecret_8: (none)
I (16481) config_secrets: - pgpsecret_9: (none)
leaving secrets mode
> 
> X
OK!
```

#### Troubleshooting

If you get `busy device` or `serial.serialutil.SerialException: device reports readiness to read but returned no data (device disconnected or multiple access on port?)`
make doubly sure that your previously opened serial terminal (e.g. ESP-IDF Monitor) is stopped.

## Usage

### Help menu

Press `?`:

```
Secrets: XXXX
User Settings:
- s - toggle autospin
- c - toggle autocatch
- l - cycle through log levels
- S - save user settings permanently
Commands:
- ? - help
- x - select secrets slot (e.g. slot 2 with 'X2')
- r - show runtime counter
- t - show FreeRTOS task list
- f - show all configuration values
- R - restart
Hardware:
- B - toggle input button
Secrets:
- x? - help
- xq - leave secrets mode
- x... - select secrets slot
- ! - activate selected slot and restart
- l - list slots
- C - clear slot
Bluetooth:
- bA - start advertising
- ba - stop advertising
- bs - show client states
- br - reset connections
- b... - set maximum client connections (e.g. 3 clients max. with 'b3', up to 4)
```

### Runtime stats

Press `r`:

```
I (1670781) stats: ---STATS 0---
Caught: 11
Fled: 12
Spin: 14
```

### Bluetooth client states

Press `bs`:

```
I (1716661) pgp_handshake: active_connections: 1
I (1716661) pgp_handshake: conn_id_map:
I (1716661) pgp_handshake: 0: ffff
I (1716661) pgp_handshake: 1: ffff
I (1716661) pgp_handshake: 2: ffff
I (1716661) pgp_handshake: 3: ffff
I (1716661) pgp_handshake: client_states:
I (1716661) pgp_handshake: [0] 0 cert_state=6, recon_key=1, notify=1
I (1716661) pgp_handshake: timestamps: hs=6358, rc=0, cs=6502, ce=0
I (1716661) pgp_handshake: keys:
I (1716671) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716671) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716671) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716671) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716671) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716671) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716671) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716671) pgp_handshake: [0] 1 cert_state=0, recon_key=0, notify=0
I (1716671) pgp_handshake: timestamps: hs=0, rc=0, cs=0, ce=0
I (1716671) pgp_handshake: keys:
I (1716671) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716671) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716671) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716671) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716671) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716671) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716671) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716671) pgp_handshake: [0] 2 cert_state=0, recon_key=0, notify=0
I (1716671) pgp_handshake: timestamps: hs=0, rc=0, cs=0, ce=0
I (1716671) pgp_handshake: keys:
I (1716671) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716671) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716681) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716681) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716681) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716681) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716681) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716681) pgp_handshake: [0] 3 cert_state=0, recon_key=0, notify=0
I (1716681) pgp_handshake: timestamps: hs=0, rc=0, cs=0, ce=0
I (1716681) pgp_handshake: keys:
I (1716681) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716681) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716681) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716681) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716681) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716691) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
I (1716691) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

### Change secret slot

Press `x2` for the secret in the second slot:

```text
W (438551) uart_events: slot=2
W (439841) uart_events: SETTING SLOT=2 AND RESTARTING
I (439851) uart_events: closing nvs
I (439851) uart_events: restarting
```

## References

- <https://github.com/yohanes/pgpemu> - Original PGPEMU implementation
- <https://github.com/spezifisch/pgpemu> - Spezifisch's fork
- <https://github.com/paper183/pgpemu> - Paper183's fork
- <https://www.reddit.com/r/pokemongodev/comments/5ovj04/pokemon_go_plus_reverse_engineering_write_up/>
- <https://www.reddit.com/r/pokemongodev/comments/8544ol/my_1_gotcha_is_connecting_and_spinning_catching_2/>
- <https://tinyhack.com/2018/11/21/reverse-engineering-pokemon-go-plus/>
- <https://tinyhack.com/2019/05/01/reverse-engineering-pokemon-go-plus-part-2-ota-signature-bypass/>
- <https://coderjesus.com/blog/pgp-suota/> - Most comprehensive write-up
- <https://github.com/Jesus805/Suota-Go-Plus> - Bootloader exploit + secrets dumper
