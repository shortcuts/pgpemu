#!/usr/bin/env fish

ls /dev/cu.*

"$HOME/.espressif/python_env/idf5.4_py3.8_env/bin/python" \
    "$HOME/esp/v5.4.1/esp-idf/tools/idf_monitor.py" \
    -p /dev/tty.usbmodem101 \
    -b 115200 \
    --toolchain-prefix riscv32-esp-elf- \
    --make "$HOME/.espressif/python_env/idf5.4_py3.8_env/bin/python $HOME/esp/v5.4.1/esp-idf/tools/idf.py" \
    --target esp32c3 \
    "$HOME/Documents/rpi/esp32/pgpemu/pgpemu-esp32/build/pgpemu.elf"
