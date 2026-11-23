#!/usr/bin/env python3
"""Localization pipeline for MikanXR.

Translators own the gettext catalogs under localization/. This tool turns them
into the JSON string tables the editor loads and the CDN serves, which live
under resources/localization/ and are never hand-edited.

    python tools/localization.py sync         # regenerate everything
    python tools/localization.py check        # CI gate: fail on drift or errors
    python tools/localization.py stats        # per-language progress
    python tools/localization.py new fr Francais

English is the source language: developers add keys to
resources/localization/en.json and run `sync`, which normalizes that file,
rebuilds localization/mikan.pot, merges it into every .po, and rewrites the
generated tables and manifest. A .po is rewritten only when its entries change,
so the formatting of whichever gettext tool last saved it is left alone.

An entry whose English text changed keeps its translation but is flagged
fuzzy, which POEdit shows as "Needs work". Fuzzy is also how a bulk machine
translation is marked, so `reviewedCount` in a table's _meta block counts only
the strings a human has vouched for.

Requires polib (pip install polib).

Exits nonzero if validation fails or, for `check`, if any generated file on
disk differs from what this tool would write.
"""

import argparse
import bisect
import json
import sys
from collections import OrderedDict
from pathlib import Path

try:
    import polib
except ImportError:
    raise SystemExit("error: polib is required (pip install polib)")

REPO_ROOT = Path(__file__).resolve().parent.parent
SOURCE_DIR = REPO_ROOT / "localization"
TABLE_DIR = REPO_ROOT / "resources" / "localization"
LANGUAGES_FILE = SOURCE_DIR / "languages.json"
TEMPLATE_FILE = SOURCE_DIR / "mikan.pot"
GLYPH_RANGES_FILE = REPO_ROOT / "resources" / "font" / "ui_glyph_ranges.txt"
ENGLISH_CODE = "en"

PRINTF_FLAG_CHARS = "-+ #0"
PRINTF_LENGTH_CHARS = "hljztL"
PRINTF_CONV_CHARS = "diouxXeEfFgGaAcsp"


# -- printf specifiers ----
# Mirrors parsePrintfSpecs in src/Editor/Localization/LocalizationManager.cpp.
# The loader is authoritative; this is the copy that gives a translator an
# answer in seconds instead of at the end of the Windows build job.
def parse_printf_specs(text):
    """Return (ok, specs) for a format string. specs is the ordered conversion
    sequence as normalized tokens ("d", "ld", "s", ...)."""
    specs = []
    index = 0
    length = len(text)

    while index < length:
        if text[index] != "%":
            index += 1
            continue

        index += 1
        if index >= length:
            return False, specs
        if text[index] == "%":
            index += 1
            continue

        while index < length and text[index] in PRINTF_FLAG_CHARS:
            index += 1
        if index < length and text[index] == "*":
            specs.append("d")
            index += 1
        else:
            while index < length and text[index].isdigit():
                index += 1
        if index < length and text[index] == ".":
            index += 1
            if index < length and text[index] == "*":
                specs.append("d")
                index += 1
            else:
                while index < length and text[index].isdigit():
                    index += 1

        length_modifier = ""
        while index < length and text[index] in PRINTF_LENGTH_CHARS:
            length_modifier += text[index]
            index += 1

        if index >= length or text[index] not in PRINTF_CONV_CHARS:
            return False, specs
        specs.append(length_modifier + text[index])
        index += 1

    return True, specs


def has_printf_specs(text):
    ok, specs = parse_printf_specs(text)
    return ok and len(specs) > 0


# -- file io ----
def read_text(path):
    """Read a file with universal newlines, so line endings never reach the
    comparison logic (git checks CRLF out on Windows and LF on CI)."""
    with open(path, "r", encoding="utf-8", newline=None) as handle:
        return handle.read()


