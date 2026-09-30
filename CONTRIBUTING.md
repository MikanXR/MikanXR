# Contributing to MikanXR

## Translations

Correcting the machine-generated UI text is the easiest way to contribute and needs no build
and no C++. See [docs/TRANSLATING.md](docs/TRANSLATING.md).

## Code formatting

C++ source under `src/` is formatted with [clang-format](https://clang.llvm.org/docs/ClangFormat.html).
The style is defined by [`.clang-format`](.clang-format) at the repo root and is enforced
by CI on every push and pull request.

### Tool version

Use **clang-format 19.1.x** to match what CI uses. clang-format output changes between
major versions, so a different version can reformat files in ways CI then rejects.

- **`InitialSetup_x64.bat`** puts a pinned 19.1.5 in `deps\clang-format`, which the
  format targets use first.
- **Visual Studio 2022** also bundles a compatible copy at
  `VC\Tools\Llvm\bin\clang-format.exe`. Visual Studio 2026 bundles 22.x, which is not
  compatible.
- Otherwise install the pinned wheel on your `PATH`: `pip install clang-format==19.1.5`
  (or `pipx install clang-format==19.1.5`).

The format targets pick the first 19.x among those, and warn when they can only find
another version.

### Fixing formatting

After configuring the project, reformat all sources in place with:

```sh
cmake --build build --target FormatFix
```

To check formatting without modifying anything (this is what CI runs):

```sh
cmake --build build --target FormatCheck
```

In Visual Studio these appear as the `FormatFix` and `FormatCheck` projects under
the `CMakePredefinedTargets` solution folder; right-click → Build to run them.

Both targets just wrap [`cmake/RunClangFormat.cmake`](cmake/RunClangFormat.cmake), which you
can also run directly without a configured build tree:

```sh
cmake -P cmake/RunClangFormat.cmake -- --fix     # reformat in place
cmake -P cmake/RunClangFormat.cmake -- --check   # verify only
```

Only files under `src/` are formatted; `thirdparty/` is left untouched.

### git blame

The one-time repository-wide reformat commit is listed in
[`.git-blame-ignore-revs`](.git-blame-ignore-revs) so it doesn't pollute `git blame`.
GitHub honors this automatically. To benefit locally, run once:

```sh
git config blame.ignoreRevsFile .git-blame-ignore-revs
```
