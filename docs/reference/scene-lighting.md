# Scene Lighting Estimation

How Mikan recovers the real scene's lighting from a captured video frame so Unreal can light a composited CG character with it. Covers the math, the ML models, the measured behavior of the estimator, and the limits that are structural rather than fixable.

Build and dependency setup for the ONNX runtime this uses is in [build.md](./build.md).

---

## Why this exists

To composite a CG character into a captured plate convincingly, the character must be lit by the real scene's light and must cast shadow onto it.

The shadow half is already solved elsewhere: the Unreal plugin renders a white catcher twice, with and without shadow casters (`EMikanCaptureKind::Shadow` / `ShadowReference`), and divides A by B to isolate the shadow factor. That is differential rendering, and it needs no changes here.

What was missing is the **incident lighting**. Note that inverse-rendering G-buffers do not provide it: an irradiance or diffuse-shading channel is `E(x) = ∫L(x,ω)(n·ω)dω`, already integrated against the *visible surface's* normal at the *visible surface's* position. A character standing elsewhere with different normals cannot index into it.

---

## The method

Diffuse irradiance is severely band-limited. Ramamoorthi and Hanrahan showed 9 order-2 spherical harmonic coefficients capture ~99% of it for a Lambertian surface. Since a per-pixel normal *and* a per-pixel diffuse shading are both available, the lighting is recoverable by least squares:

```
E(n) ≈ Σ_{l≤2} Â_l · L_lm · Y_lm(n)        Â = [π, 2π/3, π/4]
```

27 unknowns (9 coefficients × RGB) against hundreds of thousands of pixels. Solved as 9×9 normal equations per channel. No new dependency: OpenCV's `cv::solve` with `DECOMP_CHOLESKY` covers it.

The recovered coefficients are the **radiance** environment (the Â factors live in the design matrix), so they can be evaluated directly as an environment map.

Normals come out of the model in **camera space** and must be rotated into the capturing camera's stage space using the tracked camera pose at capture time. This is what makes the resulting environment correctly oriented relative to the Unreal scene, and it is the reason this belongs inside Mikan rather than as a standalone tool.

---

## Models

Two checkpoints, run in-process through ONNX Runtime with the DirectML execution provider.

| Checkpoint | Role |
|---|---|
| `prs-eth/marigold-iid-lighting-v1-1` | albedo, **diffuse shading**, non-diffuse residual |
| `Ruicheng/moge-2-vitl-normal` (ONNX export) | camera-space surface normals + metric depth/points |

Normals originally came from `prs-eth/marigold-normals-v1-1` (a second 3.4GB diffusion UNet and denoise loop). After the swap was measured to preserve the recovered lighting within Marigold's own seed-to-seed spread (see "The directly-predicted-normals experiment" below), MoGe-2's directly-predicted normal head replaced it: one 331M-parameter feed-forward pass that also supplies the metric geometry for the depth proxy mesh (see [depth-proxy-mesh.md](./depth-proxy-mesh.md)). `MarigoldInference` retains the normals path behind `Config::bEnableNormals` but the estimator disables it; `unet_normals.onnx` is neither loaded nor required on disk.

Facts verified by inspecting the downloaded Marigold checkpoints, all of which shape the export:

- **The VAE is byte-identical between the two checkpoints** (same sha256). One encoder and one decoder ONNX serves both.

- **The text encoder is also byte-identical**, and Marigold conditions on a fixed empty prompt. Bake the empty embedding as a constant and no text encoder is needed at runtime at all.

- IID UNet is `in_channels=16, out_channels=12`: the three targets are stacked in the latent channel dimension, 4 channels each, decoded separately. Normals UNet is `in=8, out=4`.

- `prediction_space` is **`linear`** for all three IID targets. Do **not** apply an sRGB decode before the fit; it is a radiometric solve.

- `shading` is flagged `up_to_scale: true`. The decomposition has an inherent global scale ambiguity, which is why the component carries a manually calibrated exposure scalar.

- `default_denoising_steps` is **4**, not 1. DDIM, `v_prediction`, 1000 train timesteps, `cross_attention_dim=1024`, `sample_size=96` (768px processing resolution).

---

## Measured behavior