def write_text(path, text):
    """Write with the platform's native line endings, matching what git checks
    out beside it."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(text)


def read_json(path):
    return json.loads(read_text(path), object_pairs_hook=OrderedDict)


def dump_json(obj, indent):
    return json.dumps(obj, ensure_ascii=False, indent=indent) + "\n"


# -- language registry ----
def load_languages(problems):
    if not LANGUAGES_FILE.exists():
        problems.append(f"{LANGUAGES_FILE.name}: missing")
        return []

    document = read_json(LANGUAGES_FILE)
    languages = document.get("languages", [])
    codes = set()
    for language in languages:
        code = language.get("code", "")
        if not code:
            problems.append(f"{LANGUAGES_FILE.name}: an entry has no code")
        elif code in codes:
            problems.append(f"{LANGUAGES_FILE.name}: duplicate code '{code}'")
        codes.add(code)
        if not language.get("nativeName", ""):
            problems.append(f"{LANGUAGES_FILE.name}: '{code}' has no nativeName")

    if not languages or languages[0].get("code") != ENGLISH_CODE:
        problems.append(f"{LANGUAGES_FILE.name}: '{ENGLISH_CODE}' must be the first entry")

    return languages


def render_languages(languages):
    return dump_json(OrderedDict([("languages", languages)]), "\t")


# -- the English source table ----
def load_english(problems):
    """resources/localization/en.json -> {section: {key: text}}, _meta dropped."""
    english_path = TABLE_DIR / f"{ENGLISH_CODE}.json"
    if not english_path.exists():
        problems.append(f"{english_path.name}: missing")
        return OrderedDict()

    document = read_json(english_path)
    sections = OrderedDict()
    for section_name, section in document.items():
        if section_name.startswith("_"):
            continue
        if not isinstance(section, dict):
            problems.append(f"{english_path.name}: section '{section_name}' is not an object")
            continue

        entries = OrderedDict()
        for string_key, value in section.items():
            if not isinstance(value, str):
                problems.append(f"{english_path.name}: {section_name}.{string_key} is not a string")
                continue
            entries[string_key] = value
        sections[section_name] = entries

    return sections


def flatten(sections):
    """{section: {key: text}} -> {"section.key": text}"""
    flat = OrderedDict()
    for section_name in sorted(sections):
        for string_key in sorted(sections[section_name]):
            flat[f"{section_name}.{string_key}"] = sections[section_name][string_key]
    return flat


def validate_english(flat_english, problems):
    window_titles = {}
    for key, text in flat_english.items():
        if "##" in text:
            problems.append(f"en: '{key}' contains ## (reserved for ImGui IDs)")
        if not parse_printf_specs(text)[0]:
            problems.append(f"en: '{key}' has a malformed printf specifier")

        # The English text of a windows.* key IS that window's ImGui ID, so an
        # empty or duplicated title would merge two windows
        if not key.startswith("windows."):
            continue
        if not text:
            problems.append(f"en: '{key}' is an empty window title")
        elif text in window_titles:
            problems.append(f"en: duplicate window title '{text}' ({key} vs {window_titles[text]})")
        else:
            window_titles[text] = key


def validate_translation(code, key, english_text, translated, problems):
    if "##" in translated:
        problems.append(f"{code}: '{key}' contains ## (reserved for ImGui IDs)")
        return

    english_ok, english_specs = parse_printf_specs(english_text)
    translated_ok, translated_specs = parse_printf_specs(translated)
    if not translated_ok:
        problems.append(f"{code}: '{key}' has a malformed printf specifier")
    elif english_ok and english_specs != translated_specs:
        problems.append(
            f"{code}: '{key}' printf specifiers do not match en "
            f"({''.join('%' + spec for spec in english_specs) or 'none'} vs "
            f"{''.join('%' + spec for spec in translated_specs) or 'none'})"
        )


# -- glyph coverage ----
# Mirrors isTextRenderableWithUiGlyphs in src/Libraries/MikanGUI/Private/MkGuiTheme.cpp
# over the ranges that file's formatUiGlyphRanges wrote out. The font atlas is
# baked into the build, so a codepoint outside these ranges draws as an empty
# box however correct the translation is.
def load_glyph_ranges(problems):
    """Return the baked ranges as parallel sorted lists (firsts, lasts)."""
    if not GLYPH_RANGES_FILE.exists():
        problems.append(f"{GLYPH_RANGES_FILE.name}: missing (write it with `MikanCmd -writeUiGlyphRanges=<path>`)")
        return None

    firsts = []
    lasts = []
    for line in read_text(GLYPH_RANGES_FILE).splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        first, _, last = line.partition("-")
        firsts.append(int(first, 16))
        lasts.append(int(last, 16))
    return firsts, lasts


def find_unrenderable_codepoint(text, glyph_ranges):
    firsts, lasts = glyph_ranges
    for character in text:
        codepoint = ord(character)
        if codepoint < 0x20:
            continue  # control characters (a line break) are not glyphs
        index = bisect.bisect_right(firsts, codepoint) - 1
        if index < 0 or codepoint > lasts[index]:
            return codepoint
    return None


def validate_glyphs(code, key, text, glyph_ranges, problems):
    if glyph_ranges is None:
        return
    codepoint = find_unrenderable_codepoint(text, glyph_ranges)
    if codepoint is not None:
        problems.append(
            f"{code}: '{key}' uses {chr(codepoint)} (U+{codepoint:04X}), which is outside the baked glyph ranges"
        )


# -- catalogs ----
def make_catalog(code, metadata=None):
    """An empty catalog. A header another tool wrote is carried over whole, so
    the translator credit and revision date it recorded survive a rewrite."""
    catalog = polib.POFile(wrapwidth=0)
    catalog.metadata = OrderedDict(
        metadata
        or [
            ("Project-Id-Version", "MikanXR"),
            ("Language", code),
            ("MIME-Version", "1.0"),
            ("Content-Type", "text/plain; charset=UTF-8"),
            ("Content-Transfer-Encoding", "8bit"),
        ]
    )
    return catalog


def build_template(flat_english):
    template = make_catalog("")
    for key, text in flat_english.items():
        template.append(
            polib.POEntry(
                msgctxt=key,
                msgid=text,
                msgstr="",
                comment=f"Section: {key.split('.', 1)[0]}",
                flags=["c-format"] if has_printf_specs(text) else [],
            )
        )
    return template


def order_flags(flags, is_c_format):
    """fuzzy first (the gettext convention), then c-format, then the rest."""
    ordered = []
    if "fuzzy" in flags:
        ordered.append("fuzzy")
    if is_c_format:
        ordered.append("c-format")
    ordered.extend(sorted(flag for flag in flags if flag not in ("fuzzy", "c-format")))
    return ordered


def load_catalog(path):
    if not path.exists():
        return None
    return polib.pofile(str(path), wrapwidth=0)


def merge_catalog(code, flat_english, previous):
    """Rebuild a language's catalog against the current English source.

    A translation survives an English edit but is flagged fuzzy with the old
    source recorded as #| msgid. A key English no longer defines becomes an
    obsolete #~ entry rather than being dropped, so the work comes back with
    the key."""
    existing = {}
    if previous is not None:
        for entry in previous:
            if entry.msgctxt:
                existing[entry.msgctxt] = entry

    catalog = make_catalog(code, previous.metadata if previous is not None else None)
    for key, english_text in flat_english.items():
        is_c_format = has_printf_specs(english_text)
        entry = polib.POEntry(
            msgctxt=key,
            msgid=english_text,
            msgstr="",
            comment=f"Section: {key.split('.', 1)[0]}",
        )

        old = existing.pop(key, None)
        flags = set()
        if old is not None:
            entry.tcomment = old.tcomment
            if old.msgstr:
                entry.msgstr = old.msgstr
                flags = set(old.flags)
                if old.msgid != english_text:
                    flags.add("fuzzy")
                    entry.previous_msgid = old.msgid
                elif "fuzzy" in flags:
                    # Still unreviewed since an earlier English edit: the old
                    # source stays until a person clears the flag.
                    entry.previous_msgid = old.previous_msgid
        entry.flags = order_flags(flags, is_c_format)
        catalog.append(entry)

    # Leftovers English dropped. An untranslated one carries no work worth
    # keeping around.
    for key in sorted(existing):
        old = existing[key]
        if not old.msgstr:
            continue
        old.obsolete = True
        old.previous_msgid = None
        catalog.append(old)

    return catalog


def render_catalog(catalog):
    return str(catalog)


def catalog_content(catalog):
    """What a catalog says, apart from how it is written: each key's source
    text, translation, flags and previous source. Wrapping, header fields,
    comments and entry order are left out, so a catalog saved by another gettext
    tool reads the same as one written here."""
    return {
        (bool(entry.obsolete), entry.msgctxt): (
            entry.msgid,
            entry.msgstr,
            frozenset(entry.flags),
            entry.previous_msgid or "",
        )
        for entry in catalog
        if entry.msgctxt
    }


# -- generated tables ----
def render_table(code, native_name, sections, translations):
    """One resources/localization/<code>.json.

    Untranslated keys carry the English text rather than being omitted: every
    table defines every key, which is what keeps the loader's missing-key
    warning meaningful for a half-finished language."""
    string_count = 0
    translated_count = 0
    reviewed_count = 0

    body = OrderedDict()
    for section_name in sorted(sections):
        entries = OrderedDict()
        for string_key in sorted(sections[section_name]):
            english_text = sections[section_name][string_key]
            translation = translations.get(f"{section_name}.{string_key}")

            string_count += 1
            if translation is None:
                entries[string_key] = english_text
            else:
                entries[string_key] = translation[0]
                translated_count += 1
                if not translation[1]:
                    reviewed_count += 1

        body[section_name] = entries

    document = OrderedDict()
    document["_meta"] = OrderedDict(
        [
            ("code", code),
            ("nativeName", native_name),
            ("stringCount", string_count),
            ("translatedCount", translated_count),
            ("reviewedCount", reviewed_count),
        ]
    )
    document.update(body)

    return dump_json(document, "\t"), (string_count, translated_count, reviewed_count)


def render_manifest(languages):
    files = [f"{language['code']}.json" for language in languages]
    return dump_json(OrderedDict([("files", files)]), "  ")


# -- the pipeline ----
def build_outputs():
    """Generate every output file in memory.

    Returns (outputs, problems, counts): path -> text, a list of validation
    failures, and code -> (string, translated, reviewed) counts."""
    problems = []
    outputs = OrderedDict()
    counts = OrderedDict()

    languages = load_languages(problems)
    sections = load_english(problems)
    if not sections:
        return outputs, problems, counts

    flat_english = flatten(sections)
    validate_english(flat_english, problems)
    glyph_ranges = load_glyph_ranges(problems)

    outputs[TEMPLATE_FILE] = render_catalog(build_template(flat_english))

    for language in languages:
        code = language.get("code", "")
        native_name = language.get("nativeName", code)
        if not code:
            continue

        if code == ENGLISH_CODE:
            translations = {key: (text, False) for key, text in flat_english.items()}
        else:
            catalog_path = SOURCE_DIR / f"{code}.po"
            previous = load_catalog(catalog_path)
            if previous is None:
                problems.append(f"{catalog_path.name}: missing (run `localization.py new {code} ...`)")
                continue

            # A catalog is rewritten only when the merge changed what it says.
            # Translators save these files from POEdit and from the web editor,
            # each with its own formatter, and none of them should be turned
            # back into this tool's layout for a difference no reader can see.
            # Taken before the merge, which reuses the entries it retires.
            previous_content = catalog_content(previous)
            catalog = merge_catalog(code, flat_english, previous)
            if catalog_content(catalog) != previous_content:
                outputs[catalog_path] = render_catalog(catalog)
            else:
                outputs[catalog_path] = read_text(catalog_path)

            translations = {}
            for entry in catalog:
                if entry.obsolete or not entry.msgstr:
                    continue
                validate_translation(code, entry.msgctxt, entry.msgid, entry.msgstr, problems)
                translations[entry.msgctxt] = (entry.msgstr, "fuzzy" in entry.flags)

        # The native name is drawn in the language picker, so it has to render too
        validate_glyphs(code, "_meta.nativeName", native_name, glyph_ranges, problems)
        for key, (translated, _) in translations.items():
            validate_glyphs(code, key, translated, glyph_ranges, problems)

        text, language_counts = render_table(code, native_name, sections, translations)
        outputs[TABLE_DIR / f"{code}.json"] = text
        counts[code] = (native_name,) + language_counts

    outputs[TABLE_DIR / "manifest.json"] = render_manifest(languages)

    return outputs, problems, counts


def find_drift(outputs):
    drifted = []
    for path, text in outputs.items():
        if not path.exists() or read_text(path) != text:
            drifted.append(path)
    return drifted


def report(problems, drifted, fix_hint):
    for problem in problems:
        print(f"error: {problem}")
    for path in drifted:
        print(f"stale: {path.relative_to(REPO_ROOT).as_posix()}")
    if drifted and fix_hint:
        print("run `python tools/localization.py sync` and commit the result")
    return 1 if (problems or drifted) else 0


# -- commands ----
def command_sync(args):
    outputs, problems, _ = build_outputs()
    if problems:
        return report(problems, [], False)

    drifted = find_drift(outputs)
    for path in drifted:
        write_text(path, outputs[path])
        print(f"wrote {path.relative_to(REPO_ROOT).as_posix()}")
    if not drifted:
        print("everything already up to date")
    return 0


def command_check(args):
    outputs, problems, _ = build_outputs()
    drifted = find_drift(outputs)
    exit_code = report(problems, drifted, True)
    if exit_code == 0:
        print(f"{len(outputs)} generated file(s) up to date, no validation errors")
    return exit_code


def command_stats(args):
    _, problems, counts = build_outputs()
    if problems:
        return report(problems, [], False)

    print(f"{'code':<6} {'language':<16} {'translated':>16} {'reviewed':>16}")
    for code, (native_name, string_count, translated_count, reviewed_count) in counts.items():
        translated_pct = 100.0 * translated_count / string_count if string_count else 0.0
        reviewed_pct = 100.0 * reviewed_count / string_count if string_count else 0.0
        print(
            f"{code:<6} {native_name:<16} "
            f"{translated_count:>6}/{string_count} ({translated_pct:5.1f}%) "
            f"{reviewed_count:>6}/{string_count} ({reviewed_pct:5.1f}%)"
        )
    return 0


def command_new(args):
    problems = []
    languages = load_languages(problems)
    sections = load_english(problems)
    if problems:
        return report(problems, [], False)

    code = args.code
    if any(language.get("code") == code for language in languages):
        raise SystemExit(f"error: language '{code}' is already registered")
    if code == ENGLISH_CODE:
        raise SystemExit("error: English is the source language, not a translation")

    catalog_path = SOURCE_DIR / f"{code}.po"
    if catalog_path.exists():
        raise SystemExit(f"error: {catalog_path.relative_to(REPO_ROOT).as_posix()} already exists")

    seeded = {}
    if args.from_json:
        document = read_json(Path(args.from_json))
        for section_name, section in document.items():
            if section_name.startswith("_") or not isinstance(section, dict):
                continue
            for string_key, value in section.items():
                if isinstance(value, str):
                    seeded[f"{section_name}.{string_key}"] = value

    flat_english = flatten(sections)
    catalog = make_catalog(code)
    seeded_count = 0
    for key, english_text in flat_english.items():
        translation = seeded.get(key, "")
        flags = set()
        if translation:
            seeded_count += 1
            # A bulk import is machine output until someone reads it entry by
            # entry, which is exactly what the fuzzy flag says
            if not args.reviewed:
                flags.add("fuzzy")
        catalog.append(
            polib.POEntry(
                msgctxt=key,
                msgid=english_text,
                msgstr=translation,
                comment=f"Section: {key.split('.', 1)[0]}",
                flags=order_flags(flags, has_printf_specs(english_text)),
            )
        )

    write_text(catalog_path, render_catalog(catalog))
    print(f"wrote {catalog_path.relative_to(REPO_ROOT).as_posix()} ({seeded_count}/{len(flat_english)} seeded)")

    languages.append(OrderedDict([("code", code), ("nativeName", args.native_name)]))
    write_text(LANGUAGES_FILE, render_languages(languages))
    print(f"wrote {LANGUAGES_FILE.relative_to(REPO_ROOT).as_posix()}")

    return command_sync(args)


def main():
    # Native language names are printed, and the Windows console defaults to a
    # codepage that cannot encode them
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")

    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    subparsers.add_parser("sync", help="regenerate the catalogs and the JSON tables").set_defaults(
        func=command_sync
    )
    subparsers.add_parser("check", help="fail on stale generated files or validation errors").set_defaults(
        func=command_check
    )
    subparsers.add_parser("stats", help="print per-language translated and reviewed counts").set_defaults(
        func=command_stats
    )

    new_parser = subparsers.add_parser("new", help="bootstrap a new language")
    new_parser.add_argument("code", help='language code and filename stem ("fr")')
    new_parser.add_argument("native_name", help='display name in that language ("Francais")')
    new_parser.add_argument(
        "--from-json", help="seed the catalog from an existing JSON table instead of starting empty"
    )
    new_parser.add_argument(
        "--reviewed",
        action="store_true",
        help="treat seeded strings as human reviewed instead of flagging them fuzzy",
    )
    new_parser.set_defaults(func=command_new)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
