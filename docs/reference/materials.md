# Materials

How a compositor layer or shape material is described, authored, compiled, and consumed. The compositing pipeline that draws with these materials is in [compositor.md](./compositor.md), the shape graph in [scripting.md](./scripting.md), the node graph machinery both editors share in the same document, and the renderer interfaces in [modules.md](./modules.md).

---

## The material file

A material is a Configuru JSON file loaded by `MikanShaderCache::loadMaterialAssetReference` (`src/Editor/Renderer/MikanShaderCache.cpp`) through `MikanShaderConfig` (`src/Editor/Renderer/MikanShaderConfig.h`). Each domain has its own extension and top-level folder (`MaterialDomainUtils::materialExtension` / `materialFolderName`): a compositor material is `<name>.compmat` under `compositor_materials/<name>/`, a shape material `<name>.shapemat` under `shape_materials/<name>/`, one folder per material holding the material file, its `.matgraph`, and its compiled shaders. The kinds are not interchangeable, so the extension is what file dialogs, the Assets panel, and the drop targets filter on, and each graph kind registers only its own domain's asset reference (`CompositorMaterialAssetReference` / `ShapeMaterialAssetReference`, both over the shared `MaterialAssetReference` behavior). The bundled materials sit under `resources/compositor_materials/` and `resources/shape_materials/`, and `PathUtils::resolveProjectResource` looks in the project before the bundled resources, so a project can shadow a bundled material by path. The project Assets panel lists each domain's folder as one tile per material, the bundled ones read-only, imports a material by copying its folder into the domain's folder (refused when the folder exists or the file's recorded domain disagrees with the folder), and deletes by removing the folder once nothing else references the material. The shader cache keys programs by the material file's path, so same-named materials in different folders never share a program. Saving a bundled material from the material editor goes through Save As into the project's matching folder, where the copy shadows the bundled one, unless the "Edit bundled resources" setting is on ([objects.md](./objects.md)).

Keys:

- `materialName`: informational. The cache keys the program by the file path.
- `vertexShaderPath`, `fragmentShaderPath`: relative to the material file's folder. A path may leave the folder (`../shared/quad.vert`) when several hand-written materials want one vertex shader.
- `vertexAttributes`: an ordered list of `{name, dataType, semantic}`. The order is the `layout(location = N)` order the shader must declare, and the vertex definition built from it is what `IMkVertexDefinition::isCompatibleProgram` checks after linking.
- `uniformSemanticMap`: uniform name to semantic name, the strings of `getUniformSemanticName` in `MkShaderConstants.cpp` (note `normalMatrix` spells as `inverseModelMatrix`). The semantic decides the data type, and therefore the pin type a consuming node creates for the uniform. Consumers bind by uniform name, so two uniforms may share a semantic.
- `domain`, `vertexPreset`, `sourceGraphPath`: optional, written by the material graph compiler. A hand-authored material file omits them. `sourceGraphPath` is relative to the material file's folder like the shader paths, and is what lets the editors reopen the material in the graph editor.
- `uniformDefaults`: optional, one entry per uniform with a default: a number or float array for a float uniform, a texture path in stored project form (forward slashes, relative to the project when the file sits under it) for a sampler. The loader writes them into the `MkMaterial` default tables, so a consumer that binds the material without setting that uniform gets the default. A hand-authored material file may carry them too.

The uniform semantics a hand-written shader can use are the `eUniformSemantic` values. Five generic ones exist for material parameters, `floatParam`, `float2Param`, `float3Param`, `float4Param`, and `textureParam`; they carry no meaning beyond their type, so a parameter of any name can use them.

---

## Domains and vertex presets

A material belongs to one domain (`eMaterialDomain`, `src/Editor/NodeEditors/MaterialCompiler/MaterialDomain.h`):

- `compositor`: shades the fullscreen layer quad drawn by `DrawLayerNode` and `ApplyMaterialNode`
- `shape`: shades shape renderables drawn by `DrawShapeMeshNode`, under a model-view-projection the node feeds through the `modelViewProjectionMatrix` semantic

Within a domain the material is compiled against one vertex preset (`eMaterialVertexPreset`), a named attribute list that matches exactly one family of meshes:

- `P2T`: `aPos vec2`, `aTexCoords vec2`, the compositor layer quad. The only compositor preset.
- `PT`: `aPos vec3`, `aTexCoords vec2`, quad and box shapes
- `PNT`: `aPos vec3`, `aNormal vec3`, `aTexCoords vec2`, model shapes

