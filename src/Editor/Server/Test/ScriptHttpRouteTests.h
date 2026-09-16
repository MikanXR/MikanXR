#pragma once

// Validates the ScriptHttpRouteTable behind the project-level HTTP Triggers
// panel:
//   - isValidRoute accepts a plain segmented path and rejects an empty,
//     slash-bounded, whitespace-containing, or doubled-slash route
//   - addRoute/setRoute/removeRoute keep the table free of duplicate routes
//     and reject an out-of-range index
//   - the table round-trips through writeToJSON / readFromJSON
//   - the single-line JSON text form round-trips and rejects malformed text
//     without touching the table
//   - readFromJSON skips a malformed or duplicate entry and keeps the rest
bool run_script_http_route_tests();
