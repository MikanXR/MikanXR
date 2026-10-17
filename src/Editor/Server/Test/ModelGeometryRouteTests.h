#pragma once

// Drives the model geometry route through a real loopback server with a stub resolver: GET
// answers the GLB with its etag, a matching If-None-Match answers 304, an unknown id or path
// answers 404, another verb 405, a preflight 204, and the editor timing out 504.
bool run_model_geometry_route_tests();
