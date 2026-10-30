//-- includes -----
#include "Sam2Segmenter.h"
#include "Logger.h"
#include "OnnxSession.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>

//-- constants -----
namespace Sam2SegmenterInternal
{
const char* k_encoderFile= "encoder.onnx";
const char* k_decoderFile= "decoder.onnx";
// The predictor's own feature split: the image embedding with the no-memory embedding added,
// and the two high-resolution levels already reduced, so a decoder call copies little
const char* k_encoderOutputNames[]= {"image_embed", "high_res_feats_0", "high_res_feats_1"};
const char* k_decoderOutputNames[]= {"low_res_masks", "iou_scores"};
const int32_t k_positiveLabel= 1;
} // namespace Sam2SegmenterInternal

//-- implementation -----
struct Sam2Segmenter::Impl
{
	Config config;
	// Owned through pointers because a session is neither copyable nor movable
	std::unique_ptr<OnnxSession> encoder= std::make_unique<OnnxSession>();
	std::unique_ptr<OnnxSession> decoder= std::make_unique<OnnxSession>();
	int encoderOutputIndices[3]= {-1, -1, -1};
	int decoderOutputIndices[2]= {-1, -1};

	// The encoded frame: the encoder's outputs, which the decoder reads in place
	std::vector<Ort::Value> features;
	std::vector<std::vector<int64_t>> featureShapes;
	cv::Size frameSize;
	bool bHasFrame= false;

	// Reused per prompt
	std::vector<float> imageNchw;
	float pointCoords[k_maxPoints * 2]= {};
	int32_t pointLabels[k_maxPoints]= {};

	static bool resolveOutputs(const OnnxSession& session, const char* const* names, size_t count, int* outIndices,
							   const char* what)
	{
		for (size_t n= 0; n < count; ++n)
		{
			outIndices[n]= -1;
			for (size_t i= 0; i < session.getOutputCount(); ++i)
			{
				if (session.getOutputName(i) == names[n])
				{
					outIndices[n]= (int)i;
					break;
				}
			}
			if (outIndices[n] < 0)
			{
				MIKAN_LOG_ERROR("Sam2Segmenter::startup") << what << " is missing output '" << names[n] << "'";
				return false;
			}
		}
		return true;
	}
};

Sam2Segmenter::Sam2Segmenter()
	: m_impl(std::make_unique<Impl>())
{
}

Sam2Segmenter::~Sam2Segmenter() { shutdown(); }

bool Sam2Segmenter::startup(const Config& config)
{
	shutdown();
	m_impl->config= config;

	const std::filesystem::path directory(config.modelDirectory);
	const std::filesystem::path encoderPath= directory / Sam2SegmenterInternal::k_encoderFile;
	const std::filesystem::path decoderPath= directory / Sam2SegmenterInternal::k_decoderFile;
	for (const std::filesystem::path& path : {encoderPath, decoderPath})
	{
		if (!std::filesystem::exists(path))
		{
			MIKAN_LOG_ERROR("Sam2Segmenter::startup") << "Missing model file: " << path.string();
			return false;
		}
	}

	const std::string preferredEp= config.preferGpu ? "directml" : "cpu";
	if (!m_impl->encoder->create(encoderPath.string(), preferredEp))
	{
		MIKAN_LOG_ERROR("Sam2Segmenter::startup") << "Failed to load " << encoderPath.string();
		return false;
	}
	if (!m_impl->decoder->create(decoderPath.string(), preferredEp))
	{
		MIKAN_LOG_ERROR("Sam2Segmenter::startup") << "Failed to load " << decoderPath.string();
		return false;
	}
	if (!Impl::resolveOutputs(*m_impl->encoder, Sam2SegmenterInternal::k_encoderOutputNames, 3,
							  m_impl->encoderOutputIndices, "encoder.onnx")
		|| !Impl::resolveOutputs(*m_impl->decoder, Sam2SegmenterInternal::k_decoderOutputNames, 2,
								 m_impl->decoderOutputIndices, "decoder.onnx"))
	{
		return false;
	}
	if (m_impl->decoder->getInputCount() != 5)
	{
		MIKAN_LOG_ERROR("Sam2Segmenter::startup")
			<< "decoder.onnx has " << m_impl->decoder->getInputCount() << " inputs, expected 5";
		return false;
	}

	MIKAN_LOG_INFO("Sam2Segmenter::startup")
		<< "SAM2 encoder on " << m_impl->encoder->activeEp() << ", decoder on " << m_impl->decoder->activeEp();
	m_bIsInitialized= true;
	return true;
}

void Sam2Segmenter::shutdown()
{
	m_impl->features.clear();
	m_impl->featureShapes.clear();
	m_impl->bHasFrame= false;
	m_impl->encoder= std::make_unique<OnnxSession>();
	m_impl->decoder= std::make_unique<OnnxSession>();
	m_bIsInitialized= false;
}

const char* Sam2Segmenter::getActiveExecutionProvider() const { return m_impl->encoder->activeEp(); }

