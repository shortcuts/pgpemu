#!/usr/bin/env fish

"$HOME/esp/v5.4.1/esp-idf/install.fish" esp32c3

. "$HOME/esp/v5.4.1/esp-idf/export.fish"

idf_tools.py install esp-clang