The preset decides which vertex input nodes a graph may use and which mesh the preview draws. A material file without a `domain` is inferred from its position attribute: a `vec2` position is compositor, a `vec3` is shape (`MaterialDomainUtils::inferDomain`). `GraphMaterialProperty::getDomain` exposes the result, and the consuming nodes refuse a link from a material of the wrong domain (`Node::editorCanAcceptProperty`, consulted by `PropertyPin::canPinsBeConnected`). A shape material whose preset does not match a renderable's mesh is reported as an evaluation error by `DrawShapeMeshNode` rather than drawn.

---

## Material graphs

`MaterialNodeGraph` (`src/Editor/NodeEditors/Graphs/MaterialNodeGraph.h`) is the third `NodeGraph` type, registered in `App::startup` beside the compositor and shape graphs and saved as a `.matgraph` file beside the `.mat` it compiles to. It persists its domain and preset in the graph config's `settings` object. It is a pure data graph with no flow pins and exactly one `MaterialOutputNode`, created by `MaterialNodeGraphFactory::initialCreateMaterialGraph` and undeletable. The output node's input pins are the material's outputs: `color` (float4) in every domain, plus `positionOffset` (float3, object space) in the shape domain. Changing the domain rebuilds those pins and resets the preset to the domain's default.

The graph is never evaluated at render time. Every node derives from `ShaderNode` (`Nodes/Material/ShaderNode.h`) and implements `compileNode(MaterialCompileContext&)` instead of `evaluateNode`. Links carry `ShaderValuePin` values (`Pins/ShaderValuePin.h`), the one pin class of the graph: a declared type (`eShaderValueType`: `float`, `float2`, `float3`, `float4`, `texture2D`, or `wildcard`), a resolved type the last compile inferred and which only colors the pin, and a four-float default an unconnected input compiles to. A link is accepted when the types are equal, either side is a wildcard, or the source is a `float` feeding a vector (broadcast). Math nodes declare wildcard pins, so a type error surfaces at compile time on the node rather than at link time.

The node set, in `Nodes/Material/`, with the create menu category each files under:

- Constants: `ShaderConstantNode` (float, float2, float3, float4, color)
- Parameters: `ShaderParameterNode` (float, float2, float3, float4, color, and the `time` float), `ShaderTextureParameterNode`. A parameter compiles to a uniform named after it with the generic parameter semantic of its type, and its default is baked into the material file as a `uniformDefaults` entry as well as feeding the preview. A parameter name must be a C identifier, and one name cannot carry two types. Parameter nodes sharing a name are one parameter: a second node with the same name is the way to use a parameter in two places without a long wire, renaming a node onto an existing name adopts that parameter's default (and is refused when the types differ), editing the default on any of them writes it to all, and a file whose copies still disagree fails to compile rather than picking one. Dropping a texture from the Graph Assets panel onto the canvas creates a texture parameter with that asset as its default, and dropping one on a texture parameter's default row in the Details panel replaces it; the material graph has no graph variables and no texture node, since a material never knows where its inputs come from at runtime.
- Inputs: `ShaderVertexInputNode` (position, normal, texCoord, color, gated by the preset), `ShaderSemanticInputNode` (screenSize, screenPosition, cameraPosition, zNear, zFar, each a uniform of the existing semantic)
- Texture: `ShaderTextureSampleNode`, `ShaderTextureSizeNode`. An unconnected `uv` samples at the preset's texture coordinate.
- Math: `ShaderMathNode`, table driven by `MaterialCompiler/ShaderMathOpTable.cpp`. Adding an op is one table row plus a `nodes.math<Op>Title` localization key. `ShaderIfNode` (one create menu entry per comparison) selects between two values by comparing two others; ordering comparisons take scalars, equality also takes vectors of one type.
- Vector: `ShaderSwizzleNode`, `ShaderAppendNode`
- Utility: `ShaderRerouteDeclarationNode` and `ShaderRerouteUsageNode`, the named reroute pair. The declaration names a value and passes it through; a usage reads that value anywhere on the same page with no wire drawn. Usages reference the declaration by node id, so renaming it renames them and deleting it leaves them reporting a missing declaration. Double-clicking a declaration drops a usage below it, a fresh usage binds to the page's only declaration, and its Details panel picks among the page's declarations otherwise. The compiler treats a usage as the declaration's input read from elsewhere, so no temporary is emitted and a reroute feeding its own declaration is the usual cycle error.
- Custom: `ShaderCustomExpressionNode`, a raw GLSL function body with declared typed inputs. It is the escape hatch for anything the node set lacks, and the one node that ties the graph to GLSL: it marks the compile result `glslOnly` and draws a warning glyph on the node.

