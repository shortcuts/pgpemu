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
- Settings menu using serial port
- Statically loaded secrets (see [Load Secrets](#load-secrets)) in [NVS](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html)
- Toggle autocatch/autospin
- Android 15+ support (see [Passkey requirements](#passkey-requirements))

### Pokemon Go Plus

- Connect up to 4 different devices simultaneously
- LED patterns parsing for many use cases:
    - Pokemon caught
    - Pokemon fled
    - Pokestop spin
    - Bag full
    - Box full
    - Pokeballs empty
- Randomized delay for button press and duration
- Settings saved on reboot

#### Passkey requirements

In order to comply with Android 15+ support, a contract must be defined between the emulator and the device. The passkey is the only thing I found to make it work reliably.

The passkey is: **000000**

## Installation

### Flash and Monitor your ESP32-C3

You need ESP-IDF v5.4.1, see the [get started with ESP-IDF for ESP32-C3 guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/get-started/index.html)

I'm using the VSCode extension, see the [tools guide](https://docs.espressif.com/projects/vscode-esp-idf-extension/en/latest/installation.html), which requires you to configure the extension in the following way:
1. Open the folder [./pgpemu-esp32](https://github.com/shortcuts/pgpemu/tree/main/pgpemu-esp32) as a new project in VSCode.
2. Shift+⌘+P > ESP-IDF: Select Port to Use > choose your device (e.g. `/dev/tty.usbmodem101`)
3. Shift+⌘+P > ESP-IDF: Set Espressif Device Target > esp32c3
4. Shift+⌘+P > ESP-IDF: Select Flash Method > JTAG
5. Shift+⌘+P > ESP-IDF: Build, Flash and Monitor

### Load Pokemon Go Plus secrets

> [!WARNING]
> Other PGP emulator forks allows many secrets to be uploaded using serial ports, I'm not an ESP expert and it seems easier for me to statically load everything from a source file, **this fork only allows one secret per flashed device.**

#### Usage

First, rename [secrets.example.csv](./pgpemu-esp32/secrets.example.csv) to **secrets.csv**, then update the values in place following the given schema:

```csv
# Secrets options               # this section is for secrets only
key,type,encoding,value         # /!\ do not edit this line
pgpsecret,namespace,,           # /!\ do not edit this line
name,data,string,"PKLMGOPLUS"   # <- the name of your device
mac,data,hex2bin,7c..........   # <- the mac address you've extracted - should be 12 char long
dkey,data,hex2bin,9b.........   # <- the device key you've extracted - should be 32 char long
blob,data,hex2bin,16.........   # <- the blob data you've extracted - should be 512 char long
# Settings options              # this section is for flash-time settings, it can be overridden later from the uart menu
user_settings,namespace,,       # /!\ do not edit this line
catch,data,i8,1                 # enable autocatch - 1 = yes, 0 = no
spin,data,i8,1                  # enable autospin - 1 = yes, 0 = no
spinp,data,u8,0                 # spin probability out of 10, low storage capacity can lead to full bag issues, this allows spinning to be enabled while aiming to reduce chances of full bags - 0 = spin everything, 1 to 9 = N/10
button,data,i8,1                # enable input button on pokemon encounter - 1 = yes, 0 = no
llevel,data,i8,2                # esp monitor log level - 1 = debug, 2 = info, 3 = verbose
maxcon,data,u8,2                # max allowed bluetooth connection to the device, up to 4
```

## Usage

### Help menu

Press `?`:

```
I (313277) uart_events: ---HELP---
Secrets: PKLMGOPLUS
User Settings:
- as - toggle autospin
- a[0,9] - autospin probability
- ac - toggle autocatch
- l - cycle through log levels
- S - save user settings permanently
Commands:
- ? - help
- r - show runtime counter
- t - show FreeRTOS task list
- s - show all configuration values
- R - restart
Hardware:
- B - toggle input button
Secrets:
- xs - show loaded secrets
- xr - reset loaded secrets
Bluetooth:
- bA - start advertising
- ba - stop advertising
- bs - show client states
- br - clear connections
- b[1,4] - set maximum client connections (e.g. 3 clients max. with 'b3', up to 4, currently 2)
```

### Current settings

Press `f`:

```
I (26297) uart_events: ---SETTINGS---
- Autospin: on
- Autospin probability: 4
- Autocatch: on
- Input button: on
- Log level: 2
- Connections: 1 / 2
```

### Runtime stats

Press `r`:

```
I (118157) stats: ---STATS---
Connection 0:
- Caught: 7
- Fled: 4
- Spin: 0
---STATS---
Connection 1:
- Caught: 35
- Fled: 24
- Spin: 60
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
