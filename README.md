# Pokemon Go Plus Emulator for ESP32

Autocatcher for Pokemon Go.

**Note:** This repository doesn't contain the secret blobs needed to make a working emulator for a PGP!
You need to dump these secrets yourself from your own original device (see [References](#references) for some pointers).

## Features

This fork :
- Updated to ESP-IDF 5.4.1 which added bluedroid support to c3 (BLE mode only)
- ESP32-C3 Support

Upcoming:
- Wifi AP and Webpage when booting with button pressed to configure on the go without serial usb.

Built on Spezifisch's fork:
https://github.com/spezifisch/pgpemu

- connect up to 4 different devices simultaneously
- parse LED patterns to detect Pokemon/Pokestops/bag full/box full/Pokeballs empty/etc. and press button only when needed
- randomized delay for pressing the button and press duration
- PGP secrets are stored in [ESP32 NVS](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html) instead of being compiled in
- settings menu using serial port
- secrets upload using serial port (see [Upload Secrets](#upload-secrets))
- setting: autocatch/autospin on/off
- setting: periodically turn on WiFi to waste some power so your powerbank doesn't turn off (depends on your powerbank if this works)
- setting: chose between PGP secrets if you have dumps from multiple PGPs
- setting: chose target number of client connections (Bluetooth advertising starts again automatically until all clients are connected)
- store user settings in NVS
- why not use it together with [a fashionable case](https://github.com/spezifisch/pgpemu-case)? :)

## Hardware

Tested with:

- ESP32-C3 Supermini

## Usage

### Build Firmware

You need ESP-IDF. I'm using v5.4.1 (stable) installed using the VSCode extension.
To install it use the [Get Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html).

Open the folder `pgpemu-esp32` in VSCode. Run "ESP-IDF Build, Flash and Monitor".

### Upload Secrets

Go to `./secrets`, rename `secrets.example.yaml` to `secrets.yaml` and edit it with your dumped PGP secrets.

You'll need Python 3.9+ and [Poetry](https://python-poetry.org/). Install dependencies:

```shell
poetry install --no-root
```

Then upload your secrets to your device using:

```shell
# Linux: (replace /dev/ttyUSB0 with your ESP's serial tty)
poetry run ./secrets_upload.py secrets.yaml /dev/ttyUSB0
```

Troubleshooting: If you get `serial.serialutil.SerialException: device reports readiness to read but returned no data (device disconnected or multiple access on port?)`
make doubly sure that your previously opened serial terminal (e.g. ESP-IDF Monitor) is stopped.

### Configuration

Serial menu:

```text
I (208804) uart_events: Device: RedPGP
I (208814) uart_events: User Settings (lost on restart unless saved):
I (208814) uart_events: - s - toggle PGP autospin
I (208814) uart_events: - c - toggle PGP autocatch
I (208824) uart_events: - p - toggle powerbank ping
I (208824) uart_events: - l - toggle verbose logging
I (208834) uart_events: - S - save user settings permanently
I (208834) uart_events: Edit values:
I (208844) uart_events: - m... - set maximum client connections (eg. 3 clients max. with 'm3', up to 4)
I (208854) uart_events: - X... - edit secrets (select eg. slot 2 with 'X2!')
I (208864) uart_events: Commands:
I (208864) uart_events: - h,? - help
I (208864) uart_events: - A - start BT advertising again
I (208874) uart_events: - t - show BT connection times
I (208884) uart_events: - C - show BT client states
I (208884) uart_events: - T - show FreeRTOS task list
I (208894) uart_events: - R - restart
```

### Development

From VSCode's Command Palette run "ESP-IDF: Add vscode configuration folder" [as described here](https://github.com/espressif/vscode-esp-idf-extension/blob/master/docs/C_CPP_CONFIGURATION.md) to get working C/C++ IntelliSense.

## Show Case

Catching Pokemon on connection ID 1 while spinning a Stop on connection ID 0:

```text
I (715392) pgp_led: Pokemon in range!
I (715392) button_task: pressing button delay=1918 ms, duration=300 ms, conn_id=1
I (716602) pgp_led: Pokestop in range!
I (717312) button_task: pressing button delay=1595 ms, duration=500 ms, conn_id=0
I (718382) pgp_led: LED pattern total duration: 6450 ms
I (718392) pgp_led: Caught Pokemon after 3 ball shakes.
I (720112) pgp_led: Got items from Pokestop.
```

Not catching Pokemon:

```text
I (503732) pgp_led: Pokemon in range!
I (503732) button_task: pressing button delay=1198 ms, duration=250 ms, conn_id=1
I (506092) pgp_led: Pokemon fled after 3 ball shakes.
```

Spinning Pokestops:

```text
I (142841) pgp_led: Pokestop in range!
I (142841) button_task: pressing button delay=1895 ms, duration=300 ms, conn_id=0
I (145851) pgp_led: Got items from Pokestop.
```

Bag full detection:

```text
I (151681) pgp_led: Pokestop in range!
I (151681) button_task: pressing button delay=2288 ms, duration=350 ms, conn_id=0
W (155031) pgp_led: Can't spin Pokestop. Bag is full.
```

Connect a second phone:

```text
I (324182) uart_events: starting advertising
I (324192) pgp_bt_gap: advertising start successful
I (356572) pgp_bt_gatts: ESP_GATTS_CONNECT_EVT, conn_id=1, mac=...
...
I (363552) pgp_led: Pokemon in range!
I (363552) button_task: pressing button delay=1217 ms, duration=450 ms, conn_id=1   # <- note that this conn_id=1 is from our second connection
W (365882) pgp_led: Pokeballs are empty or Pokestop went out of range.
I (374712) pgp_led: Pokestop in range!
I (374712) button_task: pressing button delay=1742 ms, duration=500 ms, conn_id=0   # <- all the while autocatch/spin from the first connection are still handled
```

After connecting a third phone:

```text
I (3241231) pgp_bt_gatts: ESP_GATTS_CONNECT_EVT, conn_id=2, mac=...
...
I (3283731) pgp_led: Pokestop in range!
I (3283731) button_task: pressing button delay=1925 ms, duration=350 ms, conn_id=0
I (3284351) pgp_led: Caught Pokemon after 3 ball shakes.
I (3286761) pgp_led: Pokemon in range!
I (3286761) button_task: pressing button delay=2444 ms, duration=250 ms, conn_id=1
I (3286781) pgp_led: Got items from Pokestop.
I (3290101) pgp_led: Pokemon fled after 1 ball shakes.
I (3290231) pgp_bt_gatts: ESP_GATTS_READ_EVT: state=6, CHAR_BATTERY_LEVEL_VAL
I (3291171) pgp_led: Pokemon in range!
I (3291171) button_task: pressing button delay=1168 ms, duration=250 ms, conn_id=2
I (3292271) pgp_led: New Pokemon in range!
I (3292341) button_task: pressing button delay=1069 ms, duration=500 ms, conn_id=1
I (3293501) pgp_led: Caught Pokemon after 3 ball shakes.
I (3294341) pgp_led: Pokemon fled after 3 ball shakes.
I (3299321) pgp_led: Pokemon in range!
I (3299331) button_task: pressing button delay=1457 ms, duration=250 ms, conn_id=1
I (3300471) pgp_led: Pokestop in range!
I (3300781) button_task: pressing button delay=1148 ms, duration=300 ms, conn_id=2
I (3301651) pgp_led: Caught Pokemon after 3 ball shakes.
I (3302951) pgp_led: Got items from Pokestop.
...
I (3560521) uart_events: Task List:
Task Name       Status  Prio    HWM     Task    Affinity
uart_event_task X       12      688     8       -1
IDLE            R       0       1112    6       1
IDLE            R       0       1008    5       0
powerbank_task  B       10      2408    9       -1
ipc0            B       24      1068    1       0
ipc1            B       24      1060    2       1
esp_timer       S       22      3660    3       0
auto_button_tas B       11      336     10      -1
BTC_TASK        B       19      908     12      0
hciT            B       22      1568    13      0
BTU_TASK        B       20      1900    14      0
btController    B       23      2080    11      0
Tmr Svc         B       1       1568    7       0
```

Keeping track of multiple connections:

```text
# press 't':
I (183264) pgp_handshake: active_connections: 2
I (183264) pgp_handshake: - conn_id=0 connected for 137310 ms
I (183264) pgp_handshake: - conn_id=1 connected for 91220 ms

# press 'C' to see state tables for all 4 client slots:
I (185944) pgp_handshake: active_connections: 2
I (185944) pgp_handshake: conn_id_map:
I (185944) pgp_handshake: 0: 0000
I (185954) pgp_handshake: 1: 0001
I (185954) pgp_handshake: 2: ffff
I (185954) pgp_handshake: 3: ffff
I (185964) pgp_handshake: client_states:
I (185964) pgp_handshake: 0: conn_id=0, cert_state=6, recon_key=1, notify=1
I (185974) pgp_handshake: timestamps: hs=4368, rc=0, cs=4537, ce=0
I (185984) pgp_handshake: keys:
I (185984) pgp_handshake: 8d 0f 9a 26 9c ed b3 5d 6d 3c e8 83 7d be e1 12 
...
I (186034) pgp_handshake: 1: conn_id=1, cert_state=6, recon_key=1, notify=1
I (186044) pgp_handshake: timestamps: hs=8972, rc=0, cs=9146, ce=0
I (186054) pgp_handshake: keys:
I (186054) pgp_handshake: 9c 81 78 80 ab cf e3 8c 8e a0 b6 6c 7a 85 f8 5f
...
I (186114) pgp_handshake: 2: conn_id=0, cert_state=0, recon_key=0, notify=0
I (186114) pgp_handshake: timestamps: hs=0, rc=0, cs=0, ce=0
I (186124) pgp_handshake: keys:
I (186124) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
...
I (186184) pgp_handshake: 3: conn_id=0, cert_state=0, recon_key=0, notify=0
I (186184) pgp_handshake: timestamps: hs=0, rc=0, cs=0, ce=0
I (186194) pgp_handshake: keys:
I (186194) pgp_handshake: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
...
```

Changing PGP secrets slots:

```text
# press 'X?':
W (1182721) uart_events: Secrets Mode
I (1182721) uart_events: User Commands:
I (1182721) uart_events: - h,? - help
I (1182721) uart_events: - q - leave secrets mode
I (1182731) uart_events: - 0-9 - select secrets slot
I (1182731) uart_events: - ! - activate selected slot and restart
I (1182741) uart_events: - l - list slots
I (1182741) uart_events: - C - clear slot
I (1182751) uart_events: - there are additional commands but they are for use in secrets_upload.py only!

# press 'l':
I (359801) config_secrets: - pgpsecret_0: device=RedPGP mac=7c:bb:8a:...
I (359811) config_secrets: - pgpsecret_1: device=BluePGP mac=7c:bb:8a:...
I (359811) config_secrets: - pgpsecret_2: device=YellowPGP mac=7c:bb:8a:...
I (359821) config_secrets: - pgpsecret_3: (none)
I (359821) config_secrets: - pgpsecret_4: (none)
I (359831) config_secrets: - pgpsecret_5: (none)
I (359831) config_secrets: - pgpsecret_6: (none)
I (359841) config_secrets: - pgpsecret_7: (none)
I (359841) config_secrets: - pgpsecret_8: (none)
I (359851) config_secrets: - pgpsecret_9: (none)

# press '2!':
W (438551) uart_events: slot=2
W (439841) uart_events: SETTING SLOT=2 AND RESTARTING
I (439851) uart_events: closing nvs
I (439851) uart_events: restarting
...
I (1072) PGPEMU: Device: YellowPGP
I (1072) PGPEMU: MAC: 7c:bb:8a:...
I (1072) PGPEMU: Ready.
I (1092) pgp_bt_gap: advertising start successful
```

Uploading secrets:

```console
$ poetry run ./secrets_upload.py secrets.yaml /dev/ttyUSB0
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

## Credits

- <https://github.com/yohanes/pgpemu> - Original PGPEMU implementation
- <https://github.com/spezifisch/pgpemu> - Spezifisch's fork, on which I added C3 Support and ESP-IDF 5.4.1

## References

- <https://www.reddit.com/r/pokemongodev/comments/5ovj04/pokemon_go_plus_reverse_engineering_write_up/>
- <https://www.reddit.com/r/pokemongodev/comments/8544ol/my_1_gotcha_is_connecting_and_spinning_catching_2/>
- <https://tinyhack.com/2018/11/21/reverse-engineering-pokemon-go-plus/>
- <https://tinyhack.com/2019/05/01/reverse-engineering-pokemon-go-plus-part-2-ota-signature-bypass/>
- <https://coderjesus.com/blog/pgp-suota/> - Most comprehensive write-up
- <https://github.com/Jesus805/Suota-Go-Plus> - Bootloader exploit + secrets dumper
