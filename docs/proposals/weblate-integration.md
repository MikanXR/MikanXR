# Proposal: Hosted Weblate for community translations

Accepted and in progress. The Weblate project is connected and a first translation has travelled from the web editor to a pull request. What remains is listed under Verifying it works. The existing workflow (clone, edit `localization/<code>.po` in POEdit, open a pull request) is described in [TRANSLATING.md](../TRANSLATING.md) and stays available either way.

The alternative evaluated alongside this one is in [crowdin-integration.md](./crowdin-integration.md).

---

## Why Weblate

- The Libre plan is free for public projects under an OSI or FSF libre license, with the full feature set up to 160,000 hosted strings. MikanXR is MIT and has about 1,200 strings. The plan is requested from the project's billing page, and its current conditions are listed there.
- Gettext PO is its native format, so `localization/*.po` needs no adaptation.
- Weblate's "Needs editing" state is the gettext fuzzy flag. The provenance model already in place keeps working unchanged and `reviewedCount` keeps meaning what it means. A separate "Approved" state exists on top if a two-stage review is ever wanted.
- Weblate is itself libre software. If the hosted terms change, the same instance can be self-hosted over the same PO files.

---

## Repo-side work

None of it is Weblate-specific. Each piece also helps a human who edits a PO without running the generator.

### 1. PO files are compared by content

Weblate writes PO through its own formatter, which does not match polib's header field order or comment placement. Under a byte comparison Weblate would reformat the file, CI would reformat it back, and every pull request would carry a whole-file diff.

`tools/localization.py` compares the parsed catalog instead. A `.po` is rewritten only when its entries change, meaning a key was added or removed, an English source moved, or a translation or flag changed. Formatting differences are invisible to both `sync` and `check`, and whoever wrote the file last wins. When a rewrite does happen, the header is carried over, so the translator credit and revision date Weblate records survive it.

`resources/localization/*.json`, `manifest.json` and `mikan.pot` stay byte-compared. They are ours alone and nothing else writes them.

### 2. The tables are regenerated in CI

Weblate edits `localization/<code>.po`. The JSON tables it does not touch are what jsDelivr serves and what CI checks, and hosted Weblate cannot run repo scripts. `.github/workflows/weblate-sync.yml` does the regeneration, over two branches with one writer each:

- `weblate`: written only by Weblate. Its pushes are not forced, so a commit it does not have on this branch would make its next push fail.
- `weblate-sync`: written only by the workflow. It is `weblate` plus one commit that runs `python tools/localization.py sync`, force-pushed on every Weblate push.

The workflow refuses to publish when `weblate` changes any file other than `localization/*.po`. It opens the pull request from `weblate-sync` to `main` when none is open.

The push and the pull request are made with a personal access token held in the `WEBLATE_SYNC_TOKEN` secret. GitHub starts no workflow for anything done with a workflow's own token, so with that token the checks `main` requires never ran on the pull request. Dispatching `build-and-test.yml` by hand from the workflow was tried first. Its checks landed on the commit and the pull request still showed none.

`build-and-test.yml` skips pushes to `weblate` and `weblate-sync`. The first has stale tables by construction, and the second is covered by its pull request.

### 3. Translation-only changes skip the Windows build

A translation cannot affect the build, and a Weblate push would otherwise cost a full Windows run each time. Two changes make the fast `localization-check` job a sufficient gate for such a change:

- Glyph coverage moved into `tools/localization.py check`, reading `resources/font/ui_glyph_ranges.txt`. The Windows job fails when that file differs from the ranges `MkGuiTheme` bakes.
- The `changes` job in `build-and-test.yml` skips `build`, `format-check` and `ai-usage-check` when the diff touches only catalogs and generated language tables.

Details are in [localization.md](../reference/localization.md).

---

## GitHub setup