### Material functions

A material graph can hold functions, each a page of its own (`MaterialFunctionPage`, `Graphs/MaterialFunctionPage.h`) shown in the Functions panel above Variables. A function has a name that is a C identifier unique in the graph, typed inputs with defaults, and one typed output. The Functions row renames in place (a second click on the current page's row or F2), and the page refuses a name that is not a fresh identifier, so a refused edit leaves the old name. Its page carries one `ShaderFunctionInputNode` per declared input (created and deleted with the declaration, undeletable while it exists) and one undeletable `ShaderFunctionOutputNode` whose input pin is the return value. Defining a function registers a create menu entry under Functions for its `ShaderFunctionCallNode`: one dynamic input pin per declared input, defaulting to the declared default, and a `result` pin of the output type. Call nodes follow the page's edits (rename, retype, added or removed inputs keep same-named pins and their links) and are deleted with the page. A function may call other functions; a function that reaches itself is a compile error.

The compiler emits a function once per stage the first time a call is compiled in that stage: the page's body compiles in its own scope (own temporaries and node memo, the input nodes resolving to the parameter names), then the writer declares `<outputType> <name>(<params>)` with the body's statements and a return, and every call site becomes a call expression. The generated GLSL therefore keeps the shape of a hand-written shader with helpers, which is what `sensorGrain` looks like with its `hash` and `noise` functions. Uniforms and varyings a function touches still declare at stage level. Automation reaches pages through `nodegraph list pages`, `nodegraph page`, `nodegraph createpage MaterialFunctionPage`, and `nodegraph deletepage` ([automation.md](./automation.md)); the function's inputs and output type are edited in the Details panel.

The function's source is the embedded page. A shared function asset, a page saved to its own file and referenced by several materials, would be a second source behind the same call node and the same compile path, and is the follow-up recorded in plan.md.

Several node classes serve many create menu entries through variant factories: one node class, several `NodeFactory` instances registered under `<ClassName>:<variant>` keys (`NodeFactory::getFactoryKey`), each allocating the node with its variant set so the factory's default object carries the right title. The graph keeps a second map by class name so loading and saving, which know only the class, resolve to the first factory of that class, and the node config persists the variant. `nodegraph createnode` accepts either spelling.

---

## Compilation

`MaterialCompiler::compile(graph, writer)` (`MaterialCompiler/MaterialCompiler.h`) walks the graph from the output node's pins and returns a `MaterialCompileResult`: vertex and fragment source, the `MikanShaderConfig` for the material file, the errors as `NodeEvaluationError` values so the editor's existing error overlay draws them, the `glslOnly` flag, and the parameter defaults. It touches no GL resource and no window, which is what lets it run in `MikanCmd`.

The walk is a pull: `MaterialCompileContext::input(pin)` follows the link to the upstream node, compiles that node once per stage (memoized by node id and stage, with a visiting set that reports a cycle), and hands back a `ShaderValue`, an expression string plus its type. Nodes coerce inputs (`coerce` broadcasts a `float` into a vector and rejects anything else), emit temporaries (`emitTemp`), declare uniforms (`uniform`, deduplicated by name across both stages and rejected when a name recurs with another type), and set their outputs. The color subtree compiles in the fragment stage and the position offset subtree in the vertex stage. A vertex attribute requested from the fragment stage becomes a varying, `v` plus the attribute name without its `a` prefix, declared in both stages and assigned in the vertex main. Every preset attribute is declared in the vertex stage up front, in location order, regardless of use. The shape domain always declares `mvpMatrix` with the `modelViewProjectionMatrix` semantic. After the compile the resolved types are written back onto the pins.

Nodes never spell shader text. Every expression and declaration goes through `IShaderWriter` (`MaterialCompiler/IShaderWriter.h`), the language seam: type names, literals, vector construction, swizzles, operators, builtin calls named by an enum (`mix`, `saturate`, `atan2` and so on, spelled per language), texture sampling, and the stage declarations and statements. `GlslShaderWriter` is the one writer today and targets `#version 330 core`, the version every hand-written shader in the tree uses. A second shader language is a second writer plus nothing else, except that custom expression nodes are GLSL by construction.

`MaterialCompiler::writeOutputs` writes `<stem>.vert`, `<stem>.frag`, and `<stem>.mat` beside the `.matgraph` file, so a compiled material has the same folder layout as a hand-written one. The `.matgraph` is the source of truth and the three generated files are checked in with it, the way the client bindings are: `MikanCmd -runTests` recompiles every bundled `*.matgraph` and fails when the checked-in outputs differ (the `material_compiler` module, `src/Editor/NodeEditors/Test/MaterialCompilerTests.cpp`). Regenerate with:

```
MikanCmd.exe -compileMaterial=resources/compositor_materials/rgbaFrame/rgbaFrame.matgraph
```

Every bundled material under `resources/compositor_materials` and `resources/shape_materials` is authored this way, and each carries its own generated vertex shader. Hand-written materials are still supported; the loader does not care which way a `.mat` came to be.

---

## The material editor

`MaterialNodeEditorWindow` (`src/Editor/NodeEditors/Windows/`) is a `NodeEditorWindow` bound to no component. It compiles the graph after every committed edit (the history checkpoint hook, so the error overlay follows undo and redo too) and writes the outputs on save or on the Material menu's Compile (Ctrl+B). A first save opens the file dialog in the project's folder for the graph's domain (`compositor_materials/` or `shape_materials/`), and a graph whose domain no longer matches the folder it was saved in saves as a new material there rather than writing the other domain's file beside it. With nothing selected, the Details panel shows the graph's domain and vertex preset.

The Preview panel (`MaterialPreviewPanel`) renders the last good compile into an owned framebuffer and shows it with `ImGui::Image`: the layer quad for `P2T`, a unit quad for `PT`, and `resources/models/shapes/sphere.obj` for `PNT`, under a slowly orbiting camera. Uniforms are fed through a `BindUniformCallback`: the matrices and screen size from the preview itself, parameters from their node defaults (a float parameter named `time` from elapsed seconds), texture parameters from their default texture asset or the missing-texture checker. The preview compiles its program directly rather than through the shader cache, so a driver compile failure surfaces its info log (`IMkShader::getCompileLog`) in the panel instead of only in the log file.

The create menu of every node editor groups factories by `NodeFactory::editorGetCategory` into submenus and has a filter box at the top; graphs whose factories declare no category keep the flat list.

---

## Reaching a material from the other editors

- Double-clicking a material tile in the project Assets panel or in the compositor or shape editor's Graph Assets panel, a `MaterialNode` on its canvas, or a consumer node (`DrawLayerNode`, `ApplyMaterialNode`, `DrawShapeMeshNode`) with a material wired into its material pin, opens the material's source graph in the material editor when the material file names one (`GraphMaterialProperty::editorOpenSourceGraph` over `MaterialAssetReference::editorOpen`), raising the window when one is already open. A hand-authored material file has no source graph, so the double-click only logs that.
- A material is authored from the project's Assets panel: the Compositor Materials and Shape Materials folders carry a New button that opens the material editor on a fresh graph of that domain, and each save rescans the catalog so the material shows in every Assets panel. The compositor and shape editors' Graph Assets panels are the project catalog filtered to materials and textures, the material editor's to textures; a tile dropped on a canvas is found-or-added in that graph's own reference list ([scripting.md](./scripting.md)).
- Saving a material graph reloads the material in every window's `MikanShaderCache` (`reloadMaterialByPath`): the cache recompiles the program under the same name and swaps it into the `MkMaterial` it already handed out, reapplies the `uniformDefaults`, then fires `OnMaterialReloaded`. `GraphMaterialProperty` listens and raises the graph's property-modified delegate, and `DrawLayerNode`, `ApplyMaterialNode`, and `DrawShapeMeshNode` rebuild their dynamic uniform pins from the new program, keeping links whose pin names survive. A compositor graph therefore follows a material edit without being reopened.
- The consuming nodes honor the material's defaults: a newly created float pin starts at the material's default value, and a texture pin whose uniform has a default texture is optional, so a skybox or lookup texture fixed by the material needs no wiring in the consumer. Wiring the pin overrides the default as before.

The division of labor is deliberate. The material graph describes the shader and its typed inputs with defaults; the compositor and shape graphs bind runtime sources (the camera, client textures, texture sources) and per-use values to those inputs. A material file can name a texture, but it cannot name the camera or a client's buffer, so the binding graphs stay the only place those live, and one material stays shareable across shapes that feed it different sources.

Automation covers the same surface headlessly: `nodegraph open material`, `nodegraph compile`, and the `material` namespace ([automation.md](./automation.md)).
