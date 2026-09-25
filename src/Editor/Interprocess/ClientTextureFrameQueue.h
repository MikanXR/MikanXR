#pragma once

#include "MikanAPITypes.h"
#include "MkRendererFwd.h"

#include <stdint.h>

struct ClientTextureFrameEntry
{
	IMkTexturePtr colorTexture;
	IMkTexturePtr depthTexture;
	IMkTexturePtr shadowTexture;
	int64_t frameIndex= -1;
};

class ClientTextureFrameQueue
{
public:
	explicit ClientTextureFrameQueue(int queueSize);
	~ClientTextureFrameQueue();

	bool initialize(const MikanRenderTargetDescriptor& desc);
	void dispose();

	// Current slot being written to — assign to SharedTextureReadAccessor
	IMkTexturePtr getPendingWriteColorTexture() const;
	IMkTexturePtr getPendingWriteDepthTexture() const;
	IMkTexturePtr getPendingWriteShadowTexture() const;

	// Called from onClientRenderTargetUpdated: stamps frameIndex on current slot,
	// advances pointer. Caller must then re-assign getPendingWrite*() to the accessor.
	void advanceWriteIndex(int64_t frameIndex);

	// Lookup by frame index. Returns newest if frameIndex == -1 or not found.
	IMkTexturePtr getColorTexture(int64_t frameIndex= -1) const;
	IMkTexturePtr getDepthTexture(int64_t frameIndex= -1) const;
	IMkTexturePtr getShadowTexture(int64_t frameIndex= -1) const;

	// Lookups for a specific frame that had to settle for the newest slot. Counted
	// on the color lookup only, since the depth and shadow lookups for the same
	// frame miss identically.
	inline uint64_t getFrameIndexMissCount() const { return m_frameIndexMissCount; }

private:
	void noteFrameIndexMiss(int64_t requestedFrameIndex) const;

	ClientTextureFrameEntry* m_entries= nullptr;
	int m_queueSize= 0;
	int m_lastWriteIndex= -1;
	int m_pendingWriteIndex= 0;
	mutable uint64_t m_frameIndexMissCount= 0;
};
