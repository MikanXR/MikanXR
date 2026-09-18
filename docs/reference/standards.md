# Standards

The coding conventions observed in `src/` of this repo. There is no written style guide; everything here is derived empirically from the existing code and from `.clang-format`. Module boundaries are described in [modules.md](./modules.md), build targets in [build.md](./build.md), and API/wire rules in [wire-protocol.md](./wire-protocol.md).

---

## Formatting authority

`.clang-format` at the repo root is the single formatting authority, and only `src/` is formatted (`thirdparty/` is untouched). Use clang-format 19.1.x to match CI; other major versions reformat differently and CI rejects the result. Run via the CMake wrappers `FormatFix` / `FormatCheck` (defined in `cmake/ClangFormat.cmake`), or standalone `cmake -P cmake/RunClangFormat.cmake -- --check`.

Key settings (from `.clang-format`, `BasedOnStyle: Microsoft`):

- `ColumnLimit: 120`.

- Tabs for indentation (`UseTab: ForContinuationAndIndentation`, `TabWidth: 4`, `IndentWidth: 4`).

- Allman braces (`BreakBeforeBraces: Allman`); short functions may stay on one line.

- `SpaceBeforeAssignmentOperators: false`: assignments are written `value= expr;` and initializers `int x= 0;`. This is the codebase's most distinctive tic.

- `PointerAlignment: Left` / `ReferenceAlignment: Left` (`Type* ptr`, `const Foo& ref`).

- Constructor initializers break before the comma (`BreakConstructorInitializers: BeforeComma`).

- `SortIncludes: false`: include order is authored by hand and preserved.

- Refureku macros (`STRUCT()`, `ENUM()`, `FIELD()`, ...) are declared as `AttributeMacros`, and the `MIKAN_*_FUNC(T)` export macros as `TypenameMacros`, so clang-format parses them correctly.

---

## Files and headers

- Header/source pairing: `Foo.h` + `Foo.cpp`, one primary class per pair. Libraries and plugins split `Public/` (installed, exported API) from `Private/` (implementation); the Editor uses flat per-domain folders instead.

- `#pragma once` in headers (609 of 612 headers in `src/`; no `#ifndef` guards except generated/vendored files such as `AppCore/Version.h`).

- Observed include order in a `.cpp`: the file's own header first, then project headers, then third-party headers, then standard library. Not machine-enforced (`SortIncludes: false`) and not perfectly consistent; glm sometimes appears after `<memory>`/`<string>`.

- Forward-declaration headers named `*Fwd.h` (`ObjectSystemFwd.h`, `ComponentFwd.h`, `MkWindowFwd.h`, `ScriptingFwd.h`, `OpenCVFwd.h`) hold class forward declarations plus their smart-pointer aliases; headers prefer including a `Fwd` header or an inline `class Foo` forward over the full header. Inline `class Foo*` member/return declarations are also common (see `MikanServer.h`).

- Older files carry section banner comments like `//-- includes -----` and `//-- definitions -----` (e.g. `MikanServer.h`); newer files often omit them.

- Each DLL has an export header (`MikanAPIExport.h`, `MkRendererExport.h`, ...) defining `MIKAN_<NAME>_EXPORTS`-gated macros; exported free functions are declared with `MIKAN_<NAME>_FUNC(ReturnType)`.

---

## Naming

- Types: `PascalCase` (`MikanServer`, `AnchorComponent`, `CalibrationPatternFinder_Aruco`, where an underscore suffix marks a variant of a base class).

- Interfaces: `I` prefix (`IMikanModule`, `IServerRequestHandler`, `IVideoDevice`). The renderer/window libraries additionally use an `Mk` brand prefix (`IMkTexture`, `MkMaterial`, `IMkWindowContext`); client-facing API types use a `Mikan` prefix (`MikanAnchorComponentValues`).

- Methods and free functions: `camelCase` (`getInstance`, `publishMikanJsonEvent`, `createUsbVideoDeviceManager`). Lifecycle verbs `startup`/`shutdown`/`init`/`dispose`/`update` recur across classes.

- Member variables: `m_` prefix (`m_ownerWindow`, `m_clientConnections`). Boolean members commonly add a `b`: `m_bIsInitialized`.

- Locals and parameters: `camelCase`; boolean locals often `b`-prefixed (`bSuccess`, `bFoundChessboard`); output parameters prefixed `out` (`outDescriptors`, `outClientList`).

- Constants and static class data: `k_` prefix (`k_componentClassName`, `k_VertexSemanticNames`), typically `inline static const` or `static const char*`.

- Enums: `enum class`, frequently with a lowercase `e` prefix on the type name (`eVertexSemantic`, `eWindowAPI`, `eMkBlendEquation`); unprefixed names also occur (`SharedTextureType`). Sentinels `INVALID= -1` and `COUNT` are common; enumerator casing is otherwise mixed (lowercase in renderer enums, PascalCase elsewhere). Wire-protocol enums in `MikanClientAPI` are declared through the Refureku `ENUM()` macro.

- Template parameters: lowercase `t_` snake case (`t_value_type`, `t_module_type`, `t_derived_class`). A few type aliases use the same style (`t_opengl_point3d_list` in `Editor/Math/CameraMath.h`).

