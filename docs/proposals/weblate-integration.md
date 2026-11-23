# Proposal: Hosted Weblate for community translations

Accepted and in progress. The repo-side work is done. Connecting the Weblate project and running the verification below is what remains. The existing workflow (clone, edit `localization/<code>.po` in POEdit, open a pull request) is described in [TRANSLATING.md](../TRANSLATING.md) and stays available either way.

The alternative evaluated alongside this one is in [crowdin-integration.md](./crowdin-integration.md).

---

## Why Weblate

- The Libre plan is free for public projects under an OSI or FSF libre license, with the full feature set up to 160,000 hosted strings. MikanXR is MIT and has about 1,200 strings. The plan is requested from the project's billing page, and its current conditions are listed there.
- Gettext PO is its native format, so `localization/*.po` needs no adaptation.
- Weblate's "Needs editing" state is the gettext fuzzy flag. The provenance model already in place keeps working unchanged and `reviewedCount` keeps meaning what it means. A separate "Approved" state exists on top if a two-stage review is ever wanted.
- Weblate is itself libre software. If the hosted terms change, the same instance can be self-hosted over the same PO files.

---

## Repo-side work (done)

Neither piece is Weblate-specific. Both also help a human who edits a PO without running the generator.

### 1. PO files are compared by content

Weblate writes PO through its own formatter, which does not match polib's wrapping, header field order, or comment placement. Under a byte comparison Weblate would reformat the file, CI would reformat it back, and every pull request would carry a whole-file diff.

`tools/localization.py` compares the parsed catalog instead. A `.po` is rewritten only when its entries change, meaning a key was added or removed, an English source moved, or a translation or flag changed. Formatting differences are invisible to both `sync` and `check`, and whoever wrote the file last wins. When a rewrite does happen, the header is carried over, so the translator credit and revision date Weblate records survive it.

`resources/localization/*.json`, `manifest.json` and `mikan.pot` stay byte-compared. They are ours alone and nothing else writes them.

### 2. The tables are regenerated in CI

Weblate edits `localization/<code>.po`. The JSON tables it does not touch are what jsDelivr serves and what CI checks, and hosted Weblate cannot run repo scripts. `.github/workflows/weblate-sync.yml` does the regeneration, over two branches with one writer each:

- `weblate`: written only by Weblate. Its pushes are not forced, so a commit it does not have on this branch would make its next push fail.
- `weblate-sync`: written only by the workflow. It is `weblate` plus one commit that runs `python tools/localization.py sync`, force-pushed on every Weblate push.

The workflow opens the pull request from `weblate-sync` to `main` when none is open. A push made with the workflow token starts no other workflow, so it also dispatches `build-and-test.yml` on `weblate-sync`. That is what makes the checks `main` requires report on the pull request's head commit.

`build-and-test.yml` skips pushes to `weblate`, whose tables are stale by construction.

---

## GitHub setup

- Repository setting `Settings > Actions > General > Allow GitHub Actions to create and approve pull requests`: on. The workflow token cannot open the pull request without it.
- Translation pull requests are merged with **Create a merge commit**, never squashed. Weblate rebases onto `main` after the merge and has to find its own commits there. Weblate's documentation is explicit that a squash merge leaves it in a state that needs a manual reset.

---

## Weblate setup

Create the project inside a workspace at [hosted.weblate.org](https://hosted.weblate.org/). Connect GitHub through the workspace's "Connect GitHub account" flow, which installs the [Hosted Weblate app](https://github.com/apps/hosted-weblate) on the MikanXR organization. Grant it only the repositories being translated. Components imported this way need no separate webhook.

Then one component:

- Source code repository: `MikanXR/MikanXR`
- Repository branch: `main`
- File format: `gettext PO file`
- File mask: `localization/*.po`
- Template for new translations: `localization/mikan.pot`
- Translation license: MIT
- Push branch: `weblate`
- Open a pull request for the translation changes: off. The workflow opens it, from `weblate-sync`.
- Merge style: rebase
- Long line wrapping: no wrapping, which matches what `tools/localization.py` writes and keeps diffs small

Settings that matter for this project:

- **Adding new translation: Contact maintainers.** A new language needs a font and glyph ranges baked into the build, so it is a code change, not a translation ([localization.md](../reference/localization.md)). This setting gives a translator a "message the maintainer" button instead of letting them create a `.po` for a script the build cannot render, and it keeps `localization/languages.json` and the manifest consistent, since Weblate would not update either.

- **Do not enable the msgmerge addon.** `tools/localization.py sync` owns the merge from `mikan.pot` into each catalog. Two mergers with different output would fight.

- **Do not enable "Cleanup translation files"** for the same reason.

- **Do not enable "Squash Git commits".** It rewrites commits Weblate has already pushed, which has the same effect on the rebase as a squash merge.

If the app flow is unavailable, the fallback is the Git integration with an SSH push URL, the `weblate` GitHub user invited as a collaborator with write access, and a webhook to `https://hosted.weblate.org/hooks/github/` on the push event.

---

## Verifying it works

1. Make a trivial edit to one Japanese string in Weblate and clear its "Needs editing" flag.
2. Confirm Weblate pushes to the `weblate` branch, and that the workflow pushes `weblate-sync` with a regenerated `resources/localization/ja.json` and opens a pull request.
3. Confirm the dispatched `build-and-test.yml` run reports on the pull request and satisfies the required checks: `localization-check` passes, `format-check` is untouched, and the Windows job's `run_localization_unit_tests` passes.
4. Confirm `ja.json` shows `reviewedCount` incremented by exactly one, and that the diff of `ja.po` is what a reviewer can read rather than a whole-file reformat.
5. Merge with a merge commit, then confirm Weblate rebases onto `main` without reporting a conflict and that a second edit pushes cleanly.
6. Break it on purpose: put `##` in a translation through the Weblate editor and confirm the `Weblate Sync` run fails naming the key and the pull request is left at its last good state. Weblate's own checks should also flag it before it ever gets that far.

---

## Risks and exits

- A dispatched run satisfying a required check is expected but unproven here. Step 3 is where it is proven. The fallback is pushing `weblate-sync` with a deploy key instead of the workflow token, which makes the push itself trigger CI.
- If `main` gains English strings while a translation pull request is open, the generated tables can conflict. Weblate rebases and pushes again on its next translator edit, and a manual run of `Weblate Sync` rebuilds the branch in between.
- The Hosted Weblate app has write access to the repository. Branch protection on `main` is unaffected, since Weblate never pushes there.
- Backing out is cheap and non-destructive. The PO files live in git and remain the source of truth, so disconnecting the component leaves the repository in a working state with the POEdit workflow intact.

---

## Other repositories

MikanTrack and MikanStudio become further components of the same Weblate project once their own string pipelines produce PO catalogs. Each needs the two repo-side pieces above, and each has to be public and libre licensed to stay on the Libre plan.
