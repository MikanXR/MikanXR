#include "ParallelFor.h"

#include <algorithm>
#include <cstdint>
#include <thread>
#include <vector>

void parallelFor(int count, const std::function<void(int begin, int end)>& body, int minChunk)
{
	if (count <= 0)
		return;

	const int hardwareThreads= std::max(1, (int)std::thread::hardware_concurrency());
	const int threadCount= std::min(hardwareThreads, count / std::max(minChunk, 1));
	if (threadCount <= 1)
	{
		body(0, count);
		return;
	}

	// Contiguous chunks, the last one run on the calling thread
	auto chunkBegin= [count, threadCount](int chunk) { return (int)(((int64_t)count * chunk) / threadCount); };

	std::vector<std::thread> workers;
	workers.reserve(threadCount - 1);
	for (int chunk= 0; chunk < threadCount - 1; ++chunk)
	{
		const int begin= chunkBegin(chunk);
		const int end= chunkBegin(chunk + 1);
		workers.emplace_back([&body, begin, end]() { body(begin, end); });
	}

	body(chunkBegin(threadCount - 1), count);

	for (std::thread& worker : workers)
	{
		worker.join();
	}
}
