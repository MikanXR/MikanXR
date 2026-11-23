# Translating MikanXR

Every translation in MikanXR started out machine generated. If you speak one of the supported languages, correcting those strings is one of the easiest ways to help, and you do not need to build anything or write any code.

There are two ways to contribute: in the browser with Weblate, or offline with a gettext editor and a pull request. Both end up in the same files. Once merged, translations reach people who already have MikanXR installed within a day, without them downloading a new build.

---

## Translating in the browser with Weblate

MikanXR uses [Weblate](https://weblate.org/) for translations. The project is at [hosted.weblate.org/projects/mikanxr](https://hosted.weblate.org/projects/mikanxr/). This route needs no fork, no clone and no pull request.

1. Sign in to Hosted Weblate. A GitHub account works.
2. Open the MikanXR project and pick your language.
3. Work through the strings marked **"Needs editing"**. That flag means the string is machine generated, or the English text changed after it was translated.
4. Correct the translation, leave the "Needs editing" box unticked, and save.

Saving with the box unticked is how you vouch for a string. It is what the app uses to tell users how much of their language has been reviewed.

Weblate sends your saved strings to the repository on its own, and a pull request is opened for you. There is nothing else to do.

The [three rules](#three-rules) below apply here too, and Weblate warns you when a placeholder does not match.

The rest of this page describes the offline route.

---

## What you edit

One file: `localization/ja.po` (or whichever language you are working on). These are standard gettext catalogs.

**Do not edit anything under `resources/localization/`.** Those JSON files are generated from the catalogs, and a pull request that edits them by hand will fail its checks.

---

## Getting set up

1. Install [POEdit](https://poedit.net/). It is free, runs on Windows, macOS and Linux, and is what most of this ecosystem uses. Any gettext editor works, including Lokalize and Gtranslator.
2. Fork the repository and clone your fork.
3. Open `localization/<language>.po` in POEdit.

---

## Working through the file

POEdit shows the English text on the left and the translation on the right. Strings marked **"Needs work"** (orange) are the ones to look at: that flag means the string is machine generated, or the English text changed after it was translated.

When you are happy with a translation, clear the "Needs work" flag. That flag is the only record of whether a person has vouched for a string, and it is what the app uses to tell users how much of their language has been reviewed. A string with the flag cleared counts as human reviewed.

If the English source changed under an existing translation, POEdit shows you the previous English text so you can see what moved.

Save the file, commit it, and open a pull request. That is the whole contribution.

---

## Three rules

**Keep the `%s` and `%d` placeholders.** They are where the app inserts a number or a name at runtime. They must appear in the same order and the same kinds as the English text. POEdit will warn you if they do not match. `%%` means a literal percent sign and stays as `%%`.

**Never write `##` in a translation.** Two hash characters in a row are how the UI toolkit separates a label from its internal identifier, so a translation containing them breaks the widget.

**A `\n` is a line break.** Leave it where it makes sense in your language.

Anything else is yours to phrase naturally. Short button labels should stay short, since the UI has fixed-width buttons in places, but there is no hard character limit.

---

## Finding which string is which

If you spot a bad string while using the app and want to know which catalog entry it came from, press **F9** (or `View > Show String Keys`). Every piece of text turns into its key, in place. Press F9 again to switch back. Paste that key into the search box in Weblate or POEdit and you are on the right entry.

---

## Checking your work before you push

Optional, but it catches mistakes faster than waiting on CI:

```bash
pip install polib
python tools/localization.py check
```

It prints the key of anything wrong. To see how far along each language is:

```bash
python tools/localization.py stats
```

---

## Adding a language that does not exist yet

Open an issue first. A new language usually needs a font that covers its script bundled into the app, which is a code change rather than a translation, and filling in a catalog whose characters cannot render yet is wasted effort. Say which language you want and it can be set up for you.

---

## Questions

Open an issue. Partial translations are welcome: anything you leave untranslated falls back to English, so there is no need to finish a language in one pass.
