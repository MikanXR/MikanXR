# Proposal: Hosted Weblate for community translations

Parked, not decided. This is the runbook for the day someone asks to translate MikanXR and wants a web editor instead of POEdit. The current workflow (clone, edit `localization/<code>.po` in POEdit, open a pull request) is described in [TRANSLATING.md](../TRANSLATING.md) and stays the fallback either way.

The alternative evaluated alongside this one is in [crowdin-integration.md](./crowdin-integration.md).

---

## Why Weblate

- The Libre plan is free for public projects under an OSI or FSF libre license, with the full feature set up to 160,000 hosted strings. MikanXR is MIT and has about 900 strings. Libre signup no longer waits on a manual approval queue.
- Gettext PO is its native format, so `localization/*.po` needs no adaptation.
- Weblate's "Needs editing" state is the gettext fuzzy flag. The provenance model already in place keeps working unchanged and `reviewedCount` keeps meaning what it means. A separate "Approved" state exists on top if a two-stage review is ever wanted.
- Weblate is itself libre software. If the hosted terms change, the same instance can be self-hosted over the same PO files.

---

## Prerequisite work in this repo

Two things have to land before Weblate is connected. Neither is Weblate-specific: both also help a human who edits a PO without running the generator.

### 1. Stop comparing PO files byte for byte

`tools/localization.py check` currently fails when a generated file on disk differs from what it would write, and the `.po` files are in that set. Weblate writes PO through its own formatter, which will not match polib's wrapping, header field order, or comment placement. Left alone, Weblate reformats the file, CI reformats it back, and every pull request carries a whole-file diff.

Change `build_outputs` to compare the *parsed* catalog rather than the rendered text: a `.po` is only rewritten when its entries actually changed, meaning a key was added or removed, an English source moved, or a translation or flag changed. Formatting differences are then invisible to both `sync` and `check`, and whoever wrote the file wins.

`resources/localization/*.json`, `manifest.json` and `mikan.pot` stay byte-compared. They are ours alone and nothing else writes them.

### 2. Regenerate the tables in CI

Weblate edits `localization/<code>.po`. The JSON tables it does not touch are what jsDelivr serves and what CI checks, so a Weblate pull request would arrive with `localization-check` red.

Hosted Weblate cannot run repo scripts (its script addon needs server-side allowlisting), so the regeneration belongs in GitHub Actions:

- Point Weblate's push at a `weblate` branch in this repository rather than at a fork, which keeps the token story simple: a workflow can push to a branch in its own repo with `GITHUB_TOKEN`, but not to a fork's branch.
- Add a workflow on pushes to `weblate` that runs `python tools/localization.py sync`, commits any drift, and opens or updates a pull request against `main`.

Roughly thirty lines of YAML. The pull request that reaches `main` is then already regenerated and passes the existing gate unchanged.

---

## Weblate setup

Create the project at [hosted.weblate.org](https://hosted.weblate.org/), then one component:

- Source code repository: `https://github.com/brendanwalker/MikanXR`
- Repository branch: `main`
- File format: `gettext PO file`
- File mask: `localization/*.po`
- Template for new translations: `localization/mikan.pot`
- Translation license: MIT
- Repository push URL and push branch: this repository, branch `weblate`
- Merge style: rebase

Settings that matter for this project:

- **Adding new translation: Contact maintainers.** A new language needs a font and glyph ranges baked into the build, so it is a code change, not a translation ([localization.md](../reference/localization.md)). This setting gives a translator a "message the maintainer" button instead of letting them create a `.po` for a script the build cannot render, and it keeps `localization/languages.json` and the manifest consistent, since Weblate would not update either.
- **Do not enable the msgmerge addon.** `tools/localization.py sync` owns the merge from `mikan.pot` into each catalog. Two mergers with different output would fight.
- **Do not enable "Cleanup translation files"** for the same reason.
- "Squash Git commits" per language is worth enabling so a pull request is one commit per translator rather than one per string.

---

## Verifying it works

1. Make a trivial edit to one Japanese string in Weblate and clear its "Needs editing" flag.
2. Confirm Weblate pushes to the `weblate` branch, and that the workflow regenerates `resources/localization/ja.json` and opens a pull request.
3. Confirm the pull request is green: `localization-check` passes, `format-check` is untouched, and the Windows job's `run_localization_unit_tests` passes.
4. Confirm `ja.json` shows `reviewedCount` incremented by exactly one.
5. Merge, then confirm Weblate pulls the regenerated tables back without reporting a conflict.
6. Break it on purpose: put `##` in a translation through the Weblate editor and confirm CI rejects the pull request naming the key. Weblate's own checks should also flag it before it ever gets that far.

---

## Risks and exits

- Weblate rewriting PO formatting is handled by the parsed-comparison change above. Verify it on the very first pull request rather than assuming.
- Weblate needs write access to the repository (a GitHub App install or a bot account with push rights to the `weblate` branch). Branch protection on `main` is unaffected, since Weblate never pushes there.
- Backing out is cheap and non-destructive. The PO files live in git and remain the source of truth, so disconnecting the component leaves the repository in a working state with the POEdit workflow intact.