Measurements from `tools/sh_lighting_fit.py` over two plates: a diffusely-lit office interior (`Evermotion_CreativeLoft`) and a hard-directional still life (`AdobeStock_GradientShadow`).

### The solver itself is exact

Validated against synthetic data with known ground truth, which is what the `unit_test_suite_cpp` round-trip test guards:

| Case | Result |
|---|---|
| Full normal sphere | max coefficient error **5.8e-15** |
| Hemisphere only (what a camera sees) | 3.6e-10, cond 599 |
| **Real scene normals**, synthetic shading | 1.1e-09, cond(AᵀA) = **693** |
| Real normals + 10% noise | direction error **0.1°** |
| Real normals + random occlusion | direction error **0.1°** |

Real-scene normal diversity is ample and the problem is well conditioned. Any bad result is a model violation, not a numerical one.

### Low-order SH rings, and it produces negative light

This is the defect most likely to be silently reintroduced. An unconstrained 9-coefficient fit on the office plate produced:

- radiance **negative over 30.7% of the sphere**
- negative lobe (−0.244) **larger in magnitude than the positive peak** (+0.190)
- band energies `l0=0.25, l1=0.31, l2=0.56`: most energy in the quadratic band

Negative radiance is physically impossible and the l=2 band was absorbing model error, not signal. A band-weighted ridge penalty on l=2 (`band_ridge`, default 0.1) fixes it on this plate:

| Fit | negative solid angle | R² |
|---|---|---|
| plain least squares | 30.7% | 0.144 |
| band ridge λ=0.01 | 18.9% | 0.138 |
| **band ridge λ=0.1** | **0%** | 0.119 |
| l0+l1 only (4 coefficients) | 0% | 0.109 |

Dropping from 9 coefficients to 4 costs almost nothing (0.144 → 0.109) and removes all the negativity, strong evidence the l=2 band was fitting error. λ=0.1 is the shipped default.

### But ringing is inherent, and the ridge does not remove it

The table above is easy to over-read. The ridge removed the negativity *there* because the l=2 band was over-fit to model error and dominated l=0. It does **not** fix ringing in general:

| Synthetic case | negative solid angle, λ=0 | λ=0.1 |
|---|---|---|
| clamped cosine (a directional light) | 49.0% | **47.3%** |
| narrow spot (higher frequency still) | 54.4% | 53.3% |
| directional occlusion | 14.3% | 12.5% |

Order-2 SH simply cannot represent a sharp light, because a clamped cosine has energy in every band, so a genuinely directional scene stays negative over roughly half the sphere no matter how the ridge is tuned. The C++ unit test reproduces this independently at 47.4%.

Two consequences:

- **Clamp radiance at evaluation.** `SHLightingEnvironment::evalRadianceClamped` exists for this; anything feeding an environment map or Unreal must use it. Unreal cannot emit negative light in any case.

- **Ringing does not ruin the direction estimate.** In the same clamped-cosine case the recovered dominant direction is within **0.08°** of truth. The negative lobes are an artifact of the representation, not evidence that the recovered lighting is wrong.

So the ridge's guaranteed property is that it shrinks the l=2 band, which is what the unit test asserts. Whether that also drives negativity to zero is data dependent.

### R² is low, and is the wrong success metric

R² against per-pixel shading is 0.05 to 0.14 on real plates and stays low even when shadowed pixels are excluded. This is expected and is **not** the quantity that matters:

- An environment map has no visibility term, so it **cannot** reproduce a cast shadow: that is a transport effect from geometry. On the directional plate the fit reproduces the object shading well while the cast shadows are entirely absent, and the error map is dominated by exactly those shadow regions.

- In the real pipeline the cast shadow is Unreal's job via the shadow-catcher passes, not the SH's.

The estimator only needs the incident lighting to be right. Judge it by whether a character looks correctly lit, not by per-pixel reconstruction error.

### Directionality is recovered, and carries its own confidence signal

`l1/l0` (l=1 band energy over l=0) measures how directional the recovered environment is:

| Plate | `l1/l0` | Recovered key direction |
|---|---|---|
| Office interior (many ceiling lamps) | 0.23 | unstable |
| Hard directional still life | **1.13** | `[-0.93, 0.20, 0.31]`, from the left, matching visible shadows |

