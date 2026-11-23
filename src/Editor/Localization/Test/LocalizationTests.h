#pragma once

// Validates the bundled localization tables without a GL context:
//   - load-time validation warnings (key parity in both directions, printf
//     specifier mismatches, embedded ##, _meta problems) are hard failures
//   - windows.* English titles are the ImGui window IDs, so they must be
//     non-empty and unique
//   - an unknown key passes through as the key itself, never a sentinel
//   - every displayed codepoint sits inside the glyph ranges the font atlas
//     actually bakes
//   - the committed text copy of those ranges, which the Python localization
//     check reads, matches them
bool run_localization_unit_tests();
