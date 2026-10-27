#pragma once

#include "IRtpVideoSender.h"
#include "MkRendererFwd.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// The compositor's network sink. Each composited frame is scaled into this object's
// own framebuffer, streamed back through that framebuffer's pixel buffer objects, and
// handed to an IRtpVideoSender from the GStreamer plugin. Every call runs on the main
// thread with the GL context current.
class CompositorRtpOutput
{
public:
	struct Settings
	{
		// Every receiver of the one encode
		std::vector<RtpDestination> destinations;
		// 0 keeps the composited frame's own size
		int height= 0;
		int bitrateKbps= 0;
		float fps= 30.f;
	};

	CompositorRtpOutput(class IMkGraphicsContext* graphicsContext);
	~CompositorRtpOutput();

	// Opens the sender for a composited frame of the given size. The output owns the sender from here on.
	bool start(IRtpVideoSenderPtr sender, const Settings& settings, int sourceWidth, int sourceHeight);
	void stop();
	bool getIsStreaming() const;

	// Replaces the receivers while streaming without restarting the encoder, and is
	// remembered for a reopen. Returns false when not streaming or a destination is refused.
	bool setDestinations(const std::vector<RtpDestination>& destinations);
	const std::vector<RtpDestination>& getDestinations() const { return m_settings.destinations; }

	// Set once the sender refused its settings or failed mid-stream. The owner then
	// leaves the output alone until the settings change.
	bool getHasFailed() const { return m_bHasFailed; }

	// Records why no sender could be created yet, for the status text
	void setUnavailable(const std::string& reason);

	void writeFrame(IMkTexturePtr compositedTexture);
	void update(float deltaSeconds);

	const std::string& getStatusText() const { return m_statusText; }

	// Width follows the source aspect, both dimensions even, floor 160x64
	static void computeOutputSize(int sourceWidth, int sourceHeight, int requestedHeight, int& outWidth,
								  int& outHeight);

	// The persisted destinations followed by the client feeds, each host and port once,
	// in first-seen order
	static std::vector<RtpDestination> mergeDestinations(const std::vector<RtpDestination>& persisted,
														 const std::vector<RtpDestination>& clientFeeds);

private:
	bool openSender(int sourceWidth, int sourceHeight);
	bool ensureFrameBuffer(int width, int height);
	void refreshStatusText();

	class IMkGraphicsContext* m_graphicsContext= nullptr;
	IRtpVideoSenderPtr m_sender;
	Settings m_settings;
	int m_sourceWidth= 0;
	int m_sourceHeight= 0;
	int m_outputWidth= 0;
	int m_outputHeight= 0;
	bool m_bHasFailed= false;
	bool m_bSenderWasOpen= false;

	IMkFrameBufferPtr m_frameBuffer;
	IMkTriangulatedMeshPtr m_quadMesh;
	std::vector<uint8_t> m_readbackBuffer;

	// Frames pushed over the last whole second
	float m_fpsWindowSeconds= 0.f;
	uint64_t m_fpsWindowStartPushed= 0;
	float m_measuredFps= 0.f;

	std::string m_unavailableReason;
	std::string m_statusText;
};
using CompositorRtpOutputPtr= std::shared_ptr<CompositorRtpOutput>;