The flat result on the office is *correct*: that scene really is close to uniform ambient. But when `l1/l0` is low the direction estimate is not meaningful, and it was observed flipping `+Z` to `−Z` under a change of regularization. **Surface `l1/l0` in the UI as a confidence indicator** so a near-ambient estimate is not mistaken for a confident directional one.

### The estimate is seeded, and the seed moves the answer

The denoise loop starts from random latents, so the seed changes the result. Measured across three seeds on the office plate (`MikanCmd -estimateLighting -seed=...`):

| Seed | `l1/l0` | key direction | ambient (R) |
|---|---|---|---|
| 1234 | 0.307 | `[-0.341, 0.097, -0.935]` | 0.2459 |
| 777 | 0.247 | `[-0.242, 0.210, -0.947]` | 0.2351 |
| 20260819 | 0.229 | `[-0.154, 0.320, -0.935]` | 0.2290 |

Pairwise direction spread is 8.1 to 16.7 degrees; ambient varies about 7%. The seed is therefore fixed by default so a capture is reproducible. Note this also explains why the C++ and Python paths differ by ~9° on this plate despite agreeing on ambient to four decimals. They use different RNGs, and 9° falls inside the seed spread. It is not an implementation discrepancy.

Two of the three seeds fall below the `l1/l0 < 0.25` warning threshold, which is the correct behavior: this scene really is near-ambient and the direction really is not well determined.

### Depth-derived normals are not a substitute for predicted normals

Marigold's normals UNet is 3.4GB, half the model footprint and one of the two denoise loops, while Depth Anything V2 Small is 24.8M parameters (~99MB fp32, ~50MB fp16) and a single forward pass. Since the SH fit is a least squares over ~400k samples and tolerates 10% per-pixel normal noise at 0.1° of recovered direction error, the geometric half looked over-specified. It was tested (`tools/compare_normal_sources.py`, same shading in every row, only the normals differ).

**It does not hold up where it matters.**

| Plate | Marigold `l1/l0` | depth-normal direction error | depth-normal `l1/l0` | coeff error |
|---|---|---|---|---|
| Office interior (near-ambient) | 0.226 | 16.8 to 31.0 degrees | 0.24 to 0.32 | 13 to 25% |
| Still life (directional) | **1.127** | **4.4 to 40.1 degrees** | **0.48 to 0.73** | 36 to 63% |

Three findings, in order of how much they matter:

1. **Directionality is systematically underestimated by roughly 40 to 50%** on the directional plate (0.48 to 0.73 recovered against 1.127). That corrupts the confidence signal specifically, the one number that tells an operator whether to trust the direction at all. A genuinely directional scene can be pushed down toward the near-ambient band and dismissed.
2. **Direction error swings 4.4 to 40 degrees with parameters that cannot be determined from one image.** The 4.4° best case is only locatable because Marigold was available as ground truth. In production there is nothing to tune against.
3. **The degradation is worst on near-field subjects.** The office (a large room, mostly distant) barely responded to the assumed FOV or depth range; the close-up still life swung wildly. Compositing a character into a set is a near-field problem.

The cause is structural rather than a quality gap. Monocular depth is affine-invariant, and unprojecting `d' = a·d + b` gives `P' = a·P + b·ray`, where the `b·ray` term warps the surface non-rigidly rather than translating it, so **normals genuinely depend on the unresolved scale and shift**. A directly-predicted normal has no such ambiguity. On the near-ambient plate both methods agree on the ambient level to within 8% and both correctly report low confidence, so the swap looks fine there. But that is the case where the answer barely matters.

