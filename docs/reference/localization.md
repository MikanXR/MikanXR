# Localization

How UI text gets from a developer's keyboard to a translated string on screen: the two directories, the generator between them, the gettext conventions volunteers work in, and the gates that keep a bad string out of an installed build.

Call-site rules (which `LocText.h` helper to use, how keys are named, what stays untranslated) are in [standards.md](./standards.md). The `LocalizationManager` module boundary is in [modules.md](./modules.md).

---

## Two directories

`localization/` holds sources. Nothing here is installed or served.

- `languages.json`: the ordered registry of every language, `code` to `nativeName`. English is first. This drives the manifest, the picker order, and the display name each table carries.
- `mikan.pot`: the gettext template, generated from the English table.
- `<code>.po`: one gettext catalog per translation, owned by translators.

`resources/localization/` holds what ships and what the CDN serves.

- `en.json`: the English table, authored by developers. This is where a new UI string is born.
- `<code>.json`: generated from `<code>.po`. Never hand-edited.
- `manifest.json`: generated from `languages.json`. The file list the remote fetcher walks.

The JSON tables keep the shape they have always had, so a build that predates this pipeline reads them unchanged.

---

## The generator

`tools/localization.py` is the only thing that writes the generated files. It needs `polib`, which `tools/SetupDevEnvironment.bat` installs into the repo's `.venv` from `tools/requirements.txt`. The `LocalizationSync`/`LocalizationCheck` targets and `SyncLocalizationFiles.bat` use that interpreter when it exists.

- `sync`: normalize `en.json`, rebuild `mikan.pot`, merge it into every `.po`, then write each `<code>.json` and `manifest.json`. Idempotent, so running it twice writes nothing the second time.
- `check`: run `sync` in memory and fail if any file on disk differs, or if any string breaks a rule. This is the CI gate, and it is what a translator runs locally.
- `new <code> "<nativeName>"`: register a language and create its catalog. `--from-json <path>` seeds it from an existing table, flagging every seeded string fuzzy because a bulk import is machine output until a person reads it. `--reviewed` opts out of that flag.
- `stats`: per-language translated and reviewed counts.

`LocalizationSync` and `LocalizationCheck` are CMake targets wrapping the first two, grouped beside `FormatFix`/`FormatCheck` in the IDE.

---

## Catalog conventions

An entry is identified by its `msgctxt`, which is the flat `section.key`. The `msgid` is the English source text, so a translator reads real English rather than an identifier, and translation memory works across projects.

```
#. Section: alignCameraByOriginMarker
#, c-format
msgctxt "alignCameraByOriginMarker.markerDetectedFmt"
msgid "Origin Marker Detected: %s"
msgstr "原点マーカー検出: %s"
```

- `#, fuzzy` means the string has not been vouched for by a person. POEdit shows it as "Needs work". Every machine translation carries it, and clearing it is the act of review.
- `#, c-format` is emitted for any English text carrying printf specifiers. It turns on POEdit's own specifier checking, which catches a mismatch before the pull request exists.
- `#. Section:` names the UI unit the key belongs to.
- When the English text of a key changes, `sync` keeps the translation, sets fuzzy, and records the old source as `#| msgid` so the translator sees what moved.
- A key English no longer defines becomes an obsolete `#~` entry rather than being deleted, so the work returns with the key.

---

## The generated tables

`_meta` carries the language identity and its progress:

```json
"_meta": { "code": "ja", "nativeName": "日本語", "stringCount": 903, "translatedCount": 902, "reviewedCount": 0 }
```

`translatedCount` counts non-empty translations. `reviewedCount` counts those that are not fuzzy. A table written before these fields existed reports zeroes, which reads as "no claim made" rather than "nothing translated".

An untranslated key is written into the table as its English text rather than being omitted. Every table therefore defines every key, which is what keeps the loader's missing-key warning meaningful for a half-finished language.

`LocalizationManager::LanguageInfo` exposes the three counts. The Project Settings panel uses them to say how much of the active translation a person has reviewed, so an unreviewed string does not read as the project's own wording.

---

## Finding the key behind a bad string

Two diagnostics, both under `View` in the main window and in the node editors.

**Show String Keys** (F9) makes every fetch answer with its own key, so a string that reads wrong names itself in the widget it was drawn in. It covers every string kind: display text, tooltips, format strings, widget labels, window titles, combo entries, and the descriptor-generated panel labels. The flag is global across all four ImGui contexts and is not persisted.

A key that is visible with the mode off is a key no table defines, which is a code bug rather than a translation one.

Two mechanical notes. Window titles keep their `###` suffix in this mode, because the ImGui ID is the English title and a window that changed identity mid-session would lose its docked position. Widget labels do not: ImGui's `ImHashStr` resets only at `###`, so a `text##key` label's ID already moves with the translation, and answering with the bare key moves it no further than a language switch does.

**ID Stack Tool** is ImGui's own hover inspector. Because `locLabel` puts the key after `##`, hovering an interactive widget reports its key without leaving the normal view. It only sees widgets that own an ID, so plain text needs show-keys mode. Unlike show-keys it is per window, since it reports the hovered item of its own context.

Both live in `src/Editor/Localization/LocDebugUI.cpp`.

---

## Adding a UI string

Add the key to `resources/localization/en.json`, then run `sync`. The key reaches every catalog as an untranslated entry, and every generated table as the English text. `check` fails the build if the sync step was skipped.

`sync` also normalizes `en.json`: `_meta` first, sections and keys sorted, tab indent. A key appended anywhere lands in the right place on the next run.

---

## Adding a language

Registering a language is one command. Making it render is a feature, because the font atlas is baked into the build: `MkGuiTheme::loadFonts` loads Mochiy Pop One over `getJapaneseGlyphRanges`, and `getUiGlyphRanges` is the set every string is checked against. A language outside those ranges needs a font that covers it and a range table to match before its catalog is worth filling.

The CDN serves `resources/localization` off `main`, so a language merged there is fetched by builds that shipped before it existed. `overlayLanguageFile` therefore skips a language the build does not bundle when its own native name has codepoints the baked ranges do not cover. An older build ignores the new language instead of listing it and drawing tofu.

---

## Gates

Three layers, cheapest first.

- POEdit checks format specifiers live, while the translator types.
- `localization-check` in CI runs `tools/localization.py check` on Linux in under a minute: generated files current, no `##` in any string, printf specifiers matching English, non-empty and unique English text for every `windows.*` key.
- `run_localization_unit_tests` in `MikanCmd -runTests` is the backstop and the authority. It loads the real tables through the real loader and additionally owns glyph coverage, which is not duplicated in Python because the range table lives in `MkGuiTheme` and a second copy would drift.

The CDN pin matters here. `LocalizationManager::startRemoteFetch` fetches from `@main` rather than a version tag, because jsDelivr caches a tag permanently and the point of remote localization is that a fix reaches users without a build. The consequence is that CI is the only gate between a merged typo and every installed build's next fetch.

A parse failure degrades rather than breaking: `overlayLanguageFile` bails on a malformed cache file and the bundled table survives.