- **The `weblate` user** ([github.com/weblate](https://github.com/weblate), shown as "Weblate (bot)") is a collaborator with the Write role. This is the account hosted Weblate pushes as. Several unrelated accounts share the display name, so check the handle.
- **A webhook** to `https://hosted.weblate.org/hooks/github/` on the push event tells Weblate when `main` moves.
- **The `WEBLATE_SYNC_TOKEN` secret** holds a fine-grained personal access token limited to this repository, with Contents and Pull requests set to read and write. It expires, and the sync workflow fails with a message naming the secret when it does.
- **Translation pull requests are merged with "Create a merge commit"**, never squashed. Weblate rebases onto `main` after the merge and has to find its own commits there. Weblate's documentation is explicit that a squash merge leaves it in a state that needs a manual reset.

---

## Weblate setup

The component uses the plain Git integration. The Hosted Weblate GitHub app was tried first and does not fit: an app-managed component locks its push URL, its push branch and its version control system, in the settings page and through the API, so it cannot be pointed at a `weblate` branch.

Create the component from the "From version control" tab, or through the REST API:

- Version control system: `Git`
- Source code repository: `https://github.com/MikanXR/MikanXR.git`
- Repository branch: `main`
- Repository push URL: `git@github.com:MikanXR/MikanXR.git`
- Push branch: `weblate`
- File format: `gettext PO file`
- File mask: `localization/*.po`
- Template for new translations: `localization/mikan.pot`
- Translation license: MIT
- Merge style: rebase
- Push on commit: on
- Long lines wrapping: wrap only at newlines, which is what `tools/localization.py` writes. The "no wrapping" option is different: it puts the whole PO header on one line.

Settings that matter for this project:

- **Adding new translation: Contact maintainers.** A new language needs a font and glyph ranges baked into the build, so it is a code change, not a translation ([localization.md](../reference/localization.md)). This setting gives a translator a "message the maintainer" button instead of letting them create a `.po` for a script the build cannot render, and it keeps `localization/languages.json` and the manifest consistent, since Weblate would not update either.

- **Do not enable the msgmerge addon.** `tools/localization.py sync` owns the merge from `mikan.pot` into each catalog. Two mergers with different output would fight.

- **Do not enable "Cleanup translation files"** for the same reason.

- **Do not enable "Squash Git commits".** It rewrites commits Weblate has already pushed, which has the same effect on the rebase as a squash merge.

A project-scoped API token is enough to read the component's settings and repository state, and to trigger a commit without waiting out the 24 hour commit delay.

---

## Verifying it works

- [x] An edit to one Japanese string in Weblate, with "Needs editing" cleared, is pushed to the `weblate` branch.
- [x] The workflow pushes `weblate-sync` with a regenerated `resources/localization/ja.json` and opens a pull request.
- [x] `ja.json` shows `reviewedCount` incremented by exactly one, and the diff of `ja.po` is the edited string rather than a whole-file reformat.
- [ ] The required checks report on the pull request, with `build`, `format-check` and `ai-usage-check` skipped and `localization-check` passing.
- [ ] After a merge with a merge commit, Weblate rebases onto `main` without reporting a conflict and a second edit pushes cleanly.
- [ ] A translation containing `##`, entered through the Weblate editor, fails the `Weblate Sync` run naming the key and leaves the pull request at its last good state. Weblate's own checks should also flag it before it gets that far.
- [ ] A translation containing a character outside the baked glyph ranges fails the same way.

---

## Risks and exits

- The personal access token expires. Translations then stop reaching pull requests until it is renewed, and nothing is lost in the meantime: the edits stay in Weblate and on the `weblate` branch.
- If `main` gains English strings while a translation pull request is open, the generated tables can conflict. Weblate rebases and pushes again when `main` moves, and a manual run of `Weblate Sync` rebuilds the branch in between.
- The `weblate` account can push to any unprotected branch of the repository. Branch protection on `main` is unaffected, and the workflow publishes nothing from the `weblate` branch that touches a file other than a catalog.
- Backing out is cheap and non-destructive. The PO files live in git and remain the source of truth, so disconnecting the component leaves the repository in a working state with the POEdit workflow intact.

---

## Other repositories

MikanTrack and MikanStudio become further components of the same Weblate project once their own string pipelines produce PO catalogs. Each needs the repo-side pieces above, the `weblate` collaborator, the webhook and its own token secret, and each has to be public and libre licensed to stay on the Libre plan.