Note also that depth-derived normals produce a *higher* R² on the directional plate (0.162 vs Marigold's 0.047) while being geometrically worse: flatter normals fit smooth shading more closely. One more reason R² is not the success metric here.

Two traps found while deriving the normals, both of which produced plausible-looking output:

- **Cross product order.** Image `v` runs down while camera Y runs up, so the `(u,v)` tangent pair is left-handed in camera space and `cross(T_u, T_v)` points away from the viewer. Use `cross(T_v, T_u)`.

- **Do not reorient normals toward the camera.** Forcing Z positive reads as a harmless safety net and is not: negating a normal mirrors X and Y too, turning a correct grazing-angle normal into a wrong one. It corrupted 39.5% of pixels and inverted the floor, and it masks a wrong cross-product order rather than correcting it. Grazing surfaces legitimately have slightly negative Z: 16.4% of Marigold's own output does. Together these two put the median normal error at 78°; fixing both brought it to 41°.

**Conclusion: keep the predicted-normals model.** The axis that matters is not "small vs large" but "derived-from-depth vs predicted": a small model that outputs normals *directly* sidesteps the affine ambiguity entirely, and that is the experiment worth running. It was, with MoGe-2, and the next section is the result.

### The directly-predicted-normals experiment was run, and it holds up

`tools/moge2_proxy_eval.py` re-ran the swap above with MoGe-2 ViT-L (`Ruicheng/moge-2-vitl-normal`, 331M parameters, single forward pass, MIT code and weights) whose normal head predicts normals directly rather than deriving them from depth. Same shading in every row, only the normals differ:

| Plate | vs Marigold normals | `l1/l0` (Marigold → MoGe-2) | SH direction apart |
|---|---|---|---|
| Still life (directional) | median **2.5°**, p90 7.8° | 1.127 → **1.061** | **5.8°** |
| Office (near-ambient) | median 10.4° | 0.226 → 0.245 | 31.3° |

Contrast with the depth-derived attempt: 41° median normal error, directionality crushed 40 to 50%, direction swinging 4.4 to 40 degrees with parameters untunable from one image. The direct head preserves directionality within 6% on the plate where it matters, lands the key direction inside Marigold's own seed-to-seed spread (8 to 17 degrees), and matches ambient within 7%. The office's 31° direction gap is in the regime already established as meaningless. Both fits sit at the `l1/l0 ≈ 0.25` warning threshold on a plate whose split-half disagreement is 21 to 44 degrees, and both correctly classify it as near-ambient. Confidence classification agrees on both plates.

Two further measurements from the same run:

- **The normal head is decoupled from the scale ambiguity.** Sweeping the FOV conditioning moved metric depth by up to 36% and the predicted normals by exactly 0.0°: the property depth-derived normals structurally lack.

- MoGe-2 also delivers the geometry the depth proxy mesh needs, so one model replaced the 3.4GB normals UNet *and* now provides the proxy mesh ([depth-proxy-mesh.md](./depth-proxy-mesh.md)). The IID lighting decomposition keeps Marigold, because diffuse shading has no comparable feed-forward replacement.

### The single-environment assumption is the real limit

A single probe assumes spatially-invariant lighting. Measured disagreement in recovered direction between image halves on the office plate:

- left/right split: **21.6°**
- top/bottom split: **44.4°**

Top/bottom failing worse is the signature of genuine vertical light variation (ceiling vs floor). This is why `LightEnvironmentComponent` derives from `TransformComponent`: it has a world position, so multiple probes remain possible without a wire-format change.

---

## ONNX export

`tools/export_marigold_onnx.py` runs once on a developer machine and writes to `models/marigold`:

| Artifact | Contents |
|---|---|
| `vae_encoder.onnx` (+`.data`) | image `[1,3,H,W]` in [-1,1] → latent mode `[1,4,H/8,W/8]`, **unscaled** |
| `vae_decoder.onnx` (+`.data`) | latent `[1,4,h,w]` → image in [-1,1] |
| `unet_iid_lighting.onnx` (+`.data`) | `sample[1,16,h,w]`, `timestep`, `ehs[1,2,1024]` → `[1,12,h,w]` |
| `unet_normals.onnx` (+`.data`) | `sample[1,8,h,w]`, ... -> `[1,4,h,w]` |
| `empty_text_embedding.bin` | float32 `[1,2,1024]` |
| `scheduler.json` | `alphas_cumprod` plus the DDIM config the C++ step needs |

Total ~6.8GB. `models/` and `*.onnx.data` are gitignored. Since the MoGe-2 swap the estimator no longer loads `unet_normals.onnx` (3.4GB), but the export script still produces it so the Marigold normals path remains reproducible for comparisons.

Three things that are easy to get wrong:

- **The empty text embedding is `[1,2,1024]`, not `[1,77,1024]`.** Marigold tokenizes the empty prompt with `padding="do_not_pad"`, so it is just `[BOS, EOS]`. No text encoder is needed at runtime; the embedding is baked as a constant.

- **The padded processing image must be divisible by 64** (latent divisible by 8), not 8 as Marigold's own preprocessing does. The UNet has three downsample stages; diffusers' PyTorch implementation threads an explicit `upsample_size` through to reconcile odd skip connections, but that is a Python-level shape computation that does not survive ONNX tracing. A latent of 60 fails at `up_blocks.1/Concat` with "mismatched dimensions of 15 and 16". Verified: latents 96×96, 56×96, 64×128 all run; 60×104 and 58×96 fail.

- **Export each model to its own scratch directory and consolidate its weights.** Any model over the 2GB protobuf limit forces `torch.onnx.export` into external-data mode, where it emits one loose file *per tensor*, named after the tensor and not namespaced per model. Two exports into one directory silently overwrite each other's weights. The export script re-saves each model with `all_tensors_to_one_file=True` into a single `.onnx.data` sidecar.

### Export validation

| Comparison | Result |
|---|---|
| VAE encode/decode ONNX vs PyTorch | max abs 4e-3 to 8e-3 at three resolutions |
| Both UNets ONNX vs PyTorch | **~1e-6 relative** |
| Full pipeline, identical fixed initial latents | correlation 0.9994 to 0.9998, mean abs ~5e-3 |
| **Recovered light direction, ONNX vs PyTorch** | **1.8°** apart; SH coefficients within 2.2% |

The 1.8° end-to-end agreement is what matters, and it sits far below the 20-to-45-degree scene-driven uncertainty measured above. Residual per-pixel differences come mostly from the preprocessing resize (`cv2.INTER_AREA` vs Marigold's antialiased bilinear), not from the graphs.

---

## Unreal side

`AMikanLightEnvironmentActor` in the MikanXR_UE plugin consumes the probe. It carries a Movable SkyLight in Real Time Capture mode, an inverted skydome the SkyLight captures, and an optional directional key light.

Why a skydome rather than a cubemap: `USkyLightComponent::Cubemap` wants a `UTextureCube` asset, which cannot be produced at runtime from 27 floats without an import step. Driving an unlit emissive dome and letting Real Time Capture re-read it means updating the lighting is nine vector parameter writes, with no assets and no render targets. Order-2 SH has no high-frequency detail to lose, so nothing is given up.

### The coordinate flip matters

Mikan is right-handed Y-up (OpenGL convention); Unreal is left-handed Z-up. The plugin's conversion is `FVector(v.x, v.z, v.y)`, a Y/Z swap, which is a **handedness flip**, not a rotation. An SH environment therefore *cannot* be moved between the two frames by rotating its coefficients.

The fix is to leave the coefficients in Mikan space and evaluate the basis at the swapped direction. That is why `UMikanLightEnvironmentData::GetSHCoefficients` deliberately returns Mikan-space values while `GetKeyLightDirection` returns Unreal-space ones. A direction is a vector and converts directly. A spherical function does not.

### Skydome material

Unlit, two-sided, with nine `Vector` parameters `SH0`..`SH8`. A Custom node taking the world-space direction as `Dir`:

```hlsl
// Mikan space is a Y/Z swap away from Unreal's, and that is a handedness flip,
// so evaluate the basis at the swapped direction rather than transforming the SH.
float3 n = float3(Dir.x, Dir.z, Dir.y);
float3 c = 0.282095f * SH0
         + 0.488603f * (n.y * SH1 + n.z * SH2 + n.x * SH3)
         + 1.092548f * (n.x * n.y * SH4)
         + 1.092548f * (n.y * n.z * SH5)
         + 0.315392f * (3.0f * n.z * n.z - 1.0f) * SH6
         + 1.092548f * (n.x * n.z * SH7)
         + 0.546274f * (n.x * n.x - n.y * n.y) * SH8;
return max(c, 0.0f);   // order-2 SH rings negative around sharp lights
```

Wire the result into Emissive Color. **The clamp is required**, not cosmetic. See the ringing section above.

**`Dir` must be the per-pixel view direction, not a parameter.** Wiring it to a Vector *parameter* is the easy mistake: the dome then evaluates the SH at one direction for every pixel and renders a flat constant color, the Sky Light captures a uniform environment, and all the directional information is silently thrown away. It still *looks* like it works, since the dome lights the scene, which is what makes it dangerous. Feed `Dir` from a **CameraVector** node multiplied by `-1` (`CameraVector` points from the pixel toward the camera, so negating gives the direction being looked along). `normalize(AbsoluteWorldPosition - ObjectPosition)` is equivalent while the dome is centered on the Sky Light.

`SkydomeMaterial` is intentionally *not* defaulted to a `/Game/` asset. The plugin is shared across projects, so a hard reference to project content would fail to resolve elsewhere. Assign it per level; the actor logs a warning at BeginPlay if it is unset, because an unassigned material produces a black dome that contributes no light and is otherwise indistinguishable from a bad estimate.

### The skydome material MUST have "Is Sky" enabled

This is the single most misleading failure in the whole feature, so it is worth stating plainly.

A Sky Light in Real Time Capture mode does **not** capture arbitrary scene meshes. It renders `MainView.SkyMeshBatches` through `FSkyPassMeshProcessor`, and that processor accepts a mesh only if its material returns `IsSky()` (`SkyPassRendering.cpp`, and the capture loop in `ReflectionEnvironmentRealTimeCapture.cpp`). The flag is `bIsSky` on `UMaterial`. In the editor it lives under **Details → Material → Advanced → Is Sky**, collapsed by default.

Without it the dome still renders normally in the main view, so the SH coefficients visibly drive the dome and everything looks correct, while the capture sees an empty sky and the Sky Light contributes **zero** lighting. Confirmed live: enabling `Is Sky` was the entire fix.

Two related gates, both handled in the actor:

- `UPrimitiveComponent::bVisibleInRealTimeSkyCaptures` must be true (engine default, set explicitly).
- The material must be Unlit and Opaque, which the documented constraint on `bIsSky` requires.

`AMikanLightEnvironmentActor::BeginPlay` logs a warning if the assigned material lacks `bIsSky`.

Diagnostic that isolates this quickly: switch the Sky Light to `SLS_Specified Cubemap` with any engine cubemap. If the scene lights up, the Sky Light is fine and the problem is on the capture-input side, meaning the dome is not being captured.

### Interaction with the shadow catcher

The Mikan capture components use `PRM_UseShowOnlyList`, so the skydome is not tagged and never renders into the Color pass, while still lighting the scene. The existing `Shadow` / `ShadowReference` passes then pick the environment up on the white catcher automatically, and the A/B divide carries the real scene's ambient. **Verify that Real Time Capture still sees the dome** despite those show-only lists; this is the one load-bearing assumption on the Unreal side.

### The key light is suppressed by default

`bCreateKeyLight` is off, and even when enabled the light is hidden while `directionality` is below `KeyLightDirectionalityThreshold` (0.25). A confidently-placed key light pointing the wrong way looks worse than no key light, and the direction genuinely is arbitrary in the near-ambient regime.

### Refreshing the vendored SDK

The plugin builds against a snapshot of the Mikan client SDK in `Plugins/MikanXR_UE/ThirdParty/MikanXR`. Adding a client API type means that snapshot must be refreshed or the module fails to link on the new statics (`MikanLightEnvironmentComponentValues::k_componentClassName`). The process is: build the MikanXR `INSTALL` target in **Release** to populate `dist/Win64`, then run `Plugins/MikanXR_UE/FetchMikan.bat`, which reads `MIKAN_DIST_PATH` from `SetMikanVars_x64.bat`.

---

## Capturing a probe in the editor

`AppStage_SceneLightingCapture` is the capture tool. It is reached from a `LightEnvironmentComponent` via the **Capture Scene Lighting** component function, which is registered through `IFunctionInterface` and is therefore also reachable from Lua and remotely over the client API.

Flow: pick a camera → live video while framing the shot → **Capture** runs both models → the panel shows the estimate's confidence numbers alongside a verification render of the recovered environment → **Apply** writes it into the probe.

Two deliberate choices:

- **The estimate is judged before it is committed.** The panel shows `l1/l0` prominently (in amber below 0.25) and renders the estimate over the plate. The failure mode this guards against is a confident-looking near-ambient estimate.

- **The camera is chosen explicitly** rather than inferred. The recovered environment is only meaningful in stage space if the frame's camera pose is known, so an untracked camera should fail loudly rather than silently produce a mis-oriented environment.

### Judging the estimate: reconstruct the scene, not a sphere

A lit sphere shows the environment in the abstract; it does not answer "does this explain the shot in front of me". `SceneLightingEstimator::renderReconstructionImage` answers that directly by evaluating the recovered environment at the **scene's own normals**, which is the same quantity `E(n)` the fit solved against `shading`. The panel offers four views over the plate:

| View | What it draws | Reads as |
|---|---|---|
| **Recovered lighting** | `E(n)` at the model's normals | the incident lighting the estimate claims |
| **Relit scene** | `albedo · E(n)` | the plate re-rendered under only the recovered light |
| **Model shading** | Marigold's `shading` | the target the fit solved against - the A/B partner |
| **Lit sphere** | the old camera-space sphere | the environment itself, at a glance |

Flipping between *Recovered lighting* and *Model shading* is the useful comparison: where they agree, the estimate explains the scene.

**Cast shadows will be missing, and that is structural.** An environment probe has no visibility term, so the reconstruction cannot reproduce a shadow. Confirmed on the still-life plate, where the object shading and its left-hand key direction come back cleanly while the hard window shadow and the plant shadows are entirely absent. That is the same fact the low R² measures (see "R² is low, and is the wrong success metric"); in the real pipeline those shadows are Unreal's job via the shadow-catcher passes. The panel says so on screen, because reading missing shadows as a bad estimate is the obvious trap here. Judge the direction and color of the shading instead.

All four views apply the same display transform (a 1/2.2 encode) so they stay comparable to each other and to the video frame; the model outputs and the solve are linear and are not affected. The reconstruction lives on the estimator rather than in the AppStage so that `MikanCmd -estimateLighting -dump=<dir>` writes exactly what the panel shows (`reconstruction_lighting.png`, `reconstruction_relit.png`, `reconstruction_target_shading.png`), which is also how the views were verified.

### Seeing a probe in the editor scene

`LightEnvironmentComponent::customRender` draws a sphere shaded with the probe's own environment, so a committed probe is visible in the viewport rather than only inside the capture tool. It is the editor twin of the Unreal skydome, and is **visualization only**: no collider, and it lights nothing.

The material is a new internal shader, `INTERNAL_MATERIAL_P_SH_ENVIRONMENT`, which evaluates order-2 SH radiance in the fragment shader from nine `vec3` uniforms (`shCoefficient0..8`, new `eUniformSemantic` entries deliberately mirroring the Unreal material's `SH0..SH8`). It uses the same basis constants as `sh_eval_basis`, and clamps at zero for the same reason every other consumer does: order-2 SH rings negative around sharp lights.

Two constraints worth knowing before changing it:

- **The mesh is position-only and carries the stage's rotation, not the probe's.** The shader treats the object-space position as the direction to evaluate along, which is what lets a bare unit sphere carry the environment with no normals or UVs. Object space therefore has to be the space the coefficients are in, so `customRender` builds the sphere transform from the probe's world position, uniform scale, and the owner stage's world rotation. The probe's own rotation is dropped: it would silently rotate the recovered environment with it, and a probe has no meaningful orientation. Unlike Unreal there is no Y/Z swap, because the direction is already in Mikan space.

- **The sphere is scene sized, not sky sized.** `k_environmentSphereRadius` is 10 metres, which reads as a probe ball from outside on a typical stage. It is an ordinary opaque mesh, so raising the radius far enough to enclose the scene is the one-line change that turns it into a skybox; back-face culling is already disabled so it reads correctly from inside as well as outside.

### The estimate runs on a worker thread

The ONNX sessions are held by the AppStage rather than the object system, because the models are several gigabytes and should not outlive the tool. They are owned specifically by a **worker thread** the stage starts on entry: `OnnxSession` requires a session to be created, run, and destroyed on one thread, and the estimate has to be off the UI thread for the progress readout and the Cancel button to work at all. The frame and the tracked camera pose are gathered on the UI thread and handed over as plain data; `update()` polls for the result.

**The progress bar is genuinely sub-step accurate for the part that matters.** Unlike a single opaque ONNX `Run`, the Marigold pipeline is a sequence of discrete pieces (one VAE encode, one denoise step per scheduler timestep, one decode per target), so `MarigoldInference::run` reports a unit as each lands (8 units with the normals UNet disabled). That step is also the one that dominates the wall clock, so it gets the widest band in the bar and moves smoothly through it. The model load, the MoGe-2 forward pass, and the SH fit can only jump, and an elapsed-seconds readout covers them.

**Cancel abandons an in-flight run**, not just the gaps between steps: `requestCancel` sets ONNX Runtime's terminate flag on every session (see `OnnxSession::requestTerminate`, the one method there that is safe to call cross-thread), and the denoise loop also checks the cancel flag between steps. A cancelled estimate fails like any other error, so the stage's own cancel flag is what distinguishes the two. Cancelling returns to framing with the models still loaded.

---

## Tools

Developer-only. Neither ships with Mikan nor is invoked at runtime.

- `tools/marigold_reference.py`: runs both Marigold pipelines on an image or video frame via PyTorch/diffusers, writing `normals.npy`, `albedo.npy`, `shading.npy`, `residual.npy`.

- `tools/sh_lighting_fit.py`: the reference implementation of the SH fit. Prints the fit statistics and writes a contact sheet, a lat-long environment map, and a lit sphere. **The C++ `SceneLightingEstimator` is validated against this.**

- `tools/export_marigold_onnx.py`: one-time ONNX export (see above).

- `tools/marigold_onnx_pipeline.py`: the full pipeline in ONNX Runtime + numpy only, no torch. **This is the executable specification for the C++ port**: preprocessing, VAE encode, the 4-step DDIM `v_prediction` loop, per-target decode, and postprocessing all map 1:1.

- `tools/validate_onnx_pipeline.py`: feeds both implementations the same fixed initial latents so the comparison is exact rather than statistical.

- `tools/depth_to_normals.py`: derives normals by unprojecting a monocular depth map, the approach measured and rejected above.

- `tools/compare_normal_sources.py`: runs the SH fit once per normal source with the shading held fixed, which is how any candidate normal model gets judged.

- The MoGe-2 ONNX export needs no local export step: the editor downloads the authors' published file on first use, and `MikanCmd.exe -fetchModels` does the same headlessly.

- `tools/moge2_onnx_pipeline.py`: MoGe-2 inference in ONNX Runtime + numpy only, no torch. **This is the executable specification for the C++ `MoGeInference`**: preprocessing, the forward pass, and the shift/scale recovery map 1:1. Validated against the PyTorch reference (depth within 0.04%, normals within 0.03°); the C++ port matches it per-vertex to 0.001% at p99.

- `tools/moge2_proxy_eval.py`: evaluates MoGe-2 as the geometry source for the depth-proxy work: silhouette alignment of depth edges, metric-depth sensitivity to FOV conditioning, and the SH fit re-run with MoGe-2's directly-predicted normals. Needs `moge` + `utils3d`, installed with `pip install --user --no-deps` from the microsoft/MoGe and EasternJournalist/utils3d repos so the pinned diffusers/torch environment is untouched. Writes to `build/moge2_eval/`, including oblique point-splat renders for eyeballing silhouette skirts.

These need `diffusers` pinned to **0.34.0**. Newer versions (0.39) fail to import under torch 2.4.1: they register a flash-attention custom op whose PEP 604 annotations torch 2.4's schema inference rejects.

---

## What the MoGe-2 evaluation established

The depth proxy mesh ([depth-proxy-mesh.md](./depth-proxy-mesh.md)) grew out of this work, and `tools/moge2_proxy_eval.py` measured the properties it depends on:

- depth discontinuities land at median 1.0px, p95 ~2px, from the nearest image edge
- cast shadows produce no spurious depth edges
- boundaries commit to near or far rather than averaging across them
- metric scale rides on the assumed FOV: a 45° guess against a ~56° estimate shifts depth 22-36%, which is why the capture stage feeds the calibrated FOV in
- the official ONNX export runs correctly under DirectML, matching CPU output exactly at ~7x the speed

Work deferred out of both features is tracked in [plan.md](../plan.md), not here.
