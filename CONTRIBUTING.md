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
release APK stays unsigned.

Fix all errors. Never suppress without an inline comment explaining the false positive.

## Reference Docs

| Topic | File |
|-------|------|
| Agent rules and module table | [AGENTS.md](AGENTS.md) |
| Architecture | [docs/architecture.md](docs/architecture.md) |
| Domain vocabulary | [CONTEXT.md](CONTEXT.md) |
| Wiki writing rules | [docs/wiki/CONTRIBUTING.md](docs/wiki/CONTRIBUTING.md) |
