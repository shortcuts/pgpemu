# Firmware testing

PC tests run the real firmware `.c` files against stubs. They need no hardware. The companion app has its own guide: [companion-testing.md](companion-testing.md).

## Run the tests

```bash
make test                      # PC tests plus the smoke test self-test
./run_tests.sh                 # PC tests only
./run_tests.sh test_settings   # one module
```

Each module has a `test_<module>.c` in `pgpemu-esp32/main/pc/`. Stubs and fixtures live in `pc/stubs/` and `pc/fixtures/`. Control Service wire vectors live in `protocol/control_vectors.txt` and also feed the companion app test `ControlWireVectorsTest.kt`.

## Rules

- A test includes the real firmware `.c`. Never re-declare firmware code in a test.
- Packet captures are the source of truth. If a test disagrees with a capture, the test is wrong.
- Test protocol encoding and decoding, boundary values, and error paths.
- A bug fix starts with a regression test in `test_regression.c` that fails first.
- A Control Service change updates the vector fixture in the same commit.

## On-device tests

`make smoke-test` checks pairing, reconnection, and the Control Service on a flashed device. Only the maintainer runs it. See [CONTRIBUTING.md](../CONTRIBUTING.md#smoke-test-maintainer-on-device) for options.

For a manual check of a feature that the smoke test does not cover:

1. Flash the device and run `make monitor`.
2. Pair a phone, then drive the feature from the companion app.
3. Reconnect after a power cycle. The session keys and settings must persist.
4. Repeat with up to four phones. Settings must stay independent per phone.
