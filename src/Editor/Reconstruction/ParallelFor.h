#pragma once

#include <functional>

// Split [0, count) into contiguous chunks and run body(begin, end) on each
// across the hardware threads, returning once every chunk is done. Chunks are
// at least minChunk long so small ranges run on the calling thread. The body
// must write only to disjoint data per chunk; nothing here synchronizes.
void parallelFor(int count, const std::function<void(int begin, int end)>& body, int minChunk= 1);
