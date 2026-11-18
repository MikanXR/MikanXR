# AI Token Usage

<!-- AI_USAGE_BADGES:BEGIN -->
![AI tokens](https://img.shields.io/badge/AI_tokens-14M_out_%2F_5.5B_read-blueviolet) ![API-equiv. cost](https://img.shields.io/badge/API--equiv._cost-~$3.7k-green)
(cost is the API-equivalent at list prices, see [TOKEN_STATS.md](TOKEN_STATS.md))
<!-- AI_USAGE_BADGES:END -->

MikanXR is developed almost entirely in Claude Code sessions, across whichever Claude models were current at the time. This document records the API token usage of that work and what those tokens would cost at list prices. It also derives an order-of-magnitude estimate of the electricity and water the inference consumed.

## How the numbers are collected

Claude Code writes one JSONL transcript per session under `~/.claude/projects/`, plus a transcript per subagent, and every assistant record carries the exact token counts the API metered. Those transcripts are local to one machine and are pruned on a retention timer, so they are a collection source rather than a source of truth.

The source of truth is `docs/ai-usage.json`, a committed ledger holding per-session, per-model counters. `tools/token_stats.py` scans the transcripts, folds them into that ledger, and renders the badges above and the generated block below from the ledger alone.

```bash
python tools/token_stats.py
```

Three properties make that safe to run at any cadence.

- Counters only ever grow. A session is merged with `max(stored, scanned)` per class, so a pruned or partially rotated transcript cannot erase recorded history, and a resumed session grows correctly.

- Attribution is automatic. A session counts toward MikanXR when the working directory recorded in its transcript is this repository or one of its worktrees. Everything else lands in the ledger's `excluded` map, and the script prints what it excluded and why while it runs. That map is authoritative and hand-editable, which is how a session the rule wrongly claimed gets retired: add its id with `"reason": "manual"`, run `python tools/token_stats.py --render`, and its counters are deleted for good, since later scans never re-add an excluded id. Work on a sibling project that happened to start from this directory is excluded that way, so the same session is never counted in two repositories.

- Rendering is a pure function of the ledger. Running the script twice with no new sessions produces no diff, and `python tools/token_stats.py --check` re-renders from the ledger and fails if the published files have drifted. CI runs that check on every push, which is the only part of this that can run off the developer's machine.

The ledger stores no absolute paths. Session ids are opaque, and the script refuses to write any string that looks like a filesystem location, so the file carries no drive letter, home directory, or user name.

The token counts are exact, because they are what the API metered. The cost is those counts at list prices. Everything under "Energy and water methodology" is an estimate.

## Cost estimate

The cost column prices each model's counters at Anthropic's first-party list prices per token class: input, cache writes at each of the two cache lifetimes, cache reads, and output. The price table sits at the top of `tools/token_stats.py` with the date it was read, and a model missing from it stops the render, so a new model cannot go unpriced.

It is an API-equivalent figure, a way to read the token counts in human terms. It is not what the work was billed, since most of it ran under a subscription. Fast mode and batch pricing are ignored, and Claude Haiku 5.5 is priced at its rate for prompts up to 100,000 tokens because the ledger holds no per-request prompt size.

## Energy and water methodology

Anthropic publishes no per-token energy figures, so the estimate chains public reference points with stated assumptions. Treat the result as order-of-magnitude.

**Step 1: reduce the token classes to one.** The API meters classes that cost very different amounts of compute: fresh input, cache writes at each of the two cache lifetimes, cache reads, and output. They fold into "weighted output-equivalent tokens" using Anthropic's standard price ratios as the compute proxy, since pricing is the closest public signal of relative serving cost. The table is fixed and applies to every model. Some newer models discount cache reads further, which the cost above follows and this table deliberately does not, so the energy estimate does not move when pricing does.

| Token class | Standard price relative to base input | Weight in output-equivalent tokens |
|---|---|---|
| Output | 5 | 1 |
| Input | 1 | 1/5 |
| Cache write (1 hour) | 2 | 2/5 |
| Cache write (5 minute) | 1.25 | 1/4 |
| Cache read | 0.1 | 1/50 |

Note the consequence: cache reads dominate the weighted total even at 2 percent weight, because agentic coding re-reads the growing conversation context on every call. That is real work, attention over the cached context, and should not be dropped.

**Step 2: energy per weighted token.** Public anchors: Google reported the median Gemini Apps text prompt at 0.24 Wh (Aug 2025 technical report), Epoch AI estimated roughly 0.3 Wh for a typical GPT-4o query, and Sam Altman quoted 0.34 Wh for an average ChatGPT query. A Microsoft study of inference at production scale ([Oviedo et al.](https://arxiv.org/abs/2509.20241), published in Joule in 2026) puts the median query to a frontier-scale model at 0.31 Wh, with an interquartile range of 0.16 to 0.60 Wh. A median chat prompt is a few hundred output-equivalent tokens, which lands those figures near 1 Wh per 1000 weighted tokens. The models used here are larger than what serves a median consumer prompt, so the central assumption is 2 Wh per 1000 weighted tokens, with 0.5 and 6 as low and high bounds. These constants sit at the top of `tools/token_stats.py`. Correct them as better data appears.

**Step 3: water per kWh.** Onsite datacenter cooling runs about 1.1 L/kWh, consistent with Google's reported fleet WUE and their 0.26 mL figure for the 0.24 Wh prompt above. Including the water footprint of electricity generation raises it to roughly 3 L/kWh on a typical US grid mix.

**Where the estimate is weakest.** Three simplifications carry most of the uncertainty.

- Cache reads are about 70 percent of the weighted total, and their weight comes from a price ratio. Public measurement work has not yet established what a cached prefix costs in energy.

- One energy rate covers every model. Measurement work models energy per token as a function of model size and treats input and output tokens separately, as in [Greenpixie's AI token methodology](https://arxiv.org/abs/2609.33965). That paper does not publish the per-model figures a size-aware rate would need.

- Measured energy per token varies several-fold with hardware and serving configuration, none of which is known here.

**What the estimate excludes:** model training, which is generally estimated to be small per query relative to inference at scale but is not zero, datacenter construction, the dev machine running Claude Code and the local builds, and the GPU time spent on the depth, lighting, and tracking model work this project exists for. It also cannot know Anthropic's actual serving efficiency, batching, or datacenter locations, which is why the range spans an order of magnitude.

## Why not an off-the-shelf impact library

[EcoLogits](https://github.com/genai-impact/ecologits) was evaluated for this. It wraps live provider SDK calls and attaches an impact estimate to each response, which is a good fit for an application calling a model API directly. It does not fit here: the usage being measured comes from Claude Code transcripts rather than from calls this repository makes, there is no offline entry point that accepts recorded token counts, and it reports energy and greenhouse gas rather than water. The arithmetic above is small enough to state in full, which also makes its assumptions easy to check and to correct.

<!-- TOKEN_STATS:BEGIN -->
Generated by `tools/token_stats.py` from `docs/ai-usage.json`, covering 46 sessions through 2026-09-13.

## Per-session usage

| Session | Dates | Models | API calls | Output | Cache read |
|---|---|---|---|---|---|
| ARKit streaming to MikanXR plan | 2026-07-12 to 2026-07-30 | claude-opus-5, claude-sonnet-5 | 2,766 | 2,381,531 | 1,281,281,484 |
| AngelScript vs Lua for Mikan | 2026-07-17 | claude-sonnet-5 | 5 | 1,799 | 179,616 |
| Sensor grain effect for CG compositing | 2026-07-17 | claude-sonnet-5 | 89 | 94,566 | 9,492,484 |
| Efficient Claude integration for MikanXR | 2026-07-18 | claude-sonnet-5 | 18 | 10,089 | 909,205 |
| Lua script HTTP endpoint registration | 2026-07-18 | claude-sonnet-5 | 270 | 222,891 | 45,431,122 |
| EditorWindow boilerplate refactoring | 2026-07-18 | claude-sonnet-5 | 31 | 21,707 | 3,178,012 |
| MCP server integration for MikanXR | 2026-07-22 | claude-sonnet-5 | 44 | 47,681 | 2,531,945 |
| DySample depth upsampling integration | 2026-07-25 to 2026-07-26 | claude-opus-4-8 | 413 | 465,090 | 124,537,340 |
| GStreamer event bus polling in MikanGStreamerVideoDevice | 2026-07-26 | claude-opus-4-8 | 74 | 68,142 | 8,041,655 |
| SteamVR device disconnection crash | 2026-07-26 | claude-opus-4-8 | 17 | 17,023 | 1,003,214 |
| Stencil alignment with natural features | 2026-07-27 | claude-opus-4-8 | 172 | 188,751 | 25,466,083 |
| Orthographic viewport and distance ruler | 2026-07-27 | claude-opus-4-8 | 283 | 286,166 | 69,661,824 |
| Light channel flashing in Unreal | 2026-07-27 | claude-opus-4-8 | 4 | 2,721 | 139,217 |
| MikanXR plugin request timeout handling | 2026-07-28 | claude-opus-4-8 | 59 | 91,292 | 7,807,160 |
| Doctor command | 2026-07-29 | claude-opus-4-8 | 11 | 16,111 | 659,186 |
| Mikan Claude documentation structure | 2026-08-03 | claude-fable-5 | 234 | 203,137 | 22,075,805 |
| V-RGBX CUDA device index error | 2026-08-20 to 2026-08-22 | claude-opus-5 | 483 | 394,118 | 202,001,155 |
| Depth-derived proxy geometry | 2026-08-22 to 2026-08-23 | claude-fable-5, claude-opus-5 | 405 | 413,662 | 156,858,320 |
| Depth-derived proxy geometry | 2026-08-22 | claude-fable-5, claude-opus-5 | 280 | 260,280 | 61,559,385 |
| Claude.md and reference docs review | 2026-08-23 | claude-opus-5 | 186 | 114,065 | 40,476,672 |
| Mikan automation server framework | 2026-08-23 | claude-fable-5, claude-opus-5 | 521 | 416,068 | 139,625,509 |
| Rendering order and line depth issues | 2026-08-25 | claude-fable-5 | 185 | 201,264 | 53,748,099 |
| MikanXR UI styling from MikanTrack | 2026-08-25 to 2026-08-26 | claude-fable-5, claude-opus-5, claude-sonnet-5 | 1,461 | 1,002,817 | 405,110,458 |
| 3D mesh generation for AR occlusion | 2026-08-25 to 2026-08-30 | claude-fable-5, claude-opus-5 | 119 | 162,599 | 25,002,129 |
| CompositorNodeGraph undo/redo support | 2026-08-26 to 2026-08-27 | claude-fable-5, claude-opus-5 | 652 | 679,670 | 330,111,098 |
| Fix CefShutdown ordering in App::shutdown | 2026-08-27 | claude-fable-5 | 28 | 15,538 | 1,768,221 |
| Spout logging window management | 2026-08-27 | claude-opus-5 | 143 | 78,634 | 22,165,034 |
| Lua component binding architecture | 2026-08-28 | claude-opus-5 | 16 | 9,632 | 1,374,637 |
| iPhone ARKit merge conflicts | 2026-08-28 | claude-opus-5 | 83 | 44,199 | 8,694,207 |
| List agents | 2026-08-28 to 2026-08-30 | claude-opus-5 | 1,599 | 1,278,581 | 822,545,213 |
| Project Outliner tree view panel | 2026-09-01 to 2026-09-03 | claude-fable-5, claude-opus-5 | 591 | 522,130 | 271,776,496 |
| MikanGStreamer shutdown crash | 2026-09-03 | claude-opus-5 | 53 | 40,416 | 5,099,965 |
| Project-level scripting refactor | 2026-09-03 | claude-fable-5-1, claude-opus-5, claude-sonnet-5 | 271 | 337,847 | 59,139,649 |
| Lua procedural light generation | 2026-09-04 to 2026-09-07 | claude-fable-5-1, claude-opus-5, claude-sonnet-5 | 1,144 | 1,077,907 | 338,762,427 |
| MikanXR_UEClientSource compositor rendering | 2026-09-06 | claude-fable-5-1, claude-opus-5 | 59 | 124,228 | 10,330,520 |
| MikanXR stencil model data fetch optimization | 2026-09-07 | claude-opus-5 | 251 | 150,108 | 59,424,531 |
| MikanXR cross-platform rendering | 2026-09-07 | claude-opus-5 | 24 | 25,638 | 2,022,014 |
| CEF browser window interactivity | 2026-09-07 to 2026-09-08 | claude-opus-5 | 637 | 414,758 | 296,648,741 |
| MikanXR camera position persistence | 2026-09-08 | claude-opus-5 | 272 | 145,965 | 72,989,508 |
| Fix missing CEF localization labels | 2026-09-08 | claude-opus-5 | 49 | 26,890 | 3,617,494 |
| QuadShape depth sorting in compositor | 2026-09-08 | claude-fable-5-1 | 108 | 91,524 | 18,501,042 |
| Node graph editor for materials | 2026-09-08 to 2026-09-10 | claude-fable-5-1, claude-opus-5 | 647 | 984,719 | 197,204,436 |
| Project outliner layout changes | 2026-09-10 to 2026-09-12 | claude-fable-5-1, claude-opus-5 | 260 | 307,805 | 87,869,814 |
| Project asset management system | 2026-09-12 to 2026-09-13 | claude-fable-5-1, claude-opus-5, claude-sonnet-5 | 719 | 736,874 | 169,164,630 |
| MikanXR energy/water usage badging | 2026-09-13 | claude-opus-5 | 72 | 62,709 | 9,140,162 |
| Localization contribution workflow | 2026-09-13 | claude-opus-5 | 16 | 27,634 | 1,127,725 |

## Totals per model

| Model | API calls | Input | Cache write | Cache read | Output | Weighted | API-equiv. cost |
|---|---|---|---|---|---|---|---|
| claude-fable-5 | 2,282 | 10,513 | 8,304,398 | 687,609,210 | 2,301,629 | 19,176,684 | $959 |
| claude-fable-5-1 | 1,578 | 80,557 | 12,820,183 | 597,930,275 | 2,526,984 | 19,282,735 | $516 |
| claude-opus-4-8 | 1,033 | 15,661 | 4,592,961 | 237,315,679 | 1,135,296 | 7,529,115 | $188 |
| claude-opus-5 | 6,688 | 26,796 | 26,704,328 | 2,475,865,039 | 4,980,406 | 64,389,127 | $1,610 |
| claude-sonnet-5 | 4,243 | 41,715 | 22,382,772 | 1,477,534,440 | 3,322,661 | 41,172,602 | $412 |
| **total** | 15,824 | 175,242 | 74,804,642 | 5,476,254,643 | 14,266,976 | **151,550,265** | **$3,684** |

## Energy and water estimate

| Scenario | Wh per 1k weighted tokens | Energy | Water (onsite cooling) | Water (incl. generation) |
|---|---|---|---|---|
| low | 0.5 | 76 kWh | 83 L | 227 L |
| central | 2.0 | 303 kWh | 333 L | 909 L |
| high | 6.0 | 909 kWh | 1,000 L | 2,728 L |
<!-- TOKEN_STATS:END -->
