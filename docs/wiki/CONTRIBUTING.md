# Wiki Contributing Guide

## General writing guidelines

All wiki pages are read by **app and device users, not developers**. Write every sentence as if the reader has never seen a line of code or flashed a device.

**Rules that apply to every page:**

- No code symbols, function names, opcode names, or file paths — ever. If you need to refer to something technical, describe what it does instead.
  - Bad: The app sends `Opcode.TOGGLE_AUTOSPIN` to the device.
  - Good: The app tells the device to turn Autospin on or off.
  - Bad: `write_global_settings_to_nvs()` persists the settings.
  - Good: Save to device writes the settings to the device's flash storage.
- No internal Android, BLE, or firmware concepts: no "ViewModel", "GATT characteristic", "NVS", "FreeRTOS task", "coroutine", etc. Replace with plain English.
  - Bad: The ViewModel issues a GATT write to the Control Service characteristic.
  - Good: The app sends the change to the device over Bluetooth.
- No library or framework names in descriptions unless the library itself is the subject (e.g. the Acknowledgements page). Even then, describe what it does for the user, not how it works internally.
- No firmware build steps, ESP-IDF commands, or contribution/development workflow. That belongs in the repository's `AGENTS.md` and `README.md`, not the wiki.
- `<code>` is allowed **only** for UI labels or literal values the user must type or read verbatim (e.g. the `RESET` confirmation word).
- Active voice, present tense. One idea per sentence.
- Audience test: could a non-technical friend understand every sentence? If not, rewrite.

---

## Page structure

Every page has one content type. Keep each page to a single type.

- **Tutorial.** Teaches a first-time task by doing. Structure: goal, numbered steps, a visible result after each step, the final outcome.
- **How-to guide.** Completes one task or solves one problem. Structure: goal, prerequisites, numbered steps, expected result.
- **Reference.** Looks up facts: options, values, and controls. Structure: one heading per entry. See "Reference pages".
- **Explanation.** Explains why a feature works the way it does. Structure: context, core concept, alternatives and trade-offs.

Do not mix types on one page. If content fits two types, create two pages and link between them.

### Screenshots

Put a screenshot in the section it illustrates. Wrap the section in `<section class="has-shot">` and place `<aside class="shot"><img class="phone-shot" …></aside>` right after the section heading. On wide screens the screenshot floats in the right margin and scrolls with that section only. On narrow screens it sits under the heading. Use one screenshot per section.

Capture in the light theme with `adb exec-out screencap -p`. Crop the status bar and remove device addresses and other personal data. Save to `screenshots/`.

### Reference pages

Never use a `<table>`. Tables squash or scroll sideways on phones. Give each entry its own heading instead.

- Open the section with one sentence that says what the entries are.
- Give each entry an `<h3>` under the section `<h2>`. Write the heading as the entry name only.
- Give every entry heading a unique `id`. Prefix it with the section `id`, for example `status-light-fast-blinking`.
- Write the description in plain sentences below the heading. Use a bullet list for steps or sub-points.
- End with `<p><strong>Default:</strong> …</p>` when the entry has a default. Use the same bold label style for other facts, such as `<strong>Meaning:</strong>`.
- Keep the same facts, in the same order, for every entry in a section.

---

## Navigation and page moves

### Sidebar groups

The sidebar has five groups in this fixed order:

- **Start.** First-time setup. Pages: Getting Started, Hardware, Install & Update Firmware.
- **Use.** Everyday tasks. Pages: Connect & Pair, Device Profiles.
- **Configure.** Settings and diagnostics. Pages: Settings, Diagnostics & Danger Zone.
- **Help.** Fixing problems. Pages: Common Tasks & FAQ, Troubleshooting.
- **About.** Project information. Pages: Changelog, Privacy, Acknowledgements.

All nav items live in `docs/wiki/wiki-init.js`, in the `NAV_ITEMS` array. Each group has a `group` name and an `items` array. Read the live array in that file for the current labels.

Shape of one group:

```js
{ group: 'Use', items: [
  { href: 'connect.html', label: 'Connect & Pair' },
] },
```

### Adding a new page

1. Create `docs/wiki/<page>.html` with the same shell as an existing page: DOCTYPE, `style.css`, the DocSearch stylesheet, `<div class="layout"><main>…</main></div>`, and `<script src="wiki-init.js"></script>`.
2. Add the SEO head block from "SEO basics" and one `<h1>`.
3. Add `{ href: '<page>.html', label: '<Label>' }` to the `items` array of the group where it belongs. Write `&amp;` for ampersands in labels.
4. Add the page to `sitemap.xml`.

`wiki-init.js` injects the sidebar into every page at runtime, so no other file needs an edit. External links go in `EXT_ITEMS`, not in `NAV_ITEMS`.

### Merging and moving pages

Merge a feature into an existing page when it is a sub-topic of that page, or when it has under about 300 words of content. Add a new page only for a separate user goal.

When a page moves or merges into another page:

1. Remove the old file from `NAV_ITEMS` and from `sitemap.xml`.
2. Keep the old file as a redirect stub. Do not delete it, so old links still work.
3. Give the stub a `<title>` that matches the new page, a `<meta http-equiv="refresh">` to the new page, a canonical link to the new page, and `<meta name="robots" content="noindex">`.
4. Put one plain link in the body: "This page moved. See <a href="...">Page Name</a>."