bool Sam2Segmenter::setFrame(const cv::Mat& bgrFrame)
{
	m_impl->bHasFrame= false;
	if (!m_bIsInitialized || bgrFrame.empty() || bgrFrame.type() != CV_8UC3)
		return false;

	// The frame resized to the encoder's square input, RGB in [0,1], NCHW
	cv::Mat resized;
	const int interpolation=
		(bgrFrame.cols > k_inputSize || bgrFrame.rows > k_inputSize) ? cv::INTER_AREA : cv::INTER_LINEAR;
	cv::resize(bgrFrame, resized, cv::Size(k_inputSize, k_inputSize), 0.0, 0.0, interpolation);
	cv::Mat rgb;
	cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
	rgb.convertTo(rgb, CV_32FC3, 1.0 / 255.0);
	const size_t planeSize= (size_t)k_inputSize * k_inputSize;
	m_impl->imageNchw.resize(3 * planeSize);
	std::vector<cv::Mat> planes= {
		cv::Mat(k_inputSize, k_inputSize, CV_32FC1, m_impl->imageNchw.data()),
		cv::Mat(k_inputSize, k_inputSize, CV_32FC1, m_impl->imageNchw.data() + planeSize),
		cv::Mat(k_inputSize, k_inputSize, CV_32FC1, m_impl->imageNchw.data() + 2 * planeSize),
	};
	cv::split(rgb, planes);

	const int64_t shape[4]= {1, 3, k_inputSize, k_inputSize};
	Ort::Value input= Ort::Value::CreateTensor<float>(OnnxSession::getCpuMemoryInfo(), m_impl->imageNchw.data(),
													  m_impl->imageNchw.size(), shape, 4);
	m_impl->features= m_impl->encoder->runOutputs(&input, 1, m_impl->encoderOutputIndices, 3);
	if (m_impl->features.size() != 3)
		return false;

	m_impl->featureShapes.clear();
	for (const Ort::Value& feature : m_impl->features)
	{
		m_impl->featureShapes.push_back(feature.GetTensorTypeAndShapeInfo().GetShape());
	}
	m_impl->frameSize= bgrFrame.size();
	m_impl->bHasFrame= true;
	return true;
}

bool Sam2Segmenter::predict(const Prompt& prompt, const cv::Size& promptSize, const cv::Size& outputSize,
							std::vector<cv::Mat>& outMasks, std::vector<float>& outScores)
{
	outMasks.clear();
	outScores.clear();
	if (!m_impl->bHasFrame || prompt.positives.empty() || promptSize.width <= 0 || promptSize.height <= 0)
		return false;

	// Points in the encoder's input space, exactly as many as the prompt has: the decoder's point
	// axis is dynamic because a padding point is not neutral, it joins the decoder's attention and
	// moves the masks
	const float scaleX= (float)k_inputSize / (float)promptSize.width;
	const float scaleY= (float)k_inputSize / (float)promptSize.height;
	const int count= std::min((int)prompt.positives.size(), k_maxPoints);
	for (int i= 0; i < count; ++i)
	{
		m_impl->pointCoords[i * 2]= prompt.positives[i].x * scaleX;
		m_impl->pointCoords[i * 2 + 1]= prompt.positives[i].y * scaleY;
		m_impl->pointLabels[i]= Sam2SegmenterInternal::k_positiveLabel;
	}

	// The encoder's outputs are handed to the decoder in place
	std::vector<Ort::Value> inputs;
	for (size_t i= 0; i < 3; ++i)
	{
		const std::vector<int64_t>& shape= m_impl->featureShapes[i];
		size_t elementCount= 1;
		for (int64_t dim : shape)
			elementCount*= (size_t)dim;
		inputs.push_back(Ort::Value::CreateTensor<float>(OnnxSession::getCpuMemoryInfo(),
														 m_impl->features[i].GetTensorMutableData<float>(),
														 elementCount, shape.data(), shape.size()));
	}
	const int64_t coordShape[3]= {1, count, 2};
	const int64_t labelShape[2]= {1, count};
	inputs.push_back(Ort::Value::CreateTensor<float>(OnnxSession::getCpuMemoryInfo(), m_impl->pointCoords,
													 (size_t)count * 2, coordShape, 3));
	inputs.push_back(Ort::Value::CreateTensor<int32_t>(OnnxSession::getCpuMemoryInfo(), m_impl->pointLabels,
													   (size_t)count, labelShape, 2));

	std::vector<Ort::Value> outputs=
		m_impl->decoder->runOutputs(inputs.data(), inputs.size(), m_impl->decoderOutputIndices, 2);
	if (outputs.size() != 2)
		return false;

	const std::vector<int64_t> maskShape= outputs[0].GetTensorTypeAndShapeInfo().GetShape();
	if (maskShape.size() != 4 || maskShape[1] != k_candidateCount)
	{
		MIKAN_LOG_ERROR("Sam2Segmenter::predict") << "Unexpected mask output rank " << maskShape.size();
		return false;
	}
	const int lowRows= (int)maskShape[2];
	const int lowCols= (int)maskShape[3];
	const float* logits= outputs[0].GetTensorData<float>();
	const float* scores= outputs[1].GetTensorData<float>();

	// Logits upsampled to the output size and thresholded at zero, as the
	// predictor thresholds its own upsampled masks
	for (int c= 0; c < k_candidateCount; ++c)
	{
		const cv::Mat lowRes(lowRows, lowCols, CV_32FC1, const_cast<float*>(logits + (size_t)c * lowRows * lowCols));
		cv::Mat upsampled;
		cv::resize(lowRes, upsampled, outputSize, 0.0, 0.0, cv::INTER_LINEAR);
		cv::Mat mask;
		cv::compare(upsampled, 0.f, mask, cv::CMP_GT);
		outMasks.push_back(mask);
		outScores.push_back(scores[c]);
	}
	return true;
}

void Sam2Segmenter::requestCancel()
{
	m_impl->encoder->requestTerminate();
	m_impl->decoder->requestTerminate();
}
