#pragma once

// Validates the HTTP message server's two route kinds over a real loopback socket:
//   - a background route answers on the connection thread, hops to the pumping
//     thread with runOnMainThread, and returns a binary body byte for byte
//   - a main-thread route still answers through the request queue
//   - dispose() releases a handler parked in runOnMainThread promptly, and the
//     hop reports failure
// Skips with a note when no port in its small fixed range can be bound.
bool run_http_server_tests();
