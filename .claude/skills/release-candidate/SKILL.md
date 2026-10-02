---
name: release-candidate
description: |
  Go/no-go audit of main before tagging a release: build pipeline, tests, doc
  consistency. Logs every failure to radin's backlog.
  Use for "release checks", "pre-release audit", "is this ready to ship?".
---

# Release Candidate Auditor

Every failure becomes a backlog entry detailed enough to act on without re-running the
audit. Route every finding through the `radin-record` skill, the only writer to
`BACKLOG.md`. Never flash or touch hardware.

Done when every step below has run and every failure it found is logged.

## Step 0: Baseline

```bash
bash "$HOME/.claude/radin-lib/radin-namespace.sh"
grep -c '^### ' "$BACKLOG_FILE" 2>/dev/null || echo "0"
git describe --tags --abbrev=0 2>/dev/null
```

The tag bounds "since last release" below.

## Step 1: Build pipeline

Hard gates, stop at first non-zero exit:

```bash
make format 2>&1
make build  2>&1
```

Per failure, invoke `radin-record`: "Log a fix: build pipeline failure in `make <step>`. Exit code <N>. Output: <last 30 lines>".

## Step 2: Tests

```bash
make test 2>&1
make companion-test 2>&1
```

Per failure, invoke `radin-record`: "Log a fix: `<target>` failed. Failures: <names / summary>".

## Step 3: Documentation consistency

```bash
git diff --name-only <last-release-tag> HEAD
```

Group changed `pgpemu-esp32/main/` and `companion-app/` sources by feature area. Per area verify:

1. `AGENTS.md` Key Modules table matches real modules.
2. `docs/architecture.md` matches layering, connection flow, Control Service.
3. `README.md` names user-visible features.
4. `docs/wiki/` pages match current behavior and pass the audience test in `docs/wiki/CONTRIBUTING.md`.
5. `CONTEXT.md` covers new domain terms.

Per gap, invoke `radin-record`: "Log a fix: documentation gap in <area>. Changed sources: <paths>. Gap: <what is missing>. Doc file: <path>." Skip cosmetic nits.

## Step 4: Verdict

Recount entries against the Step 0 baseline.

* None new: "✅ Release is ready: build, tests, documentation consistency passed." Then remind the user to update `docs/wiki/changelog.html` for user-visible changes and watch for the release-please PR.
* New: "❌ Release is NOT ready. N issue(s) logged." List the headings.

Lint errors stay visible: a suppression hiding a real issue is itself a finding.
