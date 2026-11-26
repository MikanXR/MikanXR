#pragma once

#include "IRtpVideoSender.h"
#include "MkRendererFwd.h"
#include "ObjectSystemFwd.h"

#include <memory>
#include <string>
#include <vector>

class RtpFrameSender;
struct OutputFrame;

// One RTP stream of the composited frame: the encoder, where its packets go, and
// the rules for keeping the two current. A caller hands it every frame with the
// configuration it wants, and the stream opens, rebuilds, retargets or closes its
// frame sender to match. An RTP output component and the client return feed are
// each one of these.
class RtpOutputStream
{
public:
	struct Config
	{
		// Every receiver of the one encode. Empty closes the stream.
		std::vector<RtpDestination> destinations;
		// 0 keeps the composited frame's own size
		int height= 0;
		int bitrateKbps= 0;

		// Whether an open sender has to be rebuilt to take this configuration. The
		// destinations are not part of it: a sender retargets without a rebuild.
		bool needsRebuildFrom(const Config& other) const
		{
			return height != other.height || bitrateKbps != other.bitrateKbps;
		}
	};

	RtpOutputStream(class IMkGraphicsContext* graphicsContext, NetworkVideoSourceSystemPtr networkVideoSystem);
	~RtpOutputStream();

	// Brings the sender in line with the configuration, then sends the frame
	void writeFrame(const OutputFrame& frame, const Config& config);
	// Advances the frame sender's readback and statistics
	void update(float deltaSeconds);
	// Closes the sender and forgets a failure, so the next frame starts clean
	void close();

	bool getIsStreaming() const;
	// What the sender reports, or why there is none
	std::string getStatusText() const;
	// The encoded picture size and frame rate of the last frame handed in. False
	// before the first frame.
	bool getStreamFormat(int& outWidth, int& outHeight, float& outFps) const;

private:
	bool start(const OutputFrame& frame, const Config& config);

	class IMkGraphicsContext* m_graphicsContext= nullptr;
	NetworkVideoSourceSystemWeakPtr m_networkVideoSystem;
	std::shared_ptr<RtpFrameSender> m_frameSender;
	Config m_openedConfig;
	float m_openedFps= 0.f;
	int m_lastSourceWidth= 0;
	int m_lastSourceHeight= 0;
	float m_lastFps= 0.f;
	int m_lastRequestedHeight= 0;
};
using RtpOutputStreamPtr= std::shared_ptr<RtpOutputStream>;
