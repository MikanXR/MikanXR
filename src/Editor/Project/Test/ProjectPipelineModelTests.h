#pragma once

// Covers the pipeline panel's status rules on hand-built snapshots:
//   - every box reads not configured when a required link is absent and
//     inactive when an optional one is
//   - each box's own rule over its facts, faulted one fact at a time from a
//     healthy snapshot
//   - roll-up to the parent only, carrying the source node, never to a sibling
//   - the no-current-scene snapshot
//   - node lookups, click, follow (including the duplicate-component tie rule)
//     and selection validation
bool run_project_pipeline_model_tests();
