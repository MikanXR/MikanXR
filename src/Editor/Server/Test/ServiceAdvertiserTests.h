#pragma once

// Validates the network advertisement's pure helpers (ServiceAdvertiser):
//   - the TXT record carries the version and the ARKit base ports
//   - base ports are sorted and deduplicated
//   - the arkit key is present and empty with no ARKit source
//   - the instance label names the computer
bool run_service_advertiser_tests();