Example stub for a page merged into Install & Update Firmware:

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Install &amp; Update Firmware — pgpemu</title>
  <meta http-equiv="refresh" content="0; url=install.html#update">
  <link rel="canonical" href="https://pgpemu.shrtcts.fr/install.html">
  <meta name="robots" content="noindex">
</head>
<body>
<p>This page moved. See <a href="install.html#update">Install &amp; Update Firmware</a>.</p>
</body>
</html>
```

---

## SEO basics

Every real page, not a redirect stub, has these tags in its `<head>`, after the charset and viewport tags:

```html
<title>Settings — pgpemu</title>
<meta name="description" content="Change how your PGPemu device advertises over Bluetooth, its status light, connection limit, log level, and Pokémon GO alerts.">
<link rel="canonical" href="https://pgpemu.shrtcts.fr/settings.html">
<meta property="og:type" content="website">
<meta property="og:site_name" content="pgpemu">
<meta property="og:title" content="Settings — pgpemu">
<meta property="og:description" content="Change how your PGPemu device advertises over Bluetooth, its status light, connection limit, log level, and Pokémon GO alerts.">
<meta property="og:url" content="https://pgpemu.shrtcts.fr/settings.html">
<meta property="og:image" content="https://pgpemu.shrtcts.fr/icon.png">
<meta name="twitter:card" content="summary">
<meta name="twitter:title" content="Settings — pgpemu">
<meta name="twitter:description" content="Change how your PGPemu device advertises over Bluetooth, its status light, connection limit, log level, and Pokémon GO alerts.">
<meta name="twitter:image" content="https://pgpemu.shrtcts.fr/icon.png">
```

Rules:

- **Title:** `<Page name> — pgpemu`. Unique per page. Write `&amp;` for an ampersand.
- **Description:** one plain sentence under 160 characters. Unique per page.
- **Canonical:** `https://pgpemu.shrtcts.fr/<page>.html`. The home page uses `https://pgpemu.shrtcts.fr/`. `og:url` repeats the canonical URL.
- **Social tags:** repeat the title and description. The card type is `summary`, because `icon.png` is square.
- **Headings:** one `<h1>` per page with a unique `id`. Every `<h2>` and `<h3>` has a unique `id`.
- **Links and images:** link text names the target. Never "click here" or "more". Every image has `alt` text.
- **Sitemap:** every real page is listed in `sitemap.xml`. `robots.txt` points to the sitemap and needs no per-page edit.

---

## External resources

Pages load nothing from outside the site except DocSearch. The one exception is the pinned ESP Web Tools script on `install.html`, which flashes the device through the browser.

---

## Maintaining `changelog.html`

### Source of truth

`CHANGELOG.md` at the repo root is the exhaustive changelog generated by conventional commits tooling. `changelog.html` is a curated, user-facing distillation of it — not a mirror.

### When to update

Update `changelog.html` whenever a new version is added to `CHANGELOG.md` (i.e. a new `## [x.y.z]` section appears).

### Structure

Each version is an `<h3>` whose text is a link to the GitHub release, followed by a `<ul>`:

```html
<h3 id="vXYZ"><a href="https://github.com/shortcuts/pgpemu/releases/tag/vX.Y.Z"
       target="_blank" rel="noopener">X.Y.Z</a> — Month DD, YYYY</h3>
<ul>
  <li>…</li>
</ul>
```

- CI fails the release if no heading links to `releases/tag/vX.Y.Z`, so the entry must be in the release PR before it is merged.
- Versions appear in descending order (newest first).
- The release URL pattern is `https://github.com/shortcuts/pgpemu/releases/tag/vX.Y.Z`.
- Date format: `Month DD, YYYY` (e.g. `June 13, 2026`). Derive from the CHANGELOG.md heading date `(YYYY-MM-DD)`.

### Content rules

**Maximum 5 bullets per version.** Prioritise in this order:
1. New user-visible features
2. Significant UX improvements
3. Fixes for problems users would notice (dropped connections, wrong status, settings not saving)

**Skip entirely:**
- Pure maintenance releases (version bump only, trigger-release commits, lint fixes) — merge them silently into the previous version's section, or omit the version heading if there is nothing user-visible.
- Internal refactors, test changes, CI/build changes, code style fixes.
- Firmware or app internals that don't change what the user sees or does.

**Writing style:**
- Audience is app and device users, not developers — no code terms, opcode names, or firmware internals.
- One sentence per bullet, active voice, present tense describing what the app or device now does.
- Bad: "TOGGLE_AUTOSPIN now clamps the profile index before dispatch."
- Good: "Fixed a crash when toggling Autospin on the last device profile."

### Example

```html
<h3 id="v110"><a href="https://github.com/shortcuts/pgpemu/releases/tag/v1.1.0"
       target="_blank" rel="noopener">1.1.0</a> — January 1, 2027</h3>
<ul>
  <li>New: the companion app now shows battery level for connected devices.</li>
  <li>Autocatch now resumes automatically the moment your Pokémon box has room again.</li>
  <li>Fixed: the device profile counters could reset unexpectedly on reconnect.</li>
</ul>
```
