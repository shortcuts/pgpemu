# Contributing

## Building

See `README.md`, "Installation & Setup". CLI only (no VSCode extension):

```bash
make install-deps && make setup-esp-idf   # once per machine
make build
```

Never flash or open a monitor in automated/agent sessions unless explicitly asked.

## Before Opening a PR

1. Read [AGENTS.md](AGENTS.md): scope, architecture rules, doc maintenance policy.
2. One feature/fix per PR, small atomic commits. Messages state what and why.
3. Doc changes ship in the same commit as the code change.

## Required Checks

```bash
make format
make build
make test
make companion-format  # if companion-app/ changed
make companion-test   # if companion-app/ changed
make companion-lint   # if companion-app/ changed (enforces stringResource() for Compose text)
make companion-build-release  # if R8/proguard rules changed (minified release APK)
```

`make companion-build-release` signs the APK when `KEYSTORE_PATH`, `STORE_PASSWORD`,
`KEY_ALIAS` and `KEY_PASSWORD` are set in the environment. Without `KEYSTORE_PATH` the
release APK stays unsigned. The release workflow signs the APK and attaches it to the
GitHub Release using the repo secrets `KEYSTORE_BASE64`, `STORE_PASSWORD`, `KEY_ALIAS`
and `KEY_PASSWORD`.

The manual **Alpha Release** workflow (Actions tab, `workflow_dispatch`) builds `main` and publishes a signed
prerelease tagged `v<base>-alpha.<run_number>`, reusing the same signing secrets. Alphas share the base
version's `versionCode`, so an alpha cannot be installed over the same-base stable build: uninstall first.

## Smoke Test (maintainer, on device)

`make smoke-test` checks pairing, reconnection and the Control Service on a flashed device. The maintainer runs it. Agents and CI never run it.

```bash
python3 -m pip install bleak
make smoke-test ARGS="--name <advertised name> --cycles 10"
```

* Use `--address <addr>` instead of `--name` when you know the address.
* The first write bonds the link. Accept the OS pairing dialog.
* It checks every read opcode, every fixture error vector with the `default` setup, and the malformed-payload reject path of each mutating opcode.
* It skips destructive opcodes (restart, forget, reset, provisioning, OTA transfer, valid setting writes) and prints them as `SKIP (destructive)`.
* Each reconnect cycle compares `GET_DEVICE_INFO` with the first sample: protocol version, reset reason, uptime (reboot) and free heap.
* `--heap-tolerance <bytes>` sets the allowed free-heap drop. Default 1024.

`make smoke-test-selftest` runs the script's hardware-free logic tests. `make test` includes it.

The firmware and companion app share one version managed by release-please. Never edit the `x-release-please-version` line in `companion-app/app/build.gradle.kts` by hand.

Fix all errors. Never suppress without an inline comment explaining the false positive.

## Reference Docs

| Topic | File |
|-------|------|
| Agent rules and module table | [AGENTS.md](AGENTS.md) |
| Architecture | [docs/architecture.md](docs/architecture.md) |
| Companion app code style | [docs/companion-code-style.md](docs/companion-code-style.md) |
| Companion app testing | [docs/companion-testing.md](docs/companion-testing.md) |
| Domain vocabulary | [CONTEXT.md](CONTEXT.md) |
| Wiki writing rules | [docs/wiki/CONTRIBUTING.md](docs/wiki/CONTRIBUTING.md) |
