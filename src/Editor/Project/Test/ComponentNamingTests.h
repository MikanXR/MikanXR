#pragma once

// Covers the default component name and the app settings prefix table:
//   - "<prefix>_<id>" with a prefix, "<className>_<id>" without one
//   - every default table entry names a real component class
//   - AppSettingsConfig stores only departures from the defaults, keeps an
//     explicit empty prefix as a fallback choice, and round-trips through JSON
bool run_component_naming_tests();