---

## Pointer idioms

- Ownership is `std::shared_ptr` almost everywhere, spelled through `using` aliases: `using FooPtr= std::shared_ptr<Foo>;` with companions `FooConstPtr` (`shared_ptr<const Foo>`) and `FooWeakPtr` (`weak_ptr<Foo>`). Aliases live next to the class or in the module's `*Fwd.h`, often with an inline forward declaration: `using IMikanAPIPtr= std::shared_ptr<class IMikanAPI>;`.

- Back-references and non-owning captures use the `WeakPtr` alias (`MikanObjectWeakPtr owner`, `SelectionComponentWeakPtr m_selectionComponent`).

- Downcasts use `std::static_pointer_cast` on shared pointers.

- Raw pointers appear for non-owning wiring inside a single process (e.g. `MikanServer`'s handler members) and at C boundaries: the plugin ABI is raw `new`/`delete` behind the exported `AllocatePluginModule`/`FreePluginModule` functions, and singletons expose `static Foo* getInstance()`.

---

## Codebase-specific rules

- Avoid method names that collide with Win32 macros (`GetObject`, `SendMessage`, `CreateWindow`, ...). The default build is a unity build (`CMAKE_UNITY_BUILD=ON`), so a transitively included `windows.h` in an unrelated concatenated TU rewrites the name (`GetObject` becomes `GetObjectA`) and produces an `LNK2019` far from the real conflict. See [build.md](./build.md). Files with genuine include-order problems are opted out per-file via `SKIP_UNITY_BUILD_INCLUSION`.

- `Serialization::String` stays `const char*`-only at the `MikanSerialization` interface (cross-CRT safety across the DLL/EXE boundary); do not widen it to `std::string` there, and watch the `const char*` to `bool` implicit-conversion trap when adding `to_binary` overloads. See [wire-protocol.md](./wire-protocol.md).

- Do not hand-edit generated code: `bindings/*/Generated`, `bindings/typescript/types`, and the `*.rfkh.h`/`*.rfks.h` Refureku output are regenerated by the build and by `MikanClientCodeGen`.

- Never compile a GObject/GstMeta-registering `.cpp` into both a plugin DLL and an executable that loads it; GLib's type registry is process-global and the duplicate registration fails at runtime (documented in `src/Programs/Tests/UnitTests/CMakeLists.txt`, which is where the case arises).

- Client-facing properties must stay consistent across the values struct, descriptor, and `getPropertyValue` implementation; the schema-guard test in `MikanCmd -runTests` enforces this (see [wire-protocol.md](./wire-protocol.md)).

- User-facing UI text carries no string literals. Every displayed string goes through `LocText.h` against a key in `resources/localization/en.json`. Keys are flat `section.key`: the section names the UI unit that owns the string (`mainMenu`, `projectSettings`, `monoLensCalibration`, `nodeEditor`, `windows`), the key is lowerCamel, and a key whose text carries printf specifiers ends in `Fmt`. Pick the helper by what the string is used for.

	- `locText` for display text, tooltips, and format strings
	- `locLabel` for an interactive widget's label, so the ImGui ID is the key rather than the translation
	- `locWindowTitle` for a window or popup title, so the ImGui ID is the English title and layouts survive a language switch
	- `locFormat` to expand a `...Fmt` key into a `std::string`

	Adding a key means editing `resources/localization/en.json` and then running `python tools/localization.py sync`, which folds it into every gettext catalog and regenerates the other tables. Only English is hand-authored; the rest are generated and CI rejects a stale one. See [localization.md](./localization.md).

	Log output, config keys, automation command names, node and pin type names, and device names are not user-facing text and stay untranslated. The `localization` module in `MikanCmd -runTests` enforces key parity against English, printf specifier parity, window-title uniqueness, and that every codepoint is inside the baked font glyph ranges.

- Pixel sizes written into UI code are authored against a 96 DPI display and must be multiplied by `MkGui::getUiScale()` where they are used: button and image sizes, fixed panel widths, cursor offsets, and any width handed to `SetNextItemWidth`. Font sizes are the exception and take no multiply, since ImGui applies the same scale to every font size, including the ones `MkGuiScopedFont` pushes. The scale combines the display's content scale with the user's preference and changes while the app runs, so it is read at the point of use rather than cached. A window's own creation size is authored the same way and is scaled by `EditorWindow::startupWindow` through `IMkWindowContextManager::getPrimaryDisplayContentScale`, since there is no ImGui context yet at that point.

- Labels the entity panels generate from the property and function databases are keyed by descriptor id rather than written at a call site, so those keys mirror the code identifier exactly (`properties.tracking_mount_id`, `functions.align_camera`) instead of following the lowerCamel rule above. A label resolves through `locResolveDescriptorKey`, which prefers a per-class override `<EntityClassName>.<descriptorId>` and otherwise takes the shared section, so a name only needs a class-specific entry when it means something different in that class. `EnumPropertyMetaData` holds keys in `propertyValues`, never display text, and never one of the `g_*` enum tables that are the JSON persistence spellings. The same test module fails the build when a descriptor that always draws a generic widget has no label.
