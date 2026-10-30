#pragma once

//-- includes -----
#include <opencv2/core.hpp>

#include <memory>
#include <string>
#include <vector>

//-- types -----
/// Runs SAM 2.1's image encoder and prompt decoder in-process through ONNX
/// Runtime, as exported by MikanXR/MikanSam2Onnx: encoder.onnx takes the frame
/// resized to 1024x1024 as RGB in [0,1] with the normalization inside and
/// returns the three backbone feature levels; decoder.onnx takes those with a
/// dynamic number of prompt points and returns three candidate masks as
/// 256x256 logits with the model's own score for each. The point axis is
/// dynamic on purpose: a padding point is not neutral to the decoder.
///
/// A frame is encoded once and prompted any number of times, which is how the
/// occlusion matte pays the encoder per frame and the decoder per contour run.
class Sam2Segmenter
{
public:
	struct Config
	{
		/// Directory holding encoder.onnx and decoder.onnx
		std::string modelDirectory;

		/// Try DirectML first, falling back to CPU
		bool preferGpu= true;
	};

	/// Points inside the object to segment, in pixels of the image the prompt
	/// is sized to (see predict), all positive
	struct Prompt
	{
		std::vector<cv::Point2f> positives;
	};

	static constexpr int k_inputSize= 1024;
	static constexpr int k_lowResSize= 256;
	static constexpr int k_maxPoints= 16;
	static constexpr int k_candidateCount= 3;

	Sam2Segmenter();
	~Sam2Segmenter();

	bool startup(const Config& config);
	void shutdown();

	bool getIsInitialized() const { return m_bIsInitialized; }

	/// "DirectML" or "CPU" of the encoder session, which is the one that costs
	const char* getActiveExecutionProvider() const;

	/// Encodes a CV_8UC3 BGR frame of any size. The frame's size is remembered
	/// so a later prompt can be given in another resolution.
	bool setFrame(const cv::Mat& bgrFrame);

	/// Decodes one prompt against the encoded frame. The prompt's points are in
	/// pixels of a promptSize image of the same framing as the frame; the
	/// candidate masks come back as CV_8UC1 (255 inside) at outputSize, with
	/// the model's score for each. Points past k_maxPoints are dropped.
	bool predict(const Prompt& prompt, const cv::Size& promptSize, const cv::Size& outputSize,
				 std::vector<cv::Mat>& outMasks, std::vector<float>& outScores);

	/// Asks an in-flight run to give up; it then returns false. Safe to call
	/// from another thread, which nothing else here is.
	void requestCancel();

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
	bool m_bIsInitialized= false;
};
