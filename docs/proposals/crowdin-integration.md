# Proposal: Crowdin for community translations

Parked, not decided, and the second choice behind [weblate-integration.md](./weblate-integration.md). This is the runbook if Crowdin is picked instead, either because a translator already knows it or because its editor is judged worth the tradeoffs below.

The current workflow (clone, edit `localization/<code>.po` in POEdit, open a pull request) is described in [TRANSLATING.md](../TRANSLATING.md) and stays the fallback either way.

---

## Why Crowdin, and why not

For:

- Free for open-source projects, granted through a license request form rather than the standard free tier. MikanXR is MIT with public source and no related commercial product, so it qualifies.
- The most polished translator-facing editor of the options, with a shorter learning curve for someone who has never seen a PO file.
- A first-party GitHub integration that opens pull requests without any workflow of our own.

Against:

- The open-source grant requires contributing project translations to Crowdin's Global Translation Memory. That is a licensing decision about contributors' work, not just a config choice.
- Proprietary and hosted. There is no self-hosted escape hatch if terms change, unlike Weblate.
- **The fuzzy round trip is unproven and is the deciding risk.** The whole provenance model rests on `#, fuzzy` meaning "not vouched for by a person", and `reviewedCount` is computed from it. Crowdin models review as its own "Approved" status rather than as a gettext flag, and how that maps onto the exported `#, fuzzy` depends on export settings. Validate this before committing to Crowdin, not after.

---

## Pre-flight: prove the fuzzy round trip

Do this on a throwaway Crowdin project before any repo change.

1. Upload `localization/mikan.pot` as the source and `localization/ja.po` as existing translations. Every entry in that file is currently fuzzy.
2. Confirm Crowdin imports them as unapproved rather than silently promoting them to approved translations.
3. Approve exactly one string in the editor.
4. Export the PO and diff it against the input.

The result to require: every unapproved entry still carries `#, fuzzy`, and the one approved entry does not. If Crowdin exports without fuzzy flags, or flags everything, the provenance model breaks and either the export settings need fixing or Crowdin is the wrong tool. Do not proceed until this diff looks right.

---

## Prerequisite work in this repo

The same two items the Weblate proposal describes, for the same reasons. Both are tool-agnostic and also help a human who edits a PO without running the generator.

- **Stop comparing PO files byte for byte.** Crowdin writes PO through its own formatter, which will not match polib's output. Change `build_outputs` in `tools/localization.py` to compare the parsed catalog rather than the rendered text, so a `.po` is only rewritten when its entries actually changed. `resources/localization/*.json`, `manifest.json` and `mikan.pot` stay byte-compared.
- **Regenerate the tables in CI.** Crowdin edits `localization/<code>.po` and does not touch the JSON tables that jsDelivr serves, so its pull requests would arrive with `localization-check` red. A workflow has to run `python tools/localization.py sync` and commit the result onto the translation branch before the pull request is reviewable.

The second item is more awkward here than with Weblate. Crowdin pushes to an `l10n_main` branch that it creates, recreates after each merge, and treats as its own. A workflow committing onto that branch has to tolerate Crowdin force-pushing over it. Two ways out:

- Let the workflow open a separate pull request from `l10n_main` into an intermediate branch it owns, and regenerate there.
- Or run the sync as a step in a scheduled Crowdin GitHub Action pipeline rather than using the native integration, so the download and the regeneration happen in one job that we control end to end.

The second is more predictable and is what the setup below assumes.

---

## Crowdin setup

1. Create the project, then apply through the [open source license request form](https://crowdin.com/page/open-source-project-setup-request). The form requires the project to already exist.
2. Add `crowdin.yml` at the repository root:

```yaml
files:
  - source: /localization/mikan.pot
    translation: /localization/%locale_with_underscore%.po
```

Use `%locale_with_underscore%` or `%language_code%` rather than `%two_letters_code%`. Today's codes are `en` and `ja`, but a future `pt-BR` would silently collapse to `pt` under the two-letter placeholder.

3. Use the [Crowdin GitHub Action](https://support.crowdin.com/github-integration/) rather than the native integration, on a schedule plus a manual trigger. The job uploads sources, downloads translations, runs `python tools/localization.py sync`, and opens a single pull request. Secrets needed: `CROWDIN_PROJECT_ID` and `CROWDIN_PERSONAL_TOKEN`.

Settings that matter for this project:

- **Disable letting translators request new languages.** A new language needs a font and glyph ranges baked into the build, so it is a code change ([localization.md](../reference/localization.md)). Crowdin would also not update `localization/languages.json` or the manifest, so a language added there alone would produce no shipped table.
- Export settings must preserve `#, fuzzy` for unapproved strings, per the pre-flight above.
- Do not enable "Export only approved translations". A machine translation that nobody has reviewed should still ship, flagged, rather than falling back to English.

---

## Verifying it works

1. Approve one Japanese string in Crowdin and run the action manually.
2. Confirm the pull request contains both the `localization/ja.po` change and the regenerated `resources/localization/ja.json`.
3. Confirm `ja.json` shows `reviewedCount` incremented by exactly one. This is the end-to-end proof that the fuzzy mapping survived the round trip.
4. Confirm the pull request is green: `localization-check`, `format-check`, and the Windows job's `run_localization_unit_tests`.
5. Break it on purpose: put `##` in a translation and confirm CI rejects the pull request naming the key.
6. Merge, then run the action again on an unchanged project and confirm it opens no pull request. A pipeline that produces churn on every run is worse than no pipeline.

---

## Risks and exits

- The fuzzy mapping is the one that can sink this. Step 3 above is the real gate, not step 4.
- Contributing to the Global Translation Memory is a condition of the free tier. Decide whether that is acceptable for translations contributed by other people before applying.
- Crowdin recommends deleting its `l10n_` branch after merging to avoid conflicts, and recreates it as needed. Expect branch churn regardless of which integration mode is used.
- Backing out is cheap and non-destructive. The PO files live in git and remain the source of truth, so disconnecting the project leaves the repository working with the POEdit workflow intact.
